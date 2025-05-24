// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/identity_ui_utils.h"

#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/monogram_utils.h"
#include "chrome/grit/platform_locale_settings.h"
#include "content/public/browser/identity_request_account.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"

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
                                     int badge_offset,
                                     int badge_radius) {
  // Get the underlying SkBitmaps.
  const SkBitmap* base_bitmap = base_image.bitmap();
  const SkBitmap* badge_bitmap = badge_image.bitmap();

  DCHECK_EQ(base_image.width(), base_image.height());
  DCHECK_EQ(badge_image.width(), badge_image.height());

  int base_size = base_image.width();
  int badge_size = badge_image.width();

  SkBitmap result_bitmap;
  int total_size = base_size + badge_offset;
  result_bitmap.allocN32Pixels(total_size, total_size);

  SkCanvas canvas(result_bitmap);
  canvas.drawImage(base_bitmap->asImage(), 0, 0);

  // Calculate badge position.
  int badge_diameter = badge_radius * 2;
  int badge_outer = badge_diameter - badge_size;
  CHECK_GE(badge_outer, 0);
  int last_position = total_size - 1;
  SkScalar badge_start = last_position - badge_diameter + badge_outer / 2.0f;

  // Create a paint for "punching out" the background.
  SkPaint clear_paint;
  clear_paint.setAntiAlias(true);
  clear_paint.setBlendMode(SkBlendMode::kDstOut);

  // Calculate badge center position. We'll use a center for the circle.
  SkScalar badge_center = last_position - badge_radius;

  // "Punch out" the area around the badge, then draw the badge.
  canvas.drawCircle(badge_center, badge_center, badge_radius, clear_paint);
  canvas.drawImage(badge_bitmap->asImage(), badge_start, badge_start);

  return gfx::ImageSkia::CreateFrom1xBitmap(result_bitmap);
}

}  // namespace

namespace webid {

LetterCircleCroppedImageSkiaSource::LetterCircleCroppedImageSkiaSource(
    const std::u16string& letter,
    int size)
    : gfx::CanvasImageSource(gfx::Size(size, size)), letter_(letter) {}

void LetterCircleCroppedImageSkiaSource::Draw(gfx::Canvas* canvas) {
  monogram::DrawMonogramInCanvas(canvas, size().width(), size().width(),
                                 letter_, SK_ColorWHITE, SK_ColorGRAY);
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

  SkPath circular_mask;
  circular_mask.addCircle(SkIntToScalar(canvas_edge_size / 2),
                          SkIntToScalar(canvas_edge_size / 2),
                          SkIntToScalar(canvas_edge_size / 2));
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
    std::optional<gfx::ImageSkia> idp_image) {
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
                                   kIdpBorderRadius);
  }
  if (account.is_filtered_out) {
    avatar = gfx::ImageSkiaOperations::CreateTransparentImage(
        avatar, kDisabledAvatarOpacity);
  }
  return avatar;
}

}  // namespace webid
