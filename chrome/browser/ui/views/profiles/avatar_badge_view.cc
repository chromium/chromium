// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_badge_view.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "build/branding_buildflags.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "components/signin/public/base/signin_switches.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/internal/profiles/profile_view_avatar_decoration_specs_branded.h"
#else
#include "chrome/browser/ui/profiles/profile_view_avatar_decoration_specs.h"
#endif

#ifndef HAS_AVATAR_BADGE_SPECS
#error \
    "HAS_AVATAR_BADGE_SPECS must be defined by the avatar decoration specs header."
#endif

namespace {

// TODO(crbug.com/532163925): Revise and finalize the specs once the integrate
// both private and public specs. These deviate slightly from the mock (CSS,
// SVG) but produce a visual result closer to it.
constexpr int kBadgeHeight = 22;
constexpr int kTopPadding = 5;
constexpr int kBottomPadding = 5;
constexpr int kHorizontalPadding = 12;
constexpr int kBetweenChildSpacing = 6;

// Constructs the badge's background wave path using `kAvatarBadgeWaveOps` and
// `kAvatarBadgeWavePoints`.
// Performs bounds validation on each path operation.
// Returns std::nullopt if the spec is invalid or incomplete.
std::optional<SkPath> GetBackgroundPatternPath() {
  if constexpr (kAvatarBadgeWaveOps.empty()) {
    return std::nullopt;
  }

  SkPathBuilder builder;
  size_t point_index = 0;
  const size_t total_points = kAvatarBadgeWavePoints.size();

  for (AvatarBadgePathOp op : kAvatarBadgeWaveOps) {
    switch (op) {
      case AvatarBadgePathOp::kMoveTo:
        if (point_index + 1 > total_points) {
          DLOG(WARNING)
              << "AvatarBadge wave spec invalid: MoveTo requires 1 point";
          return std::nullopt;
        }
        builder.moveTo(kAvatarBadgeWavePoints[point_index][0],
                       kAvatarBadgeWavePoints[point_index][1]);
        point_index += 1;
        break;
      case AvatarBadgePathOp::kLineTo:
        if (point_index + 1 > total_points) {
          DLOG(WARNING)
              << "AvatarBadge wave spec invalid: LineTo requires 1 point";
          return std::nullopt;
        }
        builder.lineTo(kAvatarBadgeWavePoints[point_index][0],
                       kAvatarBadgeWavePoints[point_index][1]);
        point_index += 1;
        break;
      case AvatarBadgePathOp::kCubicTo:
        if (point_index + 3 > total_points) {
          DLOG(WARNING)
              << "AvatarBadge wave spec invalid: CubicTo requires 3 points";
          return std::nullopt;
        }
        builder.cubicTo(kAvatarBadgeWavePoints[point_index][0],
                        kAvatarBadgeWavePoints[point_index][1],
                        kAvatarBadgeWavePoints[point_index + 1][0],
                        kAvatarBadgeWavePoints[point_index + 1][1],
                        kAvatarBadgeWavePoints[point_index + 2][0],
                        kAvatarBadgeWavePoints[point_index + 2][1]);
        point_index += 3;
        break;
      case AvatarBadgePathOp::kClose:
        builder.close();
        break;
      default:
        DLOG(WARNING)
            << "AvatarBadge wave spec invalid: Unknown path operation";
        return std::nullopt;
    }
  }

  if (point_index != total_points) {
    DLOG(WARNING)
        << "AvatarBadge wave spec invalid: Unconsumed points remaining ("
        << point_index << " / " << total_points << ")";
    return std::nullopt;
  }

  return builder.detach();
}

// Returns true if the wave path spec is valid for badge rendering. Unbranded
// builds with empty wave specs are considered valid.
bool IsAvatarBadgeSpecValid(const std::optional<SkPath>& wave_path) {
  if constexpr (kAvatarBadgeWaveOps.empty()) {
    return true;
  }
  return wave_path.has_value();
}

// Draws an inner drop shadow along the interior boundary of `pill_path` on the
// canvas.
void DrawBadgeInnerShadow(gfx::Canvas* canvas,
                          int width,
                          int height,
                          const SkPath& pill_path,
                          const ui::ColorProvider& color_provider) {
  // Save the current canvas state (matrix and clip) to restore it later.
  canvas->Save();
  canvas->ClipPath(pill_path, true);

  SkRect outer_rect = SkRect::MakeWH(width, height);
  outer_rect.outset(kAvatarBadgeShadowInversePadding,
                    kAvatarBadgeShadowInversePadding);
  SkPath inverse_path = SkPathBuilder().addRect(outer_rect).detach();

  // Construct an inverted mask frame by subtracting `pill_path` from the
  // enlarged `outer_rect` (`inverse_path`). Drawing a shadow on this
  // inverted mask frame projects the shadow inward along the interior border of
  // `pill_path`.
  SkPath result_path;
  Op(inverse_path, pill_path, kDifference_SkPathOp, &result_path);

  // Draw the path.
  cc::PaintFlags shadow_flags;
  shadow_flags.setAntiAlias(true);
  shadow_flags.setColor(SK_ColorBLACK);
  shadow_flags.setImageFilter(sk_make_sp<cc::DropShadowPaintFilter>(
      kAvatarBadgeShadowDx, kAvatarBadgeShadowDy, kAvatarBadgeShadowBlur / 2.0f,
      kAvatarBadgeShadowBlur / 2.0f,
      SkColor4f::FromColor(color_provider.GetColor(kAvatarBadgeShadowColorId)),
      cc::DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr));
  canvas->DrawPath(result_path, shadow_flags);

  // Restore the saved canvas state, removing the clip path so we can draw the
  // on the rest of the canvas.
  canvas->Restore();
}

// Scales and paints the decorative wave pattern with linear gradients and blur
// filters clipped to `pill_path`.
void DrawBadgeBackground(gfx::Canvas* canvas,
                         int width,
                         int height,
                         const SkPath& pill_path,
                         const ui::ColorProvider& color_provider,
                         const std::optional<SkPath>& wave_path) {
  if (!wave_path.has_value()) {
    return;
  }
  // Save the current canvas state (matrix and clip) to restore it later.
  canvas->Save();
  canvas->ClipPath(pill_path, true);

  const float scale_x = width / kAvatarBadgeUnscaledWidth;
  const float scale_y = height / kAvatarBadgeUnscaledHeight;

  SkMatrix matrix;
  matrix.setScale(scale_x, scale_y);
  SkPath scaled_wave_path = wave_path->makeTransform(matrix);

  cc::PaintFlags wave_flags;
  wave_flags.setAntiAlias(true);
  wave_flags.setStyle(cc::PaintFlags::kFill_Style);

  float blur_radius = kAvatarBadgeWaveBlurFactor * scale_y;
  wave_flags.setImageFilter(sk_make_sp<cc::BlurPaintFilter>(
      blur_radius, blur_radius, SkTileMode::kDecal, nullptr));

  std::vector<SkColor4f> base_colors;
  base_colors.reserve(kAvatarBadgeBaseColorIds.size());
  for (ui::ColorId id : kAvatarBadgeBaseColorIds) {
    base_colors.push_back(SkColor4f::FromColor(color_provider.GetColor(id)));
  }
  std::vector<SkColor4f> overlay_colors;
  overlay_colors.reserve(kAvatarBadgeOverlayColorIds.size());
  for (ui::ColorId id : kAvatarBadgeOverlayColorIds) {
    overlay_colors.push_back(SkColor4f::FromColor(color_provider.GetColor(id)));
  }

  SkPoint points0[2] = {SkPoint::Make(kAvatarBadgeBaseStart[0] * scale_x,
                                      kAvatarBadgeBaseStart[1] * scale_y),
                        SkPoint::Make(kAvatarBadgeBaseEnd[0] * scale_x,
                                      kAvatarBadgeBaseEnd[1] * scale_y)};
  wave_flags.setShader(cc::PaintShader::MakeLinearGradient(
      points0, base_colors.data(), kAvatarBadgeBaseOffsets.data(),
      base_colors.size(), SkTileMode::kClamp));
  // Draw the path.
  canvas->DrawPath(scaled_wave_path, wave_flags);

  SkPoint points1[2] = {SkPoint::Make(kAvatarBadgeOverlayStart[0] * scale_x,
                                      kAvatarBadgeOverlayStart[1] * scale_y),
                        SkPoint::Make(kAvatarBadgeOverlayEnd[0] * scale_x,
                                      kAvatarBadgeOverlayEnd[1] * scale_y)};
  wave_flags.setShader(cc::PaintShader::MakeLinearGradient(
      points1, overlay_colors.data(), kAvatarBadgeOverlayOffsets.data(),
      overlay_colors.size(), SkTileMode::kClamp));
  // Draw the path.
  canvas->DrawPath(scaled_wave_path, wave_flags);

  // Restore the saved canvas state, removing the clip path so we can draw the
  // on the rest of the canvas.
  canvas->Restore();
}

// Paints the base pill background, inner shadow, and wave decoration onto
// `canvas`.
void PaintAvatarBadgeBackground(gfx::Canvas* canvas,
                                int width,
                                int height,
                                const ui::ColorProvider& color_provider,
                                const std::optional<SkPath>& wave_path) {
  float radius = height / 2.f;
  SkPath pill_path =
      SkPath::RRect(SkRect::MakeWH(width, height), radius, radius);

  cc::PaintFlags bg_flags;
  bg_flags.setAntiAlias(true);
  bg_flags.setStyle(cc::PaintFlags::kFill_Style);
  bg_flags.setColor(color_provider.GetColor(kAvatarBadgeBackgroundColorId));
  canvas->DrawPath(pill_path, bg_flags);

  if (wave_path.has_value()) {
    // Draw background wave layers first so that the inner drop shadow is
    // rendered on top rather than being obscured by opaque wave fills.
    DrawBadgeBackground(canvas, width, height, pill_path, color_provider,
                        wave_path);
    DrawBadgeInnerShadow(canvas, width, height, pill_path, color_provider);
  }
}

}  // namespace

