// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/ambient_login_credential_icon.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_request.h"
#include "chrome/grit/platform_locale_settings.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/web_contents.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/monogram_utils.h"
#include "ui/gfx/vector_icon_types.h"

namespace ambient_login_credential_icon {

namespace {

// If the contrast between a monogram and its surroundings is below this, the
// monogram is outlined so that it doesn't blend into them.
constexpr float kMinMonogramSurroundingContrast = 1.5f;

// The width of the outline around monograms that need one, in DIPs.
constexpr float kMonogramOutlineWidth = 1.0f;

// The monogram's diameter, the same as account pictures', in DIPs.
constexpr int kMonogramSize = ambient_signin::kAccountPictureSize;

class MonogramImageSource : public gfx::CanvasImageSource {
 public:
  MonogramImageSource(std::u16string text,
                      SkColor background_color,
                      SkColor text_color,
                      std::optional<SkColor> outline_color)
      : gfx::CanvasImageSource(gfx::Size(kMonogramSize, kMonogramSize)),
        text_(std::move(text)),
        background_color_(background_color),
        text_color_(text_color),
        outline_color_(outline_color) {}

  MonogramImageSource(const MonogramImageSource&) = delete;
  MonogramImageSource& operator=(const MonogramImageSource&) = delete;

  ~MonogramImageSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    const std::vector<std::string> font_names = {
        l10n_util::GetStringUTF8(IDS_NTP_FONT_FAMILY)};
    gfx::DrawMonogramInCanvas(canvas, kMonogramSize, kMonogramSize, text_,
                              font_names, text_color_, background_color_);

    if (outline_color_) {
      // The outline is drawn just inside the monogram, so that the monogram
      // keeps its size.
      cc::PaintFlags outline_flags;
      outline_flags.setAntiAlias(true);
      outline_flags.setStyle(cc::PaintFlags::kStroke_Style);
      outline_flags.setStrokeWidth(kMonogramOutlineWidth);
      outline_flags.setColor(*outline_color_);
      const float center = kMonogramSize / 2.0f;
      canvas->DrawCircle(gfx::PointF(center, center),
                         (kMonogramSize - kMonogramOutlineWidth) / 2.0f,
                         outline_flags);
    }
  }

 private:
  const std::u16string text_;
  const SkColor background_color_;
  const SkColor text_color_;
  const std::optional<SkColor> outline_color_;
};

gfx::ImageSkia GenerateMonogram(std::u16string text,
                                std::optional<SkColor> site_color,
                                const ui::ColorProvider* provider) {
  SkColor background_color;
  SkColor text_color;
  if (site_color) {
    background_color = *site_color;
    text_color = color_utils::GetColorWithMaxContrast(background_color);
  } else {
    background_color = provider ? provider->GetColor(ui::kColorSysPrimary)
                                : gfx::kGoogleBlue600;
    text_color =
        provider ? provider->GetColor(ui::kColorSysOnPrimary) : SK_ColorWHITE;
  }
  const SkColor bubble_background_color =
      provider ? provider->GetColor(ui::kColorBubbleBackground) : SK_ColorWHITE;
  std::optional<SkColor> outline_color;
  if (internal::NeedsMonogramOutline(background_color,
                                     bubble_background_color)) {
    outline_color =
        provider ? provider->GetColor(ui::kColorSeparator) : SK_ColorGRAY;
  }
  return gfx::CanvasImageSource::MakeImageSkia<MonogramImageSource>(
      std::move(text), background_color, text_color, outline_color);
}

}  // namespace

