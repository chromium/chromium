// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/icons/trusted_icon_filter.h"

#include "build/build_config.h"
#include "components/webapps/browser/installable/installable_evaluator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace web_app {

namespace {

using IconPurpose = blink::mojom::ManifestImageResource_Purpose;

blink::Manifest::ImageResource CreateIcon(
    const GURL& src,
    const std::vector<gfx::Size>& sizes,
    const std::vector<IconPurpose>& purposes) {
  blink::Manifest::ImageResource icon;
  icon.src = src;
  icon.sizes = sizes;
  icon.purpose = purposes;
  return icon;
}

}  // namespace


TEST(TrustedIconFilterTest, NoIcons) {
  std::vector<blink::Manifest::ImageResource> icons;
  EXPECT_FALSE(GetTrustedIconsFromManifest(icons).has_value());
}

TEST(TrustedIconFilterTest, NoSquareIcons) {
  std::vector<blink::Manifest::ImageResource> icons{
      CreateIcon(GURL("http://example.com/icon.png"), {gfx::Size(128, 256)},
                 {IconPurpose::ANY}),
  };
  EXPECT_FALSE(GetTrustedIconsFromManifest(icons).has_value());
}

TEST(TrustedIconFilterTest, LargestAnyIconSelected) {
  std::vector<blink::Manifest::ImageResource> icons{
      CreateIcon(GURL("http://example.com/icon1.png"), {gfx::Size(128, 128)},
                 {IconPurpose::ANY}),
      CreateIcon(GURL("http://example.com/icon2.png"), {gfx::Size(256, 256)},
                 {IconPurpose::ANY}),
  };
  auto icon_info = GetTrustedIconsFromManifest(icons);
  ASSERT_TRUE(icon_info.has_value());
  EXPECT_EQ(icon_info->url, GURL("http://example.com/icon2.png"));
  EXPECT_EQ(icon_info->square_size_px, 256);
  EXPECT_EQ(icon_info->purpose, apps::IconInfo::Purpose::kAny);
}

TEST(TrustedIconFilterTest, MaskableIconSelection) {
  std::vector<blink::Manifest::ImageResource> icons{
      CreateIcon(GURL("http://example.com/any.png"), {gfx::Size(512, 512)},
                 {IconPurpose::ANY}),
      CreateIcon(GURL("http://example.com/maskable.png"), {gfx::Size(256, 256)},
                 {IconPurpose::MASKABLE}),
  };
  auto icon_info = GetTrustedIconsFromManifest(icons);
  ASSERT_TRUE(icon_info.has_value());

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(icon_info->url, GURL("http://example.com/maskable.png"));
  EXPECT_EQ(icon_info->square_size_px, 256);
  EXPECT_EQ(icon_info->purpose, apps::IconInfo::Purpose::kMaskable);
#else
  EXPECT_EQ(icon_info->url, GURL("http://example.com/any.png"));
  EXPECT_EQ(icon_info->square_size_px, 512);
  EXPECT_EQ(icon_info->purpose, apps::IconInfo::Purpose::kAny);
#endif
}

TEST(TrustedIconFilterTest, MaskableIconTooSmall) {
  std::vector<blink::Manifest::ImageResource> icons{
      CreateIcon(GURL("http://example.com/any.png"), {gfx::Size(512, 512)},
                 {IconPurpose::ANY}),
      CreateIcon(GURL("http://example.com/maskable.png"), {gfx::Size(128, 128)},
                 {IconPurpose::MASKABLE}),
  };
  auto icon_info = GetTrustedIconsFromManifest(icons);
  ASSERT_TRUE(icon_info.has_value());
  // On all platforms, the 'any' icon should be chosen because the maskable one
  // is too small.
  EXPECT_EQ(icon_info->url, GURL("http://example.com/any.png"));
  EXPECT_EQ(icon_info->square_size_px, 512);
  EXPECT_EQ(icon_info->purpose, apps::IconInfo::Purpose::kAny);
}

