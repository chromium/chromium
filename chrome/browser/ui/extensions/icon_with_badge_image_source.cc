// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/icon_with_badge_image_source.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "extensions/browser/extension_action.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

namespace {

constexpr gfx::Size kDefaultIconAreaSize(28, 28);

gfx::ImageSkiaRep ScaleImageSkiaRep(const gfx::ImageSkiaRep& rep,
                                    int target_width_dp,
                                    float target_scale) {
  int width_px = target_width_dp * target_scale;
  return gfx::ImageSkiaRep(
      skia::ImageOperations::Resize(rep.GetBitmap(),
                                    skia::ImageOperations::RESIZE_BEST,
                                    width_px, width_px),
      target_scale);
}

// Make sure the background color is opaque. See http://crbug.com/619499
SkColor GetBadgeBackgroundColor(IconWithBadgeImageSource::Badge* badge,
                                const ui::ColorProvider* color_provider) {
  return SkColorGetA(badge->background_color) == SK_AlphaTRANSPARENT
             ? color_provider->GetColor(
                   kColorExtensionIconBadgeBackgroundDefault)
             : SkColorSetA(badge->background_color, SK_AlphaOPAQUE);
}

float GetBlockedActionBadgeRadius() {
  return 12.0f;
}

}  // namespace

IconWithBadgeImageSource::Badge::Badge(const std::string& text,
                                       SkColor text_color,
                                       SkColor background_color)
    : text(text), text_color(text_color), background_color(background_color) {}

IconWithBadgeImageSource::Badge::~Badge() {}

IconWithBadgeImageSource::IconWithBadgeImageSource(
    const gfx::Size& size,
    GetColorProviderCallback get_color_provider_callback)
    : gfx::CanvasImageSource(size),
      get_color_provider_callback_(std::move(get_color_provider_callback)) {
  DCHECK(get_color_provider_callback_);
}

IconWithBadgeImageSource::~IconWithBadgeImageSource() = default;

void IconWithBadgeImageSource::SetIcon(const gfx::Image& icon) {
  icon_ = icon;
}

void IconWithBadgeImageSource::SetBadge(std::unique_ptr<Badge> badge) {
  badge_ = std::move(badge);

  if (!badge_ || badge_->text.empty())
    return;

  // Generate the badge's render text. Make sure it contrasts with the badge
  // background if it is transparent (also occurs when text color has not yet
  // been set).
  SkColor text_color =
      SkColorGetA(badge_->text_color) == SK_AlphaTRANSPARENT
          ? color_utils::GetColorWithMaxContrast(GetBadgeBackgroundColor(
                badge_.get(), get_color_provider_callback_.Run()))
          : badge_->text_color;

  constexpr int badge_height = 14;
  gfx::FontList base_font = views::TypographyProvider::Get().GetFont(
      views::style::CONTEXT_BADGE, views::style::STYLE_SECONDARY);
  std::u16string utf16_text = base::UTF8ToUTF16(badge_->text);

  constexpr int kMaxTextWidth = 23;
  const int text_width = std::min(
      kMaxTextWidth, gfx::Canvas::GetStringWidth(utf16_text, base_font));
  // Calculate badge size. It is clamped to a min width just because it looks
  // silly if it is too skinny.
  constexpr int kPadding = 2;
  int badge_width = text_width + kPadding * 2;

  const gfx::Rect icon_area = GetIconAreaRect();

  // Force the pixel width of badge to be either odd (if the icon width is odd)
  // or even otherwise. If there is a mismatch you get http://crbug.com/26400.
  if (icon_area.width() != 0 && (badge_width % 2 != icon_area.width() % 2))
    badge_width += 1;
  badge_width = std::max(badge_height, badge_width);

  // The minimum width for center-aligning the badge.
  constexpr int kCenterAlignThreshold = 20;
  // Calculate the badge background rect. It is usually right-aligned, but it
  // can also be center-aligned if it is large.
  const int badge_offset_x = badge_width >= kCenterAlignThreshold
                                 ? (icon_area.width() - badge_width) / 2
                                 : icon_area.width() - badge_width;
  const int badge_offset_y = icon_area.height() - badge_height;
  badge_background_rect_ =
      gfx::Rect(icon_area.x() + badge_offset_x, icon_area.y() + badge_offset_y,
                badge_width, badge_height);
  gfx::Rect badge_rect = badge_background_rect_;

  const int top_inset = (badge_height - base_font.GetHeight()) / 2;
  const int bottom_inset = (badge_height - base_font.GetHeight()) - top_inset;
  const int left_inset = (badge_rect.width() - text_width) / 2;
  const int right_inset = (badge_rect.width() - text_width) - left_inset;
  badge_rect.Inset(
      gfx::Insets::TLBR(top_inset, left_inset, bottom_inset, right_inset));

  badge_text_ = gfx::RenderText::CreateRenderText();
  badge_text_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  badge_text_->SetCursorEnabled(false);
  badge_text_->SetFontList(base_font);
  badge_text_->SetColor(text_color);
  badge_text_->SetText(std::move(utf16_text));
  badge_text_->SetDisplayRect(badge_rect);
}

