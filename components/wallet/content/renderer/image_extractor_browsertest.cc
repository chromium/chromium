// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/content/renderer/image_extractor.h"

#include <memory>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "content/public/test/render_view_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"

namespace wallet {

namespace {

// Helper to create a simple bitmap for testing.
SkBitmap CreateTestBitmap(int width, int height) {
  SkBitmap bitmap;
  SkAlphaType alpha_type = kOpaque_SkAlphaType;
  SkImageInfo info =
      SkImageInfo::Make(width, height, kBGRA_8888_SkColorType, alpha_type);
  bitmap.allocPixels(info);
  bitmap.eraseColor(SkColors::kBlack);
  return bitmap;
}

// Helper to encode a bitmap to PNG data.
scoped_refptr<base::RefCountedBytes> EncodeAsPNG(const SkBitmap& bitmap) {
  std::optional<std::vector<uint8_t>> encoded_data =
      gfx::PNGCodec::EncodeBGRASkBitmap(bitmap,
                                        /*discard_transparency=*/false);

  if (!encoded_data.has_value()) {
    DLOG(ERROR) << "Failed to encode bitmap as PNG.";
    return nullptr;  // Return null if encoding failed
  }

  // If encoding was successful, wrap the std::vector<uint8_t> into
  // RefCountedBytes.
  return base::MakeRefCounted<base::RefCountedBytes>(std::move(*encoded_data));
}

// Helper to encode a bitmap to a base64 data URI.
std::string EncodeBitmapToDataURI(const SkBitmap& bitmap) {
  auto png_data = EncodeAsPNG(bitmap);
  if (!png_data) {
    return std::string();  // Return empty string if encoding failed
  }
  return "data:image/png;base64," + base::Base64Encode(*png_data);
}

}  // namespace

class ImageExtractorBrowserTest : public content::RenderViewTest {
 public:
  ImageExtractorBrowserTest() = default;
  ~ImageExtractorBrowserTest() override = default;

  void SetUp() override {
    content::RenderViewTest::SetUp();
    base::DiscardableMemoryAllocator::SetInstance(&allocator_);
    // The ImageExtractor's lifetime is tied to the RenderFrame by being
    // stored as UserData.
    ImageExtractor::Create(GetMainRenderFrame(), &registry_);
    registry_.BindInterface(
        wallet::mojom::ImageExtractor::Name_,
        image_extractor_remote_.BindNewPipeAndPassReceiver().PassPipe());
    DCHECK(image_extractor_remote_.is_bound());
  }

  void TearDown() override {
    base::DiscardableMemoryAllocator::SetInstance(nullptr);
    content::RenderViewTest::TearDown();
  }