TEST(TrustedIconFilterTest, OnlyMaskableIcon) {
  std::vector<blink::Manifest::ImageResource> icons{
      CreateIcon(GURL("http://example.com/maskable.png"), {gfx::Size(256, 256)},
                 {IconPurpose::MASKABLE}),
  };
  auto icon_info = GetTrustedIconsFromManifest(icons);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  ASSERT_TRUE(icon_info.has_value());
  EXPECT_EQ(icon_info->url, GURL("http://example.com/maskable.png"));
  EXPECT_EQ(icon_info->square_size_px, 256);
  EXPECT_EQ(icon_info->purpose, apps::IconInfo::Purpose::kMaskable);
#else
  // On other platforms, no 'any' icon means no icon is chosen from bitmaps,
  // and there is no SVG fallback.
  EXPECT_FALSE(icon_info.has_value());
#endif
}

TEST(TrustedIconFilterTest, SvgFallbackAny) {
  std::vector<blink::Manifest::ImageResource> icons{
      CreateIcon(GURL("http://example.com/icon.svg"), {gfx::Size()},
                 {IconPurpose::ANY}),
  };
  auto icon_info = GetTrustedIconsFromManifest(icons);
  ASSERT_TRUE(icon_info.has_value());
  EXPECT_EQ(icon_info->url, GURL("http://example.com/icon.svg"));
  EXPECT_EQ(icon_info->square_size_px, 1024);
  EXPECT_EQ(icon_info->purpose, apps::IconInfo::Purpose::kAny);
}

TEST(TrustedIconFilterTest, SvgFallbackMaskable) {
  std::vector<blink::Manifest::ImageResource> icons{
      CreateIcon(GURL("http://example.com/any.svg"), {gfx::Size()},
                 {IconPurpose::ANY}),
      CreateIcon(GURL("http://example.com/maskable.svg"), {gfx::Size()},
                 {IconPurpose::MASKABLE}),
  };
  auto icon_info = GetTrustedIconsFromManifest(icons);
  ASSERT_TRUE(icon_info.has_value());
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(icon_info->url, GURL("http://example.com/maskable.svg"));
  EXPECT_EQ(icon_info->square_size_px, 1024);
  EXPECT_EQ(icon_info->purpose, apps::IconInfo::Purpose::kMaskable);
#else
  EXPECT_EQ(icon_info->url, GURL("http://example.com/any.svg"));
  EXPECT_EQ(icon_info->square_size_px, 1024);
  EXPECT_EQ(icon_info->purpose, apps::IconInfo::Purpose::kAny);
#endif
}

TEST(TrustedIconFilterTest, BitmapPreferredOverSvg) {
  std::vector<blink::Manifest::ImageResource> icons{
      CreateIcon(GURL("http://example.com/any.png"), {gfx::Size(128, 128)},
                 {IconPurpose::ANY}),
      CreateIcon(GURL("http://example.com/any.svg"), {gfx::Size()},
                 {IconPurpose::ANY}),
  };
  auto icon_info = GetTrustedIconsFromManifest(icons);
  ASSERT_TRUE(icon_info.has_value());
  EXPECT_EQ(icon_info->url, GURL("http://example.com/any.png"));
  EXPECT_EQ(icon_info->square_size_px, 128);
  EXPECT_EQ(icon_info->purpose, apps::IconInfo::Purpose::kAny);
}

TEST(TrustedIconFilterTest, IconWithMultiplePurposes) {
  std::vector<blink::Manifest::ImageResource> icons{
      CreateIcon(GURL("http://example.com/any_maskable.png"),
                 {gfx::Size(256, 256)},
                 {IconPurpose::ANY, IconPurpose::MASKABLE}),
  };
  auto icon_info = GetTrustedIconsFromManifest(icons);
  ASSERT_TRUE(icon_info.has_value());
  EXPECT_EQ(icon_info->url, GURL("http://example.com/any_maskable.png"));
  EXPECT_EQ(icon_info->square_size_px, 256);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(icon_info->purpose, apps::IconInfo::Purpose::kMaskable);
#else
  EXPECT_EQ(icon_info->purpose, apps::IconInfo::Purpose::kAny);
#endif
}

