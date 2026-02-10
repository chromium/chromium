// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/surface_embed/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace surface_embed {

namespace {
constexpr char kSimplePageWithEmbedTagTestUrl[] =
    "/surface_embed/embed_tag.html";
constexpr size_t kSingleEmbedCount = 1;
constexpr float kTestDeviceScaleFactor = 1.5f;
}  // namespace

class SurfaceEmbedBrowserTest : public content::ContentBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kSurfaceEmbed);
    content::ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    // Enable pixel output in tests to allow CopyFromSurface to capture actual
    // rendered content instead of returning empty/black bitmaps.
    // Note that we force a device scale factor of 1.5 to also test scaling of
    // the surface embed plugin.
    EnablePixelOutput(kTestDeviceScaleFactor);
  }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* web_contents() { return shell()->web_contents(); }

  void NavigateToTestUrl(const char* url) {
    const GURL test_url = embedded_test_server()->GetURL(url);
    ASSERT_TRUE(content::NavigateToURL(web_contents(), test_url));
  }

  int CountEmbedElementsInPage() {
    return content::EvalJs(web_contents(), "document.embeds.length")
        .ExtractInt();
  }

 protected:
  // Take a screenshot of the given rectangle area of the main web contents.
  // Empty rectangle captures the full area.
  SkBitmap TakeScreenshot(const gfx::Rect& capture_rect) {
    base::test::TestFuture<const content::CopyFromSurfaceResult&> future_bitmap;
    web_contents()->GetRenderWidgetHostView()->CopyFromSurface(
        capture_rect, gfx::Size(), base::TimeDelta(),
        future_bitmap.GetCallback());
    return future_bitmap.Take()
        .value_or(viz::CopyOutputBitmapWithMetadata())
        .bitmap;
  }

  // Check if the given color is rendered.
  bool CheckHasPixelInColor(SkColor target_color) {
    content::WaitForCopyableViewInWebContents(web_contents());
    auto bitmap = TakeScreenshot(gfx::Rect());
    for (int x = 0; x < bitmap.width(); ++x) {
      for (int y = 0; y < bitmap.height(); ++y) {
        if (bitmap.getColor(x, y) == target_color) {
          return true;
        }
      }
    }
    return false;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, EmbedTagCreatesPlugin) {
  NavigateToTestUrl(kSimplePageWithEmbedTagTestUrl);
  EXPECT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());

  // Expect the stub plugin code to render a red square.
  EXPECT_TRUE(CheckHasPixelInColor(SK_ColorRED));
}

}  // namespace surface_embed