 protected:
  // A DiscardableMemoryAllocator is needed for certain Skia operations.
  // It is instantiated as a member variable to ensure its lifetime spans the
  // entire test, from SetUp() where it's set as the global instance, to
  // TearDown() where it's unset.
  base::TestDiscardableMemoryAllocator allocator_;
  service_manager::BinderRegistry registry_;
  mojo::Remote<mojom::ImageExtractor> image_extractor_remote_;
};

// This test loads HTML containing two images and verifies that the
// ImageExtractor correctly extracts both SkBitmaps and their content.
TEST_F(ImageExtractorBrowserTest, ExtractBase64ImageElements) {
  // Extract HTML with two images encoded as data URIs.
  const SkBitmap black_bitmap = CreateTestBitmap(20, 20);
  black_bitmap.eraseColor(SkColors::kBlack);
  const SkBitmap white_bitmap = CreateTestBitmap(30, 60);
  white_bitmap.eraseColor(SkColors::kWhite);

  const std::string html_string = base::StringPrintf(
      "<!DOCTYPE html>"
      "<body>"
      "<p>Some text</p>"
      "<img id='black' src='%s'>"
      "<img id='white' src='%s'>"
      "</body>",
      EncodeBitmapToDataURI(black_bitmap).c_str(),
      EncodeBitmapToDataURI(white_bitmap).c_str());

  LoadHTML(html_string);

  base::RunLoop run_loop;
  image_extractor_remote_->ExtractImages(
      base::BindLambdaForTesting([&](const std::vector<SkBitmap>& images) {
        // Verify that exactly two images were found.
        ASSERT_EQ(2u, images.size());

        // Check the properties of the first image.
        ASSERT_FALSE(images[0].isNull());
        EXPECT_EQ(20, images[0].width());
        EXPECT_EQ(20, images[0].height());
        EXPECT_EQ(SK_ColorBLACK, images[0].getColor(0, 0));

        // Check the properties of the second image.
        ASSERT_FALSE(images[1].isNull());
        EXPECT_EQ(30, images[1].width());
        EXPECT_EQ(60, images[1].height());
        EXPECT_EQ(SK_ColorWHITE, images[1].getColor(0, 0));

        run_loop.Quit();
      }));
  run_loop.Run();
}

// This test verifies that the ImageExtractor correctly filters out images that
// are too small.
TEST_F(ImageExtractorBrowserTest, FilterSmallImages) {
  // Load HTML with 1x1 and 2x2 pixel images.
  // One is a GIF and the other is a PNG to ensure different formats are
  // handled.
  static constexpr std::string_view kHtml =
      "<html>"
      "  <body>"
      "    <img id=\"red_1x1\" src=\"data:image/gif;base64,"
      "R0lGODlhAQABAIAAAP8AAAAAACwAAAAAAQABAAACAkQBADs=\">"
      "    <img id=\"blue_2x2\" src=\"data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAEElEQVR4nGNgYPj/"
      "H4KhDAA/0gf5tBJPzQAAAABJRU5ErkJggg==\">"
      "  </body>"
      "</html>";
  LoadHTML(kHtml);

  base::RunLoop run_loop;
  image_extractor_remote_->ExtractImages(
      base::BindLambdaForTesting([&](const std::vector<SkBitmap>& images) {
        // Verify that the returned vector is empty.
        EXPECT_TRUE(images.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// This test verifies that the ImageExtractor correctly filters out images with
// extreme aspect ratios (e.g., very tall or very wide).
TEST_F(ImageExtractorBrowserTest, FilterExtremeAspectRatioImages) {
  // Load HTML with two images with extreme aspect ratio.
  const SkBitmap thin_bitmap = CreateTestBitmap(20, 1000);
  const SkBitmap fat_bitmap = CreateTestBitmap(1000, 20);

  const std::string html_string = base::StringPrintf(
      "<!DOCTYPE html>"
      "<body>"
      "<p>Some text</p>"
      "<img id='thin' src='%s'>"
      "<img id='fat' src='%s'>"
      "</body>",
      EncodeBitmapToDataURI(thin_bitmap).c_str(),
      EncodeBitmapToDataURI(fat_bitmap).c_str());

  LoadHTML(html_string);

  base::RunLoop run_loop;
  image_extractor_remote_->ExtractImages(
      base::BindLambdaForTesting([&](const std::vector<SkBitmap>& images) {
        // Verify that the returned vector is empty.
        ASSERT_TRUE(images.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// This test verifies that the ImageExtractor respects the hard limit of 10
// images, even if more are present on the page.
TEST_F(ImageExtractorBrowserTest, RespectsImageLimit) {
  std::string html_string = "<!DOCTYPE html><body>";
  const SkBitmap bitmap = CreateTestBitmap(20, 20);
  const std::string data_uri = EncodeBitmapToDataURI(bitmap);

  for (int i = 0; i < 15; ++i) {
    html_string += base::StringPrintf("<img src='%s'>", data_uri.c_str());
  }
  html_string += "</body>";

  LoadHTML(html_string);

  base::RunLoop run_loop;
  image_extractor_remote_->ExtractImages(
      base::BindLambdaForTesting([&](const std::vector<SkBitmap>& images) {
        ASSERT_EQ(10u, images.size());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// This test verifies that the callback is correctly invoked with an empty
// vector when the document contains no images.
TEST_F(ImageExtractorBrowserTest, NoImagesOnPage) {
  // Load HTML without any <img> elements.
  static constexpr std::string_view kHtml = R"HTML(
      <html>
        <body>
          <p>This document has no images.</p>
        </body>
      </html>
    )HTML";
  LoadHTML(kHtml);

  base::RunLoop run_loop;
  image_extractor_remote_->ExtractImages(
      base::BindLambdaForTesting([&](const std::vector<SkBitmap>& images) {
        // Verify that the returned vector is empty.
        ASSERT_TRUE(images.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace wallet