// static
std::u16string AvatarBadgeView::GetAvatarBadgeLabel(int tier) {
  switch (tier) {
    case 1:
      return std::u16string(kAvatarBadgeLabelTier1);
    case 2:
      return std::u16string(kAvatarBadgeLabelTier2);
    case 3:
      return std::u16string(kAvatarBadgeLabelTier3);
    default:
      return std::u16string();
  }
}

AvatarBadgeView::AvatarBadgeView(const std::u16string& label_text,
                                 std::optional<SkPath> wave_path)
    : label_text_(label_text), wave_path_(std::move(wave_path)) {
  CHECK(!label_text.empty());
  CHECK(
      base::FeatureList::IsEnabled(switches::kEnableAiSubscriptionAvatarRing));

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::TLBR(kTopPadding, kHorizontalPadding, kBottomPadding,
                        kHorizontalPadding),
      kBetweenChildSpacing));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* label = AddChildView(std::make_unique<views::Label>());
  label->SetText(label_text_);
  label->SetFontList(views::TypographyProvider::Get().GetFont(
      views::style::CONTEXT_LABEL, views::style::STYLE_BODY_3_EMPHASIS));
  label->SetSubpixelRenderingEnabled(false);
}

AvatarBadgeView::~AvatarBadgeView() = default;

