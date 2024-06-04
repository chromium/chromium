// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_background_impl.h"

#include <memory>

#include "base/i18n/base_i18n_switches.h"
#include "base/i18n/rtl.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_command_line.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_base.h"

namespace media_message_center {

namespace {

constexpr double kLightLuma = 0.9;
constexpr double kNormalLuma = 0.5;
constexpr double kDarkLuma = 0.2;

constexpr double kMutedSaturation = 0.2;
constexpr double kVibrantSaturation = 0.8;

constexpr int kDefaultForegroundArtworkHeight = 100;

SkColor GetColorFromSL(double s, double l) {
  return color_utils::HSLToSkColor({0.2, s, l}, SK_AlphaOPAQUE);
}

gfx::ImageSkia CreateTestBackgroundImage(SkColor first_color,
                                         SkColor second_color,
                                         int second_height) {
  constexpr SkColor kRightHandSideColor = SK_ColorMAGENTA;

  DCHECK_NE(kRightHandSideColor, first_color);
  DCHECK_NE(kRightHandSideColor, second_color);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);

  int first_height = bitmap.height() - second_height;
  int right_width = bitmap.width() / 2;

  // Fill the right hand side of the image with a constant color. The color
  // derivation algorithm does not look at the right hand side so we should
  // never see |kRightHandSideColor|.
  bitmap.erase(kRightHandSideColor,
               {right_width, 0, bitmap.width(), bitmap.height()});

  // Fill the left hand side with |first_color|.
  bitmap.erase(first_color, {0, 0, right_width, first_height});

  // Fill the left hand side with |second_color|.
  bitmap.erase(second_color, {0, first_height, right_width, bitmap.height()});

  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

gfx::ImageSkia CreateTestBackgroundImage(SkColor color) {
  return CreateTestBackgroundImage(color, SK_ColorTRANSPARENT, 0);
}

}  // namespace

class MediaNotificationBackgroundImplTest : public views::ViewsTestBase {
 public:
  MediaNotificationBackgroundImplTest() = default;

  MediaNotificationBackgroundImplTest(
      const MediaNotificationBackgroundImplTest&) = delete;
  MediaNotificationBackgroundImplTest& operator=(
      const MediaNotificationBackgroundImplTest&) = delete;

  ~MediaNotificationBackgroundImplTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();
    background_ =
        std::make_unique<MediaNotificationBackgroundImpl>(10, 10, 0.1);
    EXPECT_FALSE(GetBackgroundColor().has_value());
  }

  void TearDown() override {
    background_.reset();
    views::ViewsTestBase::TearDown();
  }

  MediaNotificationBackgroundImpl* background() const {
    return background_.get();
  }

  std::optional<SkColor> GetBackgroundColor() const {
    return background_->background_color_;
  }

  std::optional<SkColor> GetForegroundColor() const {
    return background_->foreground_color_;
  }

  double GetBackgroundFaviconColorShadeFactor() const {
    return MediaNotificationBackgroundImpl::kBackgroundFaviconColorShadeFactor;
  }

 private:
  std::unique_ptr<MediaNotificationBackgroundImpl> background_;
};

// If we have no artwork then we should use the default background color.
TEST_F(MediaNotificationBackgroundImplTest, DeriveBackgroundColor_NoArtwork) {
  background()->UpdateArtwork(gfx::ImageSkia());
  EXPECT_FALSE(GetBackgroundColor().has_value());
}

// If we have artwork with no popular color then we should use the default
// background color.
TEST_F(MediaNotificationBackgroundImplTest,
       DeriveBackgroundColor_NoPopularColor) {
  background()->UpdateArtwork(CreateTestBackgroundImage(SK_ColorTRANSPARENT));
  EXPECT_FALSE(GetBackgroundColor().has_value());
}

// If the most popular color is not white or black then we should use that
// color.
TEST_F(MediaNotificationBackgroundImplTest,
       DeriveBackgroundColor_PopularNonWhiteBlackColor) {
  constexpr SkColor kTestColor = SK_ColorYELLOW;
  background()->UpdateArtwork(CreateTestBackgroundImage(kTestColor));
  EXPECT_EQ(kTestColor, GetBackgroundColor());
}

