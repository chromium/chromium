// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/core_tab_helper.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_metadata.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libwebp/src/src/webp/decode.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

class CoreTabHelperImageProcessingTest
    : public ChromeRenderViewHostTestHarness {
 public:
  CoreTabHelperImageProcessingTest() : helper_(nullptr) {}

  CoreTabHelperImageProcessingTest(const CoreTabHelperImageProcessingTest&) =
      delete;
  CoreTabHelperImageProcessingTest& operator=(
      const CoreTabHelperImageProcessingTest&) = delete;

  ~CoreTabHelperImageProcessingTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    CoreTabHelper::CreateForWebContents(web_contents());
    helper_ = CoreTabHelper::FromWebContents(web_contents());
  }

  void DownscaleAndEncodeBitmapAndVerifyResponse(
      SkBitmap& bitmap,
      int thumbnail_min_size,
      int thumbnail_max_width,
      int thumbnail_max_height,
      std::string expected_content_type,
      int expected_downscaled_width,
      int expected_downscaled_height) {
    gfx::Size expected_downscaled_size =
        gfx::Size(expected_downscaled_width, expected_downscaled_height);

    auto callback =
        [](std::vector<unsigned char>* response_thumbnail_data,
           std::string* response_content_type,
           gfx::Size* response_original_size,
           gfx::Size* response_downscaled_size, int* response_log_data_size,
           base::OnceClosure quit,
           const std::vector<unsigned char>& received_thumbnail_data,
           const std::string& received_content_type,
           const gfx::Size& received_original_size,
           const gfx::Size& received_downscaled_size,
           const std::vector<lens::mojom::LatencyLogPtr> received_log_data) {
          *response_thumbnail_data = received_thumbnail_data;
          *response_original_size = received_original_size;
          *response_downscaled_size = received_downscaled_size;
          *response_content_type = received_content_type;
          *response_log_data_size = received_log_data.size();
          std::move(quit).Run();
        };

    std::vector<unsigned char> thumbnail_data;
    std::string content_type;
    gfx::Size original_size;
    gfx::Size downscaled_size;
    int log_data_size;
    base::RunLoop run_loop;
    helper_->DownscaleAndEncodeBitmap(
        bitmap, thumbnail_min_size, thumbnail_max_width, thumbnail_max_height,
        base::BindOnce(callback, &thumbnail_data, &content_type, &original_size,
                       &downscaled_size, &log_data_size,
                       run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(downscaled_size, expected_downscaled_size);
    EXPECT_EQ(content_type, expected_content_type);

    if (bitmap.width() == expected_downscaled_width &&
        bitmap.height() == expected_downscaled_height) {
      // Only encoding steps start and end steps should be logged.
      EXPECT_EQ(log_data_size, 2);
    } else {
      // Encoding and downscaling start and end steps should be logged.
      EXPECT_EQ(log_data_size, 4);
    }

    if (content_type == "image/jpeg") {
      SkBitmap decoded_bitmap = *gfx::JPEGCodec::Decode(&thumbnail_data.front(),
                                                        thumbnail_data.size())
                                     .get();
      ASSERT_EQ(expected_downscaled_width, decoded_bitmap.width());
      ASSERT_EQ(expected_downscaled_height, decoded_bitmap.height());
    } else if (content_type == "image/webp") {
      int width;
      int height;
      EXPECT_TRUE(WebPGetInfo(&thumbnail_data.front(), thumbnail_data.size(),
                              &width, &height));
      ASSERT_EQ(expected_downscaled_width, width);
      ASSERT_EQ(expected_downscaled_height, height);
    }
  }

 private:
  raw_ptr<CoreTabHelper, DanglingUntriaged> helper_;
};

}  // namespace

TEST_F(CoreTabHelperImageProcessingTest,
       DownscaleAndEncodeBitmap_EncodesOpaqueAsJpeg) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100, /*isOpaque=*/true);
  DownscaleAndEncodeBitmapAndVerifyResponse(
      bitmap, /*thumbnail_min_size=*/1, /*thumbnail_max_width=*/100,
      /*thumbnail_max_height=*/100, "image/jpeg",
      /*expected_downscaled_width=*/100, /*expected_downscaled_height=*/100);
}

TEST_F(CoreTabHelperImageProcessingTest,
       DownscaleAndEncodeBitmap_EncodesNonOpaqueAsWebp) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100, /*isOpaque=*/false);
  DownscaleAndEncodeBitmapAndVerifyResponse(
      bitmap, /*thumbnail_min_size=*/1, /*thumbnail_max_width=*/100,
      /*thumbnail_max_height=*/100, "image/webp",
      /*expected_downscaled_width=*/100, /*expected_downscaled_height=*/100);
}

TEST_F(CoreTabHelperImageProcessingTest,
       DownscaleAndEncodeBitmap_DownscalesLargeImage) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(300, 300, /*isOpaque=*/false);
  DownscaleAndEncodeBitmapAndVerifyResponse(
      bitmap, /*thumbnail_min_size=*/1, /*thumbnail_max_width=*/100,
      /*thumbnail_max_height=*/100, "image/webp",
      /*expected_downscaled_width=*/100, /*expected_downscaled_height=*/100);
}

TEST_F(CoreTabHelperImageProcessingTest,
       DownscaleAndEncodeBitmap_DoesNotDownscaleThinImage) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 300, /*isOpaque=*/false);
  DownscaleAndEncodeBitmapAndVerifyResponse(
      bitmap, /*thumbnail_min_size=*/100 * 100, /*thumbnail_max_width=*/100,
      /*thumbnail_max_height=*/100, "image/webp",
      /*expected_downscaled_width=*/1, /*expected_downscaled_height=*/300);
}

TEST(CoreTabHelperUnitTest, EncodeImageIntoSearchArgs_EncodesAsJpeg) {
  gfx::Image image = gfx::test::CreateImage(100, 100);
  TemplateURLRef::SearchTermsArgs search_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());

  size_t encoded_image_size_bytes;
  lens::mojom::ImageFormat image_format =
      CoreTabHelper::EncodeImageIntoSearchArgs(image, encoded_image_size_bytes,
                                               search_args);

  EXPECT_FALSE(search_args.image_thumbnail_content.empty());
  EXPECT_EQ("image/jpeg", search_args.image_thumbnail_content_type);
  EXPECT_EQ(359ul, encoded_image_size_bytes);
  EXPECT_EQ(lens::mojom::ImageFormat::JPEG, image_format);
}

TEST(CoreTabHelperUnitTest,
     EncodeImageIntoSearchArgs_JpegEncodingFails_EncodesAsPng) {
  gfx::Image image = gfx::test::CreateImage(0, 0);  // Encoding 0x0 will fail
  TemplateURLRef::SearchTermsArgs search_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());

  size_t encoded_image_size_bytes;
  lens::mojom::ImageFormat image_format =
      CoreTabHelper::EncodeImageIntoSearchArgs(image, encoded_image_size_bytes,
                                               search_args);

  EXPECT_EQ("image/png", search_args.image_thumbnail_content_type);
  EXPECT_EQ(0ul, encoded_image_size_bytes);
  EXPECT_EQ(lens::mojom::ImageFormat::PNG, image_format);
}
