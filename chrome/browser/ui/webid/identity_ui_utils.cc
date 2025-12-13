// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/identity_ui_utils.h"

#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/platform_locale_settings.h"
#include "content/public/browser/webid/identity_request_account.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "ui/gfx/monogram_utils.h"

namespace {

#if BUILDFLAG(IS_ANDROID)
// The desired size of the IDP icon used as badge for the user account avatar
// when there are multiple IDPs.
inline constexpr int kLargeAvatarBadgeSize = 20;
// The border radius of the background circle containing the IDP icon in an
// account button.
constexpr int kIdpBorderRadius = 12;
#else
// The desired size of the IDP icon used as badge for the user account avatar
// when there are multiple IDPs.
inline constexpr int kLargeAvatarBadgeSize = 16;
// The border radius of the background circle containing the IDP icon in an
// account button.
constexpr int kIdpBorderRadius = 10;
#endif  // BUILDFLAG(IS_ANDROID)

// Returns an image consisting of `base_image` with `badge_image` being badged
// towards its bottom right corner. `badge_offset` is used to determine how much
// bigger the badged image should be with respect to the base image. A
// transparent circular circle is cut out from the bottom right corner of the
// output image, of size `badge_radius`. The following are prerequisites for
// invoking this method:
// * `base_image` and `badge_image` need to be square images.
// * `badge_radius` needs to be at least half of the width of `badge_image`.
//    That is, the diameter of the transparent cutout needs to be larger than
//    the size of `badge_image`.
gfx::ImageSkia CreateBadgedImageSkia(const gfx::ImageSkia& base_image,
                                     const gfx::ImageSkia& badge_image,
                                     int badge_offset_dip,
                                     int badge_radius_dip,
                                     float scale) {
  // Get the representations for the target scale.
  gfx::ImageSkiaRep base_rep = base_image.GetRepresentation(scale);
  gfx::ImageSkiaRep badge_rep = badge_image.GetRepresentation(scale);

  const SkBitmap& base_skbitmap = base_rep.GetBitmap();
  const SkBitmap& badge_skbitmap = badge_rep.GetBitmap();

  if (base_skbitmap.isNull() || badge_skbitmap.isNull()) {
    // Cannot perform the operations, return `base_image` as fallback.
    return base_image;
  }
  DCHECK_EQ(base_skbitmap.width(), base_skbitmap.height());
  DCHECK_EQ(badge_skbitmap.width(), badge_skbitmap.height());

  // Pixel dimensions of the bitmaps obtained for the target scale.
  int base_size_px = base_skbitmap.width();
  int badge_size_px = badge_skbitmap.width();

  // Convert DIP parameters to pixels for calculations.
  int badge_offset_px = static_cast<int>(std::round(badge_offset_dip * scale));
  int badge_radius_px = static_cast<int>(std::round(badge_radius_dip * scale));

  SkBitmap result_bitmap;
  // Total size of the resulting bitmap in pixels.
  int total_size_px = base_size_px + badge_offset_px;
  result_bitmap.allocN32Pixels(total_size_px, total_size_px);
  result_bitmap.eraseColor(
      SK_ColorTRANSPARENT);  // Initialize with transparency.

  SkCanvas canvas(result_bitmap);
  // Draw base image (which is already the version for the target scale).
  canvas.drawImage(base_skbitmap.asImage(), 0, 0);

  // Calculate badge position.
  int badge_diameter_px = badge_radius_px * 2;
  int badge_padding_px = badge_diameter_px - badge_size_px;
  CHECK_GE(badge_padding_px, 0);
  float badge_draw_start_px =
      total_size_px - badge_diameter_px + badge_padding_px / 2.0f;

  // Create a paint for "punching out" the background.
  SkPaint clear_paint;
  clear_paint.setAntiAlias(true);
  clear_paint.setBlendMode(SkBlendMode::kClear);

  // Calculate badge center position. We'll use a center for the circle.
  float badge_cutout_center_px = total_size_px - badge_radius_px;

  // "Punch out" the area around the badge, then draw the badge.
  canvas.drawCircle(
      SkPoint::Make(badge_cutout_center_px, badge_cutout_center_px),
      badge_radius_px, clear_paint);
  canvas.drawImage(badge_skbitmap.asImage(), badge_draw_start_px,
                   badge_draw_start_px);

  return gfx::ImageSkia::CreateFromBitmap(result_bitmap, scale);
}

}  // namespace