TEST_F(MediaNotificationBackgroundImplTest,
       DeriveBackgroundColor_NoArtworkAfterHavingOne) {
  constexpr SkColor kTestColor = SK_ColorYELLOW;
  background()->UpdateArtwork(CreateTestBackgroundImage(kTestColor));
  EXPECT_EQ(kTestColor, GetBackgroundColor());

  background()->UpdateArtwork(gfx::ImageSkia());
  EXPECT_FALSE(GetBackgroundColor().has_value());
}

// Favicons should be used when available but have a shade applying to them.
TEST_F(MediaNotificationBackgroundImplTest,
       DeriveBackgroundColor_PopularNonWhiteBlackColorFavicon) {
  constexpr SkColor kTestColor = SK_ColorYELLOW;
  background()->UpdateFavicon(CreateTestBackgroundImage(kTestColor));
  const SkColor expected_color = SkColorSetRGB(
      SkColorGetR(kTestColor) * GetBackgroundFaviconColorShadeFactor(),
      SkColorGetG(kTestColor) * GetBackgroundFaviconColorShadeFactor(),
      SkColorGetB(kTestColor) * GetBackgroundFaviconColorShadeFactor());
  EXPECT_EQ(expected_color, GetBackgroundColor());
}

TEST_F(MediaNotificationBackgroundImplTest,
       DeriveBackgroundColor_NoFaviconAfterHavingOne) {
  constexpr SkColor kTestColor = SK_ColorYELLOW;
  background()->UpdateFavicon(CreateTestBackgroundImage(kTestColor));
  const SkColor expected_color = SkColorSetRGB(
      SkColorGetR(kTestColor) * GetBackgroundFaviconColorShadeFactor(),
      SkColorGetG(kTestColor) * GetBackgroundFaviconColorShadeFactor(),
      SkColorGetB(kTestColor) * GetBackgroundFaviconColorShadeFactor());
  EXPECT_EQ(expected_color, GetBackgroundColor());

  background()->UpdateFavicon(gfx::ImageSkia());
  EXPECT_FALSE(GetBackgroundColor().has_value());
}

TEST_F(MediaNotificationBackgroundImplTest,
       DeriveBackgroundColor_FaviconSetThenArtwork) {
  constexpr SkColor kArtworkColor = SK_ColorYELLOW;
  constexpr SkColor kFaviconColor = SK_ColorRED;

  background()->UpdateFavicon(CreateTestBackgroundImage(kFaviconColor));
  background()->UpdateArtwork(CreateTestBackgroundImage(kArtworkColor));

  EXPECT_EQ(kArtworkColor, GetBackgroundColor());
}

TEST_F(MediaNotificationBackgroundImplTest,
       DeriveBackgroundColor_ArtworkSetThenFavicon) {
  constexpr SkColor kArtworkColor = SK_ColorYELLOW;
  constexpr SkColor kFaviconColor = SK_ColorRED;

  background()->UpdateArtwork(CreateTestBackgroundImage(kArtworkColor));
  background()->UpdateFavicon(CreateTestBackgroundImage(kFaviconColor));

  EXPECT_EQ(kArtworkColor, GetBackgroundColor());
}

TEST_F(MediaNotificationBackgroundImplTest,
       DeriveBackgroundColor_SetAndRemoveArtworkWithFavicon) {
  constexpr SkColor kArtworkColor = SK_ColorYELLOW;
  constexpr SkColor kFaviconColor = SK_ColorRED;

  background()->UpdateArtwork(CreateTestBackgroundImage(kArtworkColor));
  background()->UpdateFavicon(CreateTestBackgroundImage(kFaviconColor));
  background()->UpdateArtwork(gfx::ImageSkia());

  const SkColor expected_color = SkColorSetRGB(
      SkColorGetR(kFaviconColor) * GetBackgroundFaviconColorShadeFactor(),
      SkColorGetG(kFaviconColor) * GetBackgroundFaviconColorShadeFactor(),
      SkColorGetB(kFaviconColor) * GetBackgroundFaviconColorShadeFactor());
  EXPECT_EQ(expected_color, GetBackgroundColor());
}