gfx::Size AvatarBadgeView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::FontList font_list = views::TypographyProvider::Get().GetFont(
      views::style::CONTEXT_LABEL, views::style::STYLE_BODY_3_EMPHASIS);

  // Width is dynamically flexible based on the actual label text + side
  // padding.
  int text_width = gfx::Canvas::GetStringWidth(label_text_, font_list);
  int preferred_width = text_width + (kHorizontalPadding * 2);

  // Height is target fixed height (22px), or font height + vertical padding.
  int font_height = font_list.GetHeight();
  int preferred_height =
      std::max(kBadgeHeight, font_height + kTopPadding + kBottomPadding);

  return gfx::Size(preferred_width, preferred_height);
}

void AvatarBadgeView::OnPaint(gfx::Canvas* canvas) {
  const auto* color_provider = GetColorProvider();
  CHECK(color_provider);
  PaintAvatarBadgeBackground(canvas, width(), height(), *color_provider,
                             wave_path_);
}

std::unique_ptr<views::View> GetAvatarBadgeView(const std::u16string& label) {
  if (label.empty() || !base::FeatureList::IsEnabled(
                           switches::kEnableAiSubscriptionAvatarRing)) {
    return nullptr;
  }
  std::optional<SkPath> wave_path = GetBackgroundPatternPath();
  if (!IsAvatarBadgeSpecValid(wave_path)) {
    DLOG(WARNING) << "Failed to create AvatarBadgeView: spec validation failed";
    return nullptr;
  }
  return std::make_unique<AvatarBadgeView>(label, std::move(wave_path));
}

BEGIN_METADATA(AvatarBadgeView)
END_METADATA