void IconWithBadgeImageSource::Draw(gfx::Canvas* canvas) {
  // TODO(crbug.com/40576276): There should be a cleaner delineation
  // between what is drawn here and what is handled by the button itself.

  if (icon_.IsEmpty())
    return;

  if (paint_blocked_actions_decoration_)
    PaintBlockedActionDecoration(canvas);

  gfx::ImageSkia skia = icon_.AsImageSkia();
  gfx::ImageSkiaRep rep = skia.GetRepresentation(canvas->image_scale());
  if (rep.scale() != canvas->image_scale()) {
    skia.AddRepresentation(
        ScaleImageSkiaRep(rep, extensions::ExtensionAction::ActionIconSize(),
                          canvas->image_scale()));
  }
  if (grayscale_)
    skia = gfx::ImageSkiaOperations::CreateHSLShiftedImage(skia, {-1, 0, 0.6});

  int x_offset = std::floor(
      (size().width() - extensions::ExtensionAction::ActionIconSize()) / 2.0);
  int y_offset = std::floor(
      (size().height() - extensions::ExtensionAction::ActionIconSize()) / 2.0);
  canvas->DrawImageInt(skia, x_offset, y_offset);

  // Draw a badge on the provided browser action icon's canvas.
  PaintBadge(canvas);
}

// Paints badge with specified parameters to |canvas|.
void IconWithBadgeImageSource::PaintBadge(gfx::Canvas* canvas) {
  if (!badge_text_)
    return;

  SkColor background_color =
      GetBadgeBackgroundColor(badge_.get(), get_color_provider_callback_.Run());
  cc::PaintFlags rect_flags;
  rect_flags.setStyle(cc::PaintFlags::kFill_Style);
  rect_flags.setAntiAlias(true);
  rect_flags.setColor(background_color);

  // Clear part of the background icon.
  gfx::Rect cutout_rect(badge_background_rect_);
  cutout_rect.Inset(-1);
  cc::PaintFlags cutout_flags = rect_flags;
  cutout_flags.setBlendMode(SkBlendMode::kClear);
  constexpr int kOuterCornerRadius = 3;
  const int corner_radius_for_badge_background_rect = kOuterCornerRadius + 1;
  canvas->DrawRoundRect(cutout_rect, kOuterCornerRadius, cutout_flags);

  // Paint the backdrop.
  canvas->DrawRoundRect(badge_background_rect_,
                        corner_radius_for_badge_background_rect, rect_flags);

  // Paint the text.
  badge_text_->Draw(canvas);
}

void IconWithBadgeImageSource::PaintBlockedActionDecoration(
    gfx::Canvas* canvas) {
  // TODO(elainechien): This looks like it's trying to match the GM2 elevation
  // +2 spec.  Move to ShadowValue::MakeShadowValues() and systematize.

  // To match the CSS notion of blur (spread outside the bounding box) to the
  // Skia notion of blur (spread outside and inside the bounding box), we have
  // to double the CSS-based blur values.
  constexpr int kBlurCorrection = 2;

  const ui::ColorProvider* color_provider = get_color_provider_callback_.Run();
  const gfx::ShadowValue key_shadow(
      gfx::Vector2d(0, 1), kBlurCorrection * 2 /*blur*/,
      color_provider->GetColor(kColorExtensionIconDecorationKeyShadow));

  const gfx::ShadowValue ambient_shadow(
      gfx::Vector2d(0, 2), kBlurCorrection * 6 /*blur*/,
      color_provider->GetColor(kColorExtensionIconDecorationAmbientShadow));

  const float blocked_action_badge_radius = GetBlockedActionBadgeRadius();

  // Sanity checking.
  const gfx::Rect icon_rect = GetIconAreaRect();
  DCHECK_LE(2 * blocked_action_badge_radius, icon_rect.width());
  DCHECK_EQ(icon_rect.width(), icon_rect.height());

  cc::PaintFlags paint_flags;
  paint_flags.setStyle(cc::PaintFlags::kFill_Style);
  paint_flags.setAntiAlias(true);
  paint_flags.setColor(
      color_provider->GetColor(kColorExtensionIconDecorationBackground));
  paint_flags.setLooper(
      gfx::CreateShadowDrawLooper({key_shadow, ambient_shadow}));

  canvas->DrawCircle(gfx::PointF(icon_rect.CenterPoint()),
                     blocked_action_badge_radius, paint_flags);
}

gfx::Rect IconWithBadgeImageSource::GetIconAreaRect() const {
  gfx::Rect icon_area(size());
  icon_area.ClampToCenteredSize(kDefaultIconAreaSize);
  return icon_area;
}
