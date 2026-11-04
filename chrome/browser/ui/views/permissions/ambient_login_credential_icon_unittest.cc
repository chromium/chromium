// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/ambient_login_credential_icon.h"

#include <optional>
#include <string>

#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_request.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace ambient_login_credential_icon {
namespace {

using internal::GetMonogramText;
using internal::GetMonogramTextForCredential;
using internal::GetSiteColor;
using internal::NeedsMonogramOutline;

constexpr int kSize = ambient_signin::kAccountPictureSize;

// A point near the top of the monogram, clear of its edge, letter and badge.
constexpr gfx::Point kTopOfMonogram(kSize / 2, 3);

// Returns a `size` x `size` bitmap filled with `color`.
SkBitmap CreateBitmap(SkColor color, int size = 16) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseColor(color);
  return bitmap;
}

TEST(AmbientLoginCredentialIconTest, MonogramText) {
  const struct {
    std::u16string name;
    std::u16string expected;
  } kTests[] = {
      {u"alice@example.com", u"A"},
      {u"Bob", u"B"},
      {u"123@gmail.com", u"1"},
      {u"\U0001F600frank", u"\U0001F600"},
      // Leading whitespace, punctuation, and math symbols are skipped.
      {u"  carol", u"C"},
      {u"+1 555-0100", u"1"},
      {u"(555) 123-4567", u"5"},
      {u"_dave", u"D"},
      // Leading bidi marks, zero-width characters, and default-ignorable
      // characters are skipped.
      {u"\u200E\u200Falice", u"A"},
      {u"\u200B\u200C\u200D\uFEFFbob", u"B"},
      {u"\u3164carol", u"C"},  // Hangul filler.
      // Accented letters, including with combining marks, stay intact.
      {u"\u00E9lodie", u"\u00C9"},
      {u"e\u0301mile", u"E\u0301"},
      // Non-Latin scripts.
      {u"\uAE40\uCCA0\uC218", u"\uAE40"},  // Hangul.
      {u"\u0639\u0644\u064A", u"\u0639"},  // Arabic.
      {u"\u5F20\u4F1F", u"\u5F20"},        // Han.
      // Names with no letters, numbers, or non-ignorable symbols return empty.
      {u"", u""},
      {u"   \u200B\u200E", u""},
      {u"---", u""},
  };
  for (const auto& test : kTests) {
    SCOPED_TRACE(test.name);
    EXPECT_EQ(GetMonogramText(test.name), test.expected);
  }
}

TEST(AmbientLoginCredentialIconTest, SiteColorFromFavicon) {
  // The favicon's colour is used even if the site has a theme colour.
  constexpr SkColor kFaviconColor = SkColorSetRGB(0x1A, 0x73, 0xE8);
  EXPECT_EQ(
      GetSiteColor(gfx::Image::CreateFrom1xBitmap(CreateBitmap(kFaviconColor)),
                   SkColorSetRGB(0xEA, 0x43, 0x35)),
      kFaviconColor);
}

TEST(AmbientLoginCredentialIconTest, SiteColorPrefersVibrantFaviconColor) {
  // Most of the favicon is white, but a vibrant colour is preferred.
  constexpr SkColor kVibrantColor = SkColorSetRGB(0x18, 0x80, 0x38);
  SkBitmap bitmap = CreateBitmap(SK_ColorWHITE);
  bitmap.erase(kVibrantColor, SkIRect::MakeWH(4, 4));
  EXPECT_EQ(GetSiteColor(gfx::Image::CreateFrom1xBitmap(bitmap), std::nullopt),
            kVibrantColor);
}

TEST(AmbientLoginCredentialIconTest, SiteColorFromNeutralFavicon) {
  // If a favicon has no vibrant colours, its most common neutral colour is
  // used and still preferred to the theme colour.
  SkBitmap bitmap = CreateBitmap(SK_ColorBLACK);
  bitmap.erase(SkColorSetRGB(0x80, 0x80, 0x80), SkIRect::MakeWH(4, 4));
  EXPECT_EQ(GetSiteColor(gfx::Image::CreateFrom1xBitmap(bitmap),
                         SkColorSetRGB(0xEA, 0x43, 0x35)),
            SK_ColorBLACK);
}