TEST_F(MediaNotificationBackgroundImplTest, GetBackgroundColorRespectsTheme) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  auto* theme = widget->GetNativeTheme();
  theme->set_use_dark_colors(false);
  auto* owner = widget->SetContentsView(std::make_unique<views::View>());
  SkColor light_background_color = background()->GetBackgroundColor(*owner);

  theme->set_use_dark_colors(true);
  EXPECT_NE(light_background_color, background()->GetBackgroundColor(*owner));
}

// MediaNotificationBackgroundImplBlackWhiteTest will repeat these tests with a
// parameter that is either black or white.
class MediaNotificationBackgroundImplBlackWhiteTest
    : public MediaNotificationBackgroundImplTest,
      public testing::WithParamInterface<SkColor> {
 public:
  bool IsBlack() const { return GetParam() == SK_ColorBLACK; }

  gfx::ImageSkia CreateTestForegroundArtwork(const SkColor& first,
                                             const SkColor& second,
                                             int first_width,
                                             int second_height) {
    gfx::Rect area(100, 100);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(area.width(), area.height());
    bitmap.eraseColor(GetParam());

    area.Inset(gfx::Insets::TLBR(0, 40, 0, 0));
    bitmap.erase(first, gfx::RectToSkIRect(area));

    area.Inset(
        gfx::Insets::TLBR(area.height() - second_height, first_width, 0, 0));
    bitmap.erase(second, gfx::RectToSkIRect(area));

    return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         MediaNotificationBackgroundImplBlackWhiteTest,
                         testing::Values(SK_ColorBLACK, SK_ColorWHITE));

// If the most popular color is black or white but there is no secondary color
// we should use the most popular color.
TEST_P(MediaNotificationBackgroundImplBlackWhiteTest,
       DeriveBackgroundColor_PopularBlackWhiteNoSecondaryColor) {
  background()->UpdateArtwork(CreateTestBackgroundImage(GetParam()));
  EXPECT_EQ(GetParam(), GetBackgroundColor());
}

// If the most popular color is black or white and there is a secondary color
// that is very minor then we should use the most popular color.
TEST_P(MediaNotificationBackgroundImplBlackWhiteTest,
       DeriveBackgroundColor_VeryPopularBlackWhite) {
  background()->UpdateArtwork(
      CreateTestBackgroundImage(GetParam(), SK_ColorYELLOW, 20));
  EXPECT_EQ(GetParam(), GetBackgroundColor());
}

// If the most popular color is black or white but it is not that popular then
// we should use the secondary color.
TEST_P(MediaNotificationBackgroundImplBlackWhiteTest,
       DeriveBackgroundColor_NotVeryPopularBlackWhite) {
  constexpr SkColor kTestColor = SK_ColorYELLOW;
  background()->UpdateArtwork(
      CreateTestBackgroundImage(GetParam(), kTestColor, 40));
  EXPECT_EQ(kTestColor, GetBackgroundColor());
}

// If there are multiple vibrant colors then the foreground color should be the
// most popular one.
TEST_P(MediaNotificationBackgroundImplBlackWhiteTest,
       DeriveForegroundColor_Palette_MultiVibrant) {
  const SkColor kTestColor = SK_ColorCYAN;

  background()->UpdateArtwork(CreateTestForegroundArtwork(
      kTestColor, GetColorFromSL(kVibrantSaturation, kDarkLuma), 59,
      kDefaultForegroundArtworkHeight));

  EXPECT_EQ(GetParam(), GetBackgroundColor());
  EXPECT_EQ(kTestColor, GetForegroundColor());
}

// If there is a vibrant and muted color then the foreground color should be the
// more vibrant one.
TEST_P(MediaNotificationBackgroundImplBlackWhiteTest,
       DeriveForegroundColor_Palette_Vibrant) {
  const SkColor kTestColor = GetColorFromSL(kVibrantSaturation, kNormalLuma);

  background()->UpdateArtwork(CreateTestForegroundArtwork(
      kTestColor, GetColorFromSL(kMutedSaturation, kNormalLuma), 30,
      kDefaultForegroundArtworkHeight));

  EXPECT_EQ(GetParam(), GetBackgroundColor());
  EXPECT_EQ(kTestColor, GetForegroundColor());
}

// If there are multiple muted colors then the foreground color should be the
// most popular one.
TEST_P(MediaNotificationBackgroundImplBlackWhiteTest,
       DeriveForegroundColor_Palette_MultiMuted) {
  const SkColor kTestColor = GetColorFromSL(kMutedSaturation, kNormalLuma);

  background()->UpdateArtwork(CreateTestForegroundArtwork(
      kTestColor, GetColorFromSL(kMutedSaturation, kDarkLuma), 59,
      kDefaultForegroundArtworkHeight));

  EXPECT_EQ(GetParam(), GetBackgroundColor());
  EXPECT_EQ(kTestColor, GetForegroundColor());
}

// If there is a normal and light muted color then the foreground color should
// be the normal one.
TEST_P(MediaNotificationBackgroundImplBlackWhiteTest,
       DeriveForegroundColor_Palette_Muted) {
  const SkColor kTestColor = GetColorFromSL(kMutedSaturation, kNormalLuma);
  const SkColor kSecondColor =
      GetColorFromSL(kMutedSaturation, IsBlack() ? kDarkLuma : kLightLuma);

  background()->UpdateArtwork(CreateTestForegroundArtwork(
      kTestColor, kSecondColor, 30, kDefaultForegroundArtworkHeight));

  EXPECT_EQ(GetParam(), GetBackgroundColor());
  EXPECT_EQ(kTestColor, GetForegroundColor());
}

// If the best color is not the most popular one, but the most popular one is
// not that popular then we should use the best color.
TEST_P(MediaNotificationBackgroundImplBlackWhiteTest,
       DeriveForegroundColor_Palette_NotPopular) {
  const SkColor kTestColor = SK_ColorMAGENTA;

  background()->UpdateArtwork(CreateTestForegroundArtwork(
      kTestColor, GetColorFromSL(kMutedSaturation, kNormalLuma), 25,
      kDefaultForegroundArtworkHeight));

  EXPECT_EQ(GetParam(), GetBackgroundColor());
  EXPECT_EQ(kTestColor, GetForegroundColor());
}

// If we do not have a best color but we have a popular one over a threshold
// then we should use that one.
TEST_P(MediaNotificationBackgroundImplBlackWhiteTest,
       DeriveForegroundColor_MostPopular) {
  const SkColor kTestColor = GetColorFromSL(kMutedSaturation, kNormalLuma);

  background()->UpdateArtwork(CreateTestForegroundArtwork(
      kTestColor, GetColorFromSL(kVibrantSaturation, kNormalLuma), 59, 50));

  EXPECT_EQ(GetParam(), GetBackgroundColor());
  EXPECT_EQ(kTestColor, GetForegroundColor());
}

// If the background color is dark then we should select for a lighter color,
// otherwise we should select for a darker one.
TEST_P(MediaNotificationBackgroundImplBlackWhiteTest,
       DeriveForegroundColor_Palette_MoreVibrant) {
  const SkColor kTestColor =
      GetColorFromSL(kVibrantSaturation, IsBlack() ? kLightLuma : kDarkLuma);

  background()->UpdateArtwork(CreateTestForegroundArtwork(
      kTestColor, GetColorFromSL(kVibrantSaturation, kNormalLuma), 30,
      kDefaultForegroundArtworkHeight));

  EXPECT_EQ(GetParam(), GetBackgroundColor());
  EXPECT_EQ(kTestColor, GetForegroundColor());
}

// If the background color is dark then we should select for a lighter color,
// otherwise we should select for a darker one.
TEST_P(MediaNotificationBackgroundImplBlackWhiteTest,
       DeriveForegroundColor_Palette_MoreMuted) {
  const SkColor kTestColor =
      GetColorFromSL(kMutedSaturation, IsBlack() ? kLightLuma : kDarkLuma);
  const SkColor kSecondColor =
      GetColorFromSL(kMutedSaturation, IsBlack() ? kDarkLuma : kLightLuma);

  background()->UpdateArtwork(CreateTestForegroundArtwork(
      kTestColor, kSecondColor, 30, kDefaultForegroundArtworkHeight));

  EXPECT_EQ(GetParam(), GetBackgroundColor());
  EXPECT_EQ(kTestColor, GetForegroundColor());
}

// If we do not have any colors then we should use the fallback color based on
// the background color.
TEST_P(MediaNotificationBackgroundImplBlackWhiteTest,
       DeriveForegroundColor_Fallback) {
  background()->UpdateArtwork(CreateTestForegroundArtwork(
      SK_ColorTRANSPARENT, SK_ColorTRANSPARENT, 0, 0));

  EXPECT_EQ(GetParam(), GetBackgroundColor());
  EXPECT_EQ(GetParam() == SK_ColorBLACK ? SK_ColorWHITE : SK_ColorBLACK,
            GetForegroundColor());
}

// MediaNotificationBackgroundImplRTLTest will repeat these tests with RTL
// disabled and enabled.
class MediaNotificationBackgroundImplRTLTest
    : public MediaNotificationBackgroundImplTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        switches::kForceUIDirection, GetParam() ? switches::kForceDirectionRTL
                                                : switches::kForceDirectionLTR);

    MediaNotificationBackgroundImplTest::SetUp();

    ASSERT_EQ(IsRTL(), base::i18n::IsRTL());
  }

  bool IsRTL() const { return GetParam(); }

 private:
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_;
  base::test::ScopedCommandLine command_line_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         MediaNotificationBackgroundImplRTLTest,
                         testing::Bool());