// kMaximumIconSizeInPx is std::numeric_limits<int>::max() on Android, so we can
// only test a size larger than that on non-Android platforms.
#if !BUILDFLAG(IS_ANDROID)
TEST(TrustedIconFilterTest, IconTooLargeIsIgnored) {
  std::vector<blink::Manifest::ImageResource> icons{
      CreateIcon(GURL("http://example.com/small.png"), {gfx::Size(128, 128)},
                 {IconPurpose::ANY}),
      CreateIcon(
          GURL("http://example.com/large.png"),
          {gfx::Size(webapps::InstallableEvaluator::kMaximumIconSizeInPx + 1,
                     webapps::InstallableEvaluator::kMaximumIconSizeInPx + 1)},
          {IconPurpose::ANY}),
  };
  auto icon_info = GetTrustedIconsFromManifest(icons);
  ASSERT_TRUE(icon_info.has_value());
  EXPECT_EQ(icon_info->url, GURL("http://example.com/small.png"));
  EXPECT_EQ(icon_info->square_size_px, 128);
  EXPECT_EQ(icon_info->purpose, apps::IconInfo::Purpose::kAny);
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST(TrustedIconFilterTest, OnlyMonochromeIcon) {
  std::vector<blink::Manifest::ImageResource> icons{
      CreateIcon(GURL("http://example.com/icon.png"), {gfx::Size(256, 256)},
                 {IconPurpose::MONOCHROME}),
  };
  EXPECT_FALSE(GetTrustedIconsFromManifest(icons).has_value());
}

TEST(TrustedIconFilterTest, SvgWithEmptySizeButNotSvgUrl) {
  std::vector<blink::Manifest::ImageResource> icons{
      CreateIcon(GURL("http://example.com/icon.png"), {gfx::Size()},
                 {IconPurpose::ANY}),
  };
  auto icon_info = GetTrustedIconsFromManifest(icons);
  ASSERT_TRUE(icon_info.has_value());
  EXPECT_EQ(icon_info->url, GURL("http://example.com/icon.png"));
}

TEST(TrustedIconFilterTest, PngWithAnySizeIsIgnoredWithSvgFallback1) {
  std::vector<blink::Manifest::ImageResource> icons{
      CreateIcon(GURL("http://example.com/icon.png"), {gfx::Size()},
                 {IconPurpose::ANY}),
      CreateIcon(GURL("http://example.com/icon.svg"), {gfx::Size()},
                 {IconPurpose::ANY}),
  };
  auto icon_info = GetTrustedIconsFromManifest(icons);
  ASSERT_TRUE(icon_info.has_value());
  EXPECT_EQ(icon_info->url, GURL("http://example.com/icon.svg"));
}

TEST(TrustedIconFilterTest, PngWithAnySizeIsIgnoredWithSvgFallback2) {
  std::vector<blink::Manifest::ImageResource> icons{
      CreateIcon(GURL("http://example.com/icon.svg"), {gfx::Size()},
                 {IconPurpose::ANY}),
      CreateIcon(GURL("http://example.com/icon.png"), {gfx::Size()},
                 {IconPurpose::ANY})};
  auto icon_info = GetTrustedIconsFromManifest(icons);
  ASSERT_TRUE(icon_info.has_value());
  EXPECT_EQ(icon_info->url, GURL("http://example.com/icon.svg"));
}

TEST(TrustedIconFilterTest, SvgMaskableVersusPngWithIdealSize) {
  std::vector<blink::Manifest::ImageResource> icons{
      CreateIcon(GURL("http://example.com/maskable.svg"), {gfx::Size()},
                 {IconPurpose::MASKABLE}),
      CreateIcon(GURL("http://example.com/any.png"), {gfx::Size(256, 256)},
                 {IconPurpose::ANY})};
  auto icon_info = GetTrustedIconsFromManifest(icons);
  ASSERT_TRUE(icon_info.has_value());

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(icon_info->url, GURL("http://example.com/maskable.svg"));
  EXPECT_EQ(icon_info->square_size_px, 1024);
  EXPECT_EQ(icon_info->purpose, apps::IconInfo::Purpose::kMaskable);
#else
  EXPECT_EQ(icon_info->url, GURL("http://example.com/any.png"));
  EXPECT_EQ(icon_info->square_size_px, 256);
  EXPECT_EQ(icon_info->purpose, apps::IconInfo::Purpose::kAny);
#endif
}

}  // namespace web_app