TEST(AmbientLoginCredentialIconTest, SiteColorFromThemeColor) {
  // Without a usable favicon, the theme colour is used as is.
  constexpr SkColor kThemeColor = SkColorSetRGB(0xFB, 0xBC, 0x04);
  EXPECT_EQ(GetSiteColor(gfx::Image(), kThemeColor), kThemeColor);
  EXPECT_EQ(GetSiteColor(gfx::Image::CreateFrom1xBitmap(
                             CreateBitmap(SK_ColorTRANSPARENT)),
                         kThemeColor),
            kThemeColor);
}

TEST(AmbientLoginCredentialIconTest, SiteColorDefault) {
  EXPECT_EQ(GetSiteColor(gfx::Image(), std::nullopt), std::nullopt);
  EXPECT_EQ(GetSiteColor(gfx::Image::CreateFrom1xBitmap(
                             CreateBitmap(SK_ColorTRANSPARENT)),
                         std::nullopt),
            std::nullopt);
}

TEST(AmbientLoginCredentialIconTest, CreatesMonogramInSiteColor) {
  ambient_signin::PasskeyOrPasswordCredential credential{
      u"alice@example.com", u"Google Password Manager",
      ambient_signin::CredentialType::kPasskey};
  credential.provider_icon = &vector_icons::kPasskeyIcon;
  constexpr SkColor kSiteColor = SkColorSetRGB(0x18, 0x80, 0x38);

  const ui::ImageModel icon = CreateCredentialIcon(credential, kSiteColor);
  ASSERT_TRUE(icon.IsImageGenerator());
  EXPECT_EQ(icon.Size(), gfx::Size(kSize, kSize));

  ui::ColorProvider color_provider;
  color_provider.SetColorForTesting(ui::kColorBubbleBackground, SK_ColorWHITE);
  const gfx::ImageSkia image = icon.Rasterize(&color_provider);
  const SkBitmap* bitmap = image.bitmap();
  ASSERT_TRUE(bitmap);
  EXPECT_EQ(bitmap->width(), kSize);
  EXPECT_EQ(bitmap->getColor(kTopOfMonogram.x(), kTopOfMonogram.y()),
            kSiteColor);

  const ui::ImageModel badge = GetCredentialBadge(credential);
  EXPECT_TRUE(badge.IsVectorIcon());
  EXPECT_EQ(badge.Size(), gfx::Size(ambient_signin::kBadgeImageSize,
                                    ambient_signin::kBadgeImageSize));
}

TEST(AmbientLoginCredentialIconTest, CreatesMonogramInDefaultThemeColor) {
  ambient_signin::PasskeyOrPasswordCredential credential{
      u"alice@example.com", u"Google Password Manager",
      ambient_signin::CredentialType::kPasskey};
  constexpr SkColor kPrimaryColor = SkColorSetRGB(0x1A, 0x73, 0xE8);

  ui::ColorProvider color_provider;
  color_provider.SetColorForTesting(ui::kColorSysPrimary, kPrimaryColor);
  color_provider.SetColorForTesting(ui::kColorSysOnPrimary, SK_ColorWHITE);
  color_provider.SetColorForTesting(ui::kColorBubbleBackground, SK_ColorWHITE);

  const gfx::ImageSkia image =
      CreateCredentialIcon(credential, std::nullopt).Rasterize(&color_provider);
  ASSERT_TRUE(image.bitmap());
  EXPECT_EQ(image.bitmap()->getColor(kTopOfMonogram.x(), kTopOfMonogram.y()),
            kPrimaryColor);
}

