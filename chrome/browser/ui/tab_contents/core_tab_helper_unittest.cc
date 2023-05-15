// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/core_tab_helper.h"

#include "base/test/scoped_feature_list.h"
#include "components/lens/lens_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

TEST(CoreTabHelperUnitTest,
     EncodeImageIntoSearchArgs_OptimizedImageFormatsDisabled_EncodesAsPng) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(lens::features::kLensImageFormatOptimizations);
  gfx::Image image = gfx::test::CreateImage(100, 100);
  TemplateURLRef::SearchTermsArgs search_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());

  lens::mojom::ImageFormat image_format =
      CoreTabHelper::EncodeImageIntoSearchArgs(image, search_args);

  EXPECT_FALSE(search_args.image_thumbnail_content.empty());
  EXPECT_EQ("image/png", search_args.image_thumbnail_content_type);
  EXPECT_EQ(lens::mojom::ImageFormat::PNG, image_format);
}

TEST(CoreTabHelperUnitTest,
     EncodeImageIntoSearchArgs_WebpEnabledAndEncodingSucceeds_EncodesAsWebp) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      lens::features::kLensImageFormatOptimizations,
      {{"use-webp-region-search", "true"},
       {"use-jpeg-region-search", "false"}});
  gfx::Image image = gfx::test::CreateImage(100, 100);
  TemplateURLRef::SearchTermsArgs search_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());

  lens::mojom::ImageFormat image_format =
      CoreTabHelper::EncodeImageIntoSearchArgs(image, search_args);

  EXPECT_FALSE(search_args.image_thumbnail_content.empty());
  EXPECT_EQ("image/webp", search_args.image_thumbnail_content_type);
  EXPECT_EQ(lens::mojom::ImageFormat::WEBP, image_format);
}

TEST(CoreTabHelperUnitTest,
     EncodeImageIntoSearchArgs_WebpEnabledAndEncodingFails_EncodesAsPng) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      lens::features::kLensImageFormatOptimizations,
      {{"use-webp-region-search", "true"},
       {"use-jpeg-region-search", "false"}});
  gfx::Image image = gfx::test::CreateImage(0, 0);  // Encoding 0x0 will fail
  TemplateURLRef::SearchTermsArgs search_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());

  lens::mojom::ImageFormat image_format =
      CoreTabHelper::EncodeImageIntoSearchArgs(image, search_args);

  EXPECT_EQ("image/png", search_args.image_thumbnail_content_type);
  EXPECT_EQ(lens::mojom::ImageFormat::PNG, image_format);
}

TEST(CoreTabHelperUnitTest,
     EncodeImageIntoSearchArgs_JpegEnabledAndEncodingSucceeds_EncodesAsJpeg) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      lens::features::kLensImageFormatOptimizations,
      {{"use-webp-region-search", "false"},
       {"use-jpeg-region-search", "true"}});
  gfx::Image image = gfx::test::CreateImage(100, 100);
  TemplateURLRef::SearchTermsArgs search_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());

  lens::mojom::ImageFormat image_format =
      CoreTabHelper::EncodeImageIntoSearchArgs(image, search_args);

  EXPECT_FALSE(search_args.image_thumbnail_content.empty());
  EXPECT_EQ("image/jpeg", search_args.image_thumbnail_content_type);
  EXPECT_EQ(lens::mojom::ImageFormat::JPEG, image_format);
}

TEST(CoreTabHelperUnitTest,
     EncodeImageIntoSearchArgs_JpegEnabledAndEncodingFails_EncodesAsPng) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      lens::features::kLensImageFormatOptimizations,
      {{"use-webp-region-search", "false"},
       {"use-jpeg-region-search", "true"}});
  gfx::Image image = gfx::test::CreateImage(0, 0);  // Encoding 0x0 will fail
  TemplateURLRef::SearchTermsArgs search_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());

  lens::mojom::ImageFormat image_format =
      CoreTabHelper::EncodeImageIntoSearchArgs(image, search_args);

  EXPECT_EQ("image/png", search_args.image_thumbnail_content_type);
  EXPECT_EQ(lens::mojom::ImageFormat::PNG, image_format);
}