namespace webid {

LetterCircleCroppedImageSkiaSource::LetterCircleCroppedImageSkiaSource(
    const std::u16string& letter,
    int size)
    : gfx::CanvasImageSource(gfx::Size(size, size)), letter_(letter) {}

void LetterCircleCroppedImageSkiaSource::Draw(gfx::Canvas* canvas) {
  const std::vector<std::string> font_names = {
      l10n_util::GetStringUTF8(IDS_NTP_FONT_FAMILY)};
  gfx::DrawMonogramInCanvas(canvas, size().width(), size().width(), letter_,
                            font_names, SK_ColorWHITE, SK_ColorGRAY);
}

CircleCroppedImageSkiaSource::CircleCroppedImageSkiaSource(
    gfx::ImageSkia avatar,
    const std::optional<int>& pre_resize_avatar_crop_size,
    int canvas_edge_size)
    : gfx::CanvasImageSource(gfx::Size(canvas_edge_size, canvas_edge_size)) {
  // The avatar may need to be scaled to match the canvas. To make sure that the
  // avatar's aspect ratio is maintained, we'd need to adjust the scaled
  // dimensions accordingly.
  int scaled_width = canvas_edge_size;
  int scaled_height = canvas_edge_size;
  if (pre_resize_avatar_crop_size) {
    const float avatar_scale =
        canvas_edge_size / static_cast<float>(*pre_resize_avatar_crop_size);
    scaled_width = floor(avatar.width() * avatar_scale);
    scaled_height = floor(avatar.height() * avatar_scale);
  } else {
    // Resize `avatar` so that it completely fills the canvas.
    const float height_ratio = static_cast<float>(avatar.height()) /
                               static_cast<float>(avatar.width());
    if (height_ratio >= 1.0f) {
      scaled_height = floor(canvas_edge_size * height_ratio);
    } else {
      scaled_width = floor(canvas_edge_size / height_ratio);
    }
  }
  avatar_ = gfx::ImageSkiaOperations::CreateResizedImage(
      avatar, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(scaled_width, scaled_height));
}

void CircleCroppedImageSkiaSource::Draw(gfx::Canvas* canvas) {
  const int canvas_edge_size = size().width();

  // Center the avatar in the canvas.
  const int x = (canvas_edge_size - avatar_.width()) / 2;
  const int y = (canvas_edge_size - avatar_.height()) / 2;

  const SkPath circular_mask =
      SkPath::Circle(canvas_edge_size / 2,
                     canvas_edge_size / 2,
                     canvas_edge_size / 2);
  canvas->ClipPath(circular_mask, true);
  canvas->DrawImageInt(avatar_, x, y);
}

std::u16string GetInitialLetterAsUppercase(const std::string& utf8_string) {
  std::u16string utf16_string(base::UTF8ToUTF16(utf8_string));
  base::i18n::BreakIterator iter(utf16_string,
                                 base::i18n::BreakIterator::BREAK_CHARACTER);
  if (!iter.Init()) {
    return u"";
  }

  if (!iter.Advance()) {
    return u"";
  }

  return base::i18n::ToUpper(iter.GetString());
}

gfx::ImageSkia CreateCircleCroppedImage(const gfx::ImageSkia& original_image,
                                        int image_size) {
  return gfx::CanvasImageSource::MakeImageSkia<CircleCroppedImageSkiaSource>(
      original_image, original_image.width() * kMaskableWebIconSafeZoneRatio,
      image_size);
}

gfx::ImageSkia ComputeAccountCircleCroppedPicture(
    const content::IdentityRequestAccount& account,
    int avatar_size,
    std::optional<gfx::ImageSkia> idp_image,
    float device_scale_factor) {
  gfx::ImageSkia avatar;
  if (account.decoded_picture.IsEmpty()) {
    std::u16string letter = GetInitialLetterAsUppercase(account.display_name);
    avatar = gfx::CanvasImageSource::MakeImageSkia<
        LetterCircleCroppedImageSkiaSource>(letter, avatar_size);
  } else {
    avatar =
        gfx::CanvasImageSource::MakeImageSkia<CircleCroppedImageSkiaSource>(
            account.decoded_picture.AsImageSkia(), std::nullopt, avatar_size);
  }
  if (idp_image && idp_image->width() == idp_image->height() &&
      idp_image->width() >=
          kLargeAvatarBadgeSize / kMaskableWebIconSafeZoneRatio) {
    gfx::ImageSkia cropped_idp_image =
        CreateCircleCroppedImage(*idp_image, kLargeAvatarBadgeSize);
    avatar = CreateBadgedImageSkia(avatar, cropped_idp_image, kIdpBadgeOffset,
                                   kIdpBorderRadius, device_scale_factor);
  }
  if (account.is_filtered_out) {
    avatar = gfx::ImageSkiaOperations::CreateTransparentImage(
        avatar, kDisabledAvatarOpacity);
  }
  return avatar;
}

}  // namespace webid