TEST_P(MediaNotificationBackgroundImplRTLTest, BoundsSanityCheck) {
  // The test notification will have a width of 200 and a height of 50.
  gfx::Rect bounds(0, 0, 200, 50);
  auto owner = std::make_unique<views::StaticSizedView>();
  owner->SetBoundsRect(bounds);
  ASSERT_EQ(bounds, owner->GetContentsBounds());

  // Check the artwork is not visible by default.
  EXPECT_EQ(0, background()->GetArtworkWidth(bounds.size()));
  EXPECT_EQ(0, background()->GetArtworkVisibleWidth(bounds.size()));
  EXPECT_EQ(gfx::Rect(IsRTL() ? 0 : 200, 0, 0, 50),
            background()->GetArtworkBounds(*owner.get()));
  EXPECT_EQ(gfx::Rect(IsRTL() ? 0 : 0, 0, 200, 50),
            background()->GetFilledBackgroundBounds(*owner.get()));
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0),
            background()->GetGradientBounds(*owner.get()));

  // The background artwork image will have an aspect ratio of 2:1.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(20, 10);
  bitmap.eraseColor(SK_ColorWHITE);
  background()->UpdateArtwork(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));

  // The artwork width will be 2x the height of the notification and the visible
  // width will be limited to 10% the width of the notification.
  EXPECT_EQ(100, background()->GetArtworkWidth(bounds.size()));
  EXPECT_EQ(20, background()->GetArtworkVisibleWidth(bounds.size()));

  // Update the visible width % to be greater than the width of the image.
  background()->UpdateArtworkMaxWidthPct(0.6);
  EXPECT_EQ(100, background()->GetArtworkVisibleWidth(bounds.size()));

  // Check the artwork is positioned to the right.
  EXPECT_EQ(gfx::Rect(IsRTL() ? 0 : 100, 0, 100, 50),
            background()->GetArtworkBounds(*owner.get()));

  // Check the filled background is to the left of the image.
  EXPECT_EQ(gfx::Rect(IsRTL() ? 100 : 0, 0, 100, 50),
            background()->GetFilledBackgroundBounds(*owner.get()));

  // Check the gradient is positioned above the artwork.
  const gfx::Rect gradient_bounds =
      background()->GetGradientBounds(*owner.get());
  EXPECT_EQ(gfx::Rect(IsRTL() ? 60 : 100, 0, 40, 50), gradient_bounds);

  // Check the gradient point X-values are the start and end of
  // |gradient_bounds|.
  EXPECT_EQ(100, background()->GetGradientStartPoint(gradient_bounds).x());
  EXPECT_EQ(IsRTL() ? 60 : 140,
            background()->GetGradientEndPoint(gradient_bounds).x());

  // Check both of the gradient point Y-values are half the height.
  EXPECT_EQ(25, background()->GetGradientStartPoint(gradient_bounds).y());
  EXPECT_EQ(25, background()->GetGradientEndPoint(gradient_bounds).y());
}

}  // namespace media_message_center