namespace internal {

std::u16string GetMonogramText(std::u16string_view name) {
  // Use a grapheme cluster iterator so that combining marks stay attached to
  // their base character (e.g. 'e' + U+0301 -> "É"). Skip leading whitespace,
  // punctuation, control/format characters (such as bidi marks and zero-width
  // characters), and other default-ignorable code points.
  base::i18n::BreakIterator iter(name,
                                 base::i18n::BreakIterator::BREAK_CHARACTER);
  if (!iter.Init()) {
    return std::u16string();
  }
  while (iter.Advance()) {
    const std::u16string_view grapheme = iter.GetString();
    size_t char_index = 0;
    base_icu::UChar32 code_point = 0;
    if (base::ReadUnicodeCharacter(grapheme, &char_index, &code_point) &&
        (U_GET_GC_MASK(code_point) &
         (U_GC_L_MASK | U_GC_N_MASK | U_GC_SO_MASK)) &&
        !u_hasBinaryProperty(code_point, UCHAR_DEFAULT_IGNORABLE_CODE_POINT)) {
      return base::i18n::ToUpper(grapheme);
    }
  }
  return std::u16string();
}

std::u16string GetMonogramTextForCredential(
    const ambient_signin::PasskeyOrPasswordCredential& credential) {
  if (credential.type == ambient_signin::CredentialType::kPasskey) {
    std::u16string text = GetMonogramText(credential.display_name);
    if (!text.empty()) {
      return text;
    }
  }
  return GetMonogramText(credential.username);
}

bool NeedsMonogramOutline(SkColor monogram_background,
                          SkColor surrounding_color) {
  return color_utils::GetContrastRatio(monogram_background, surrounding_color) <
         kMinMonogramSurroundingContrast;
}

bool IsBitmapTransparent(const SkBitmap& bitmap) {
  for (int y = 0; y < bitmap.height(); ++y) {
    for (int x = 0; x < bitmap.width(); ++x) {
      if (SkColorGetA(bitmap.getColor(x, y)) != SK_AlphaTRANSPARENT) {
        return false;
      }
    }
  }
  return true;
}

std::optional<SkColor> GetSiteColor(const gfx::Image& favicon,
                                    std::optional<SkColor> theme_color) {
  if (!favicon.IsEmpty()) {
    const SkBitmap& bitmap = *favicon.ToSkBitmap();
    // CalculateKMeanColorOfBitmap() returns white if every pixel is
    // transparent.
    if (!IsBitmapTransparent(bitmap)) {
      // Prefer a vibrant colour over neutral (nearly black, nearly white, or
      // grey) backgrounds; CalculateKMeanColorOfBitmap() falls back to the
      // largest cluster if all clusters are neutral.
      constexpr color_utils::HSL kLowerBound = {-1, 0.15, 0.15};
      constexpr color_utils::HSL kUpperBound = {-1, 1.0, 0.85};
      return color_utils::CalculateKMeanColorOfBitmap(bitmap, bitmap.height(),
                                                      kLowerBound, kUpperBound,
                                                      /*find_closest=*/true);
    }
  }
  if (theme_color) {
    return SkColorSetA(*theme_color, SK_AlphaOPAQUE);
  }
  return std::nullopt;
}

}  // namespace internal

std::optional<SkColor> GetSiteColorForWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return std::nullopt;
  }
  // The favicon that the browser has already fetched for the page is used, so
  // that no extra requests are made. It may not have loaded yet, in which case
  // the theme colour is used.
  gfx::Image favicon;
  if (auto* favicon_driver =
          favicon::ContentFaviconDriver::FromWebContents(web_contents);
      favicon_driver && favicon_driver->FaviconIsValid()) {
    favicon = favicon_driver->GetFavicon();
  }
  return internal::GetSiteColor(favicon, web_contents->GetThemeColor());
}

ui::ImageModel CreateCredentialIcon(
    const ambient_signin::PasskeyOrPasswordCredential& credential,
    std::optional<SkColor> site_color) {
  std::u16string text = internal::GetMonogramTextForCredential(credential);
  if (text.empty()) {
    return ui::ImageModel::FromVectorIcon(
        ambient_signin::AmbientLoginPermissionRequest::GetCredentialIcon(
            credential.type),
        ui::kColorIcon, kVectorIconSize);
  }
  return ui::ImageModel::FromImageGenerator(
      base::BindRepeating(&GenerateMonogram, std::move(text), site_color),
      gfx::Size(kMonogramSize, kMonogramSize));
}

ui::ImageModel GetCredentialBadge(
    const ambient_signin::PasskeyOrPasswordCredential& credential) {
  // Credentials from providers without an icon get the generic passkey or
  // password icon. Today, only passkeys get here (e.g. from a phone, see
  // `webauthn_ui_helpers::GetProviderIcon()`), since passwords are always from
  // Google Password Manager.
  const gfx::VectorIcon& badge_icon =
      credential.provider_icon
          ? *credential.provider_icon
          : ambient_signin::AmbientLoginPermissionRequest::GetCredentialIcon(
                credential.type);
  return ui::ImageModel::FromVectorIcon(badge_icon, ui::kColorIcon,
                                        ambient_signin::kBadgeImageSize);
}

}  // namespace ambient_login_credential_icon
