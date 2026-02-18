// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/surface_embed/browser/surface_embed_host.h"
#include "components/surface_embed/common/features.h"
#include "components/surface_embed/common/surface_embed.mojom.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace surface_embed {

namespace {
constexpr std::string_view kTestUrl = "/surface_embed/embed_tag.html";
constexpr size_t kSingleEmbedCount = 1;
constexpr float kTestDeviceScaleFactor = 1.5f;

// Helper class for tracking SurfaceEmbedHost instances.
class SurfaceEmbedHostTracker {
 public:
  SurfaceEmbedHostTracker() = default;

  SurfaceEmbedHostTracker(const SurfaceEmbedHostTracker&) = delete;
  SurfaceEmbedHostTracker& operator=(const SurfaceEmbedHostTracker&) = delete;

  ~SurfaceEmbedHostTracker() = default;

  void AddHost(SurfaceEmbedHost* host) { hosts_.push_back(host); }

  void RemoveHost(SurfaceEmbedHost* host) { std::erase(hosts_, host); }

  SurfaceEmbedHost* GetHost(size_t index) const {
    if (index < hosts_.size()) {
      return hosts_[index];
    }
    return nullptr;
  }

  size_t GetHostCount() const { return hosts_.size(); }

 private:
  std::vector<raw_ptr<SurfaceEmbedHost>> hosts_;
};

class SurfaceEmbedTestContentBrowserClient
    : public content::ContentBrowserTestContentBrowserClient {
 public:
  explicit SurfaceEmbedTestContentBrowserClient(
      SurfaceEmbedHostTracker* tracker)
      : tracker_(tracker) {}
  ~SurfaceEmbedTestContentBrowserClient() override = default;

  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override {
    map->Add<mojom::SurfaceEmbedHost>(base::BindRepeating(
        [](SurfaceEmbedHostTracker* tracker,
           content::RenderFrameHost* render_frame_host,
           mojo::PendingReceiver<mojom::SurfaceEmbedHost> receiver) {
          SurfaceEmbedHost* host =
              SurfaceEmbedHost::Create(render_frame_host, std::move(receiver));
          host->SetDestructionCallbackForTesting(
              base::BindOnce(&SurfaceEmbedHostTracker::RemoveHost,
                             base::Unretained(tracker), host));
          tracker->AddHost(host);
        },
        base::Unretained(tracker_)));
  }

 private:
  raw_ptr<SurfaceEmbedHostTracker> tracker_;
};

}  // namespace

class SurfaceEmbedBrowserTest : public content::ContentBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kSurfaceEmbed);
    content::ContentBrowserTest::SetUp();
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    content::ContentBrowserTest::CreatedBrowserMainParts(browser_main_parts);
    test_browser_client_ =
        std::make_unique<SurfaceEmbedTestContentBrowserClient>(&tracker_);
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

  void NavigateToTestUrl(std::string_view url) {
    const GURL test_url = embedded_test_server()->GetURL(url);
    ASSERT_TRUE(content::NavigateToURL(web_contents(), test_url));
  }

  int CountEmbedElementsInPage() {
    return content::EvalJs(web_contents(), "document.embeds.length")
        .ExtractInt();
  }

  void WaitForHostCount(size_t expected_count) {
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return GetHostCount() == expected_count; }));
  }

  SurfaceEmbedHost* GetHost(size_t index) { return tracker_.GetHost(index); }

  size_t GetHostCount() const { return tracker_.GetHostCount(); }

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
  SurfaceEmbedHostTracker tracker_;
  std::unique_ptr<SurfaceEmbedTestContentBrowserClient> test_browser_client_;
};

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, EmbedTagCreatesPlugin) {
  NavigateToTestUrl(kTestUrl);
  EXPECT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());

  // Verify that the host is created.
  WaitForHostCount(kSingleEmbedCount);
  ASSERT_EQ(kSingleEmbedCount, GetHostCount());
  SurfaceEmbedHost* host = GetHost(0);
  ASSERT_NE(nullptr, host);

  // Expect the stub plugin code to render a red square.
  EXPECT_TRUE(CheckHasPixelInColor(SK_ColorRED));
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, EmbedPixelTest) {
  NavigateToTestUrl(kTestUrl);

  EXPECT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());

  // Verify that the host is created.
  WaitForHostCount(kSingleEmbedCount);
  ASSERT_EQ(kSingleEmbedCount, GetHostCount());
  SurfaceEmbedHost* host = GetHost(0);
  ASSERT_NE(nullptr, host);

  content::RenderWidgetHostView* const view =
      web_contents()->GetRenderWidgetHostView();
  ASSERT_TRUE(view);

  // The embed element is at 10,10 with size 100x100 in embed_tag.html.
  const gfx::Rect embed_bounds(10, 10, 100, 100);

  // Capture a snapshot of the view containing the embed and a bit of
  // surrounding area.
  const gfx::Rect snapshot_bounds(0, 0, embed_bounds.right() + 10,
                                  embed_bounds.bottom() + 10);

  // Scale the bounds by device scale factor since we capture the whole view.
  gfx::Rect scaled_snapshot_bounds =
      gfx::ScaleToEnclosingRect(snapshot_bounds, kTestDeviceScaleFactor);
  gfx::Rect scaled_embed_bounds =
      gfx::ScaleToEnclosingRect(embed_bounds, kTestDeviceScaleFactor);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    SkBitmap bitmap = TakeScreenshot(snapshot_bounds);

    if (bitmap.width() != scaled_snapshot_bounds.width() ||
        bitmap.height() != scaled_snapshot_bounds.height()) {
      return false;
    }

    // Check a pixel inside the embed element.
    if (bitmap.getColor(scaled_embed_bounds.x() + 1,
                        scaled_embed_bounds.y() + 1) != SK_ColorRED) {
      return false;
    }

    // Check a pixel outside the embed element.
    if (bitmap.getColor(1, 1) != SK_ColorWHITE) {
      return false;
    }

    return true;
  }));
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest,
                       EmbedMultipleTagsCreatesMultiplePlugins) {
  constexpr char kMultipleEmbedsTestUrl[] =
      "/surface_embed/multiple_embed_tags.html";
  constexpr size_t kMultipleEmbedCount = 2;

  NavigateToTestUrl(kMultipleEmbedsTestUrl);
  EXPECT_EQ(kMultipleEmbedCount, CountEmbedElementsInPage());

  // Verify that the hosts are created.
  WaitForHostCount(kMultipleEmbedCount);
  ASSERT_EQ(kMultipleEmbedCount, GetHostCount());

  for (size_t i = 0; i < kMultipleEmbedCount; ++i) {
    SurfaceEmbedHost* host = GetHost(i);
    ASSERT_NE(nullptr, host);
  }

  // Expect the stub plugin code to render a red square.
  EXPECT_TRUE(CheckHasPixelInColor(SK_ColorRED));
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, EmbedTagRemovedDestroysHost) {
  NavigateToTestUrl(kTestUrl);
  EXPECT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());

  // Verify that the host is created.
  WaitForHostCount(kSingleEmbedCount);
  ASSERT_EQ(kSingleEmbedCount, GetHostCount());

  // Remove the embed element from the page.
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              "document.querySelector('embed').remove();"));

  // Verify that the host is destroyed.
  WaitForHostCount(0);
  EXPECT_EQ(0u, GetHostCount());
}

}  // namespace surface_embed