TEST(AmbientLoginCredentialIconTest, NeedsMonogramOutline) {
  EXPECT_TRUE(NeedsMonogramOutline(SK_ColorWHITE, SK_ColorWHITE));
  // Very light grey on white.
  EXPECT_TRUE(
      NeedsMonogramOutline(SkColorSetRGB(0xF1, 0xF3, 0xF4), SK_ColorWHITE));
  // Nearly black on a dark background.
  EXPECT_TRUE(
      NeedsMonogramOutline(SK_ColorBLACK, SkColorSetRGB(0x20, 0x21, 0x24)));
  EXPECT_FALSE(
      NeedsMonogramOutline(SkColorSetRGB(0x1A, 0x73, 0xE8), SK_ColorWHITE));
  EXPECT_FALSE(NeedsMonogramOutline(SK_ColorBLACK, SK_ColorWHITE));
  EXPECT_FALSE(
      NeedsMonogramOutline(SK_ColorWHITE, SkColorSetRGB(0x20, 0x21, 0x24)));
}

TEST(AmbientLoginCredentialIconTest, OutlinesMonogramCloseToBubbleColor) {
  ambient_signin::PasskeyOrPasswordCredential credential{
      u"alice@example.com", u"", ambient_signin::CredentialType::kPasskey};
  ui::ColorProvider color_provider;
  color_provider.SetColorForTesting(ui::kColorBubbleBackground, SK_ColorWHITE);
  color_provider.SetColorForTesting(ui::kColorSysPrimary,
                                    SkColorSetRGB(0x1A, 0x73, 0xE8));
  color_provider.SetColorForTesting(ui::kColorSysOnPrimary, SK_ColorWHITE);
  constexpr SkColor kSeparatorColor = SK_ColorRED;
  color_provider.SetColorForTesting(ui::kColorSeparator, kSeparatorColor);
  // The top edge of the monogram, where an outline would be.
  const gfx::Point top_edge(kSize / 2, 0);

  // A white monogram on a white bubble is outlined.
  {
    const gfx::ImageSkia image = CreateCredentialIcon(credential, SK_ColorWHITE)
                                     .Rasterize(&color_provider);
    ASSERT_TRUE(image.bitmap());
    const SkColor edge = image.bitmap()->getColor(top_edge.x(), top_edge.y());
    EXPECT_GT(SkColorGetR(edge), 0xC0u);
    EXPECT_LT(SkColorGetG(edge), 0x80u);
    // The inside of the monogram isn't affected.
    EXPECT_EQ(image.bitmap()->getColor(kTopOfMonogram.x(), kTopOfMonogram.y()),
              SK_ColorWHITE);
  }

  // A blue monogram stands out on its own, so it isn't outlined.
  {
    const gfx::ImageSkia image = CreateCredentialIcon(credential, std::nullopt)
                                     .Rasterize(&color_provider);
    ASSERT_TRUE(image.bitmap());
    const SkColor edge = image.bitmap()->getColor(top_edge.x(), top_edge.y());
    EXPECT_GT(SkColorGetB(edge), SkColorGetR(edge));
  }
}

TEST(AmbientLoginCredentialIconTest, PasskeyDisplayNameIsPreferred) {
  EXPECT_EQ(GetMonogramTextForCredential(
                {u"alice@example.com", u"",
                 ambient_signin::CredentialType::kPasskey, u"Zoe"}),
            u"Z");
  // Without a display name, the username is used.
  EXPECT_EQ(GetMonogramTextForCredential(
                {u"alice@example.com", u"",
                 ambient_signin::CredentialType::kPasskey, u""}),
            u"A");
  // Passwords have no display name, so the username is used.
  EXPECT_EQ(GetMonogramTextForCredential(
                {u"alice@example.com", u"",
                 ambient_signin::CredentialType::kPassword, u"Zoe"}),
            u"A");
}

TEST(AmbientLoginCredentialIconTest, FallsBackToVectorIconWhenUsernameIsEmpty) {
  ambient_signin::PasskeyOrPasswordCredential credential{
      u"", u"", ambient_signin::CredentialType::kPassword};

  const ui::ImageModel icon = CreateCredentialIcon(credential, std::nullopt);
  ASSERT_TRUE(icon.IsVectorIcon());
  EXPECT_EQ(icon.Size(), gfx::Size(kVectorIconSize, kVectorIconSize));
}

}  // namespace
}  // namespace ambient_login_credential_icon
