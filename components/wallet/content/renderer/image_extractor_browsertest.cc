// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/content/renderer/image_extractor.h"

#include <memory>
#include <string_view>
#include <vector>

#include "base/run_loop.h"
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

namespace wallet {

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
        // Verify that exactly two images were found.
        ASSERT_EQ(2u, images.size());

        // Check the properties and content of the first image (red).
        ASSERT_FALSE(images[0].isNull());
        EXPECT_EQ(1, images[0].width());
        EXPECT_EQ(1, images[0].height());
        EXPECT_EQ(SK_ColorRED, images[0].getColor(0, 0));

        // Check the properties and content of the second image (blue).
        ASSERT_FALSE(images[1].isNull());
        EXPECT_EQ(2, images[1].width());
        EXPECT_EQ(2, images[1].height());
        EXPECT_EQ(SK_ColorBLUE, images[1].getColor(0, 0));
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
