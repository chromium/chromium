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
#include "build/build_config.h"
#include "components/guest_contents/browser/guest_contents_handle.h"
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
constexpr std::string_view kAttachHarnessUrl =
    "/surface_embed/attach_harness.html";
constexpr std::string_view kEmptyUrl = "/surface_embed/empty.html";
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

  size_t GetAttachedHostCount() const {
    size_t attached_count = 0;
    for (auto host : hosts_) {
      if (host->IsAttachedForTesting()) {
        ++attached_count;
      }
    }
    return attached_count;
  }

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

  bool WaitForHostAttachment(size_t expected_count) {
    // After host creation, the attachment happens asynchronously when
    // SurfaceEmbedHost::AttachConnector() is called. Poll until the expected
    // number of hosts are attached.
    return base::test::RunUntil(
        [&]() { return GetAttachedHostCount() >= expected_count; });
  }

  void NavigateToAttachHarness() {
    const GURL harness_url = embedded_test_server()->GetURL(kAttachHarnessUrl);
    ASSERT_TRUE(NavigateToURL(web_contents(), harness_url));
  }

  std::unique_ptr<content::WebContents> CreateChildWebContents() {
    content::WebContents::CreateParams create_params(
        shell()->web_contents()->GetBrowserContext());
    std::unique_ptr<content::WebContents> child_contents =
        content::WebContents::Create(create_params);
    EXPECT_NE(child_contents, nullptr);
    return child_contents;
  }

  // Navigate child WebContents to a URL. Wait for load to complete.
  void NavigateChildToUrl(content::WebContents* child_contents,
                          const std::string_view& path) {
    const GURL child_url = embedded_test_server()->GetURL(path);
    ASSERT_TRUE(NavigateToURL(child_contents, child_url));
    ASSERT_TRUE(content::WaitForLoadStop(child_contents));
  }

  // Setup child with harness navigation and content loading.
  std::unique_ptr<content::WebContents> SetupHarnessAndChildWithContent(
      const std::string_view& child_path) {
    NavigateToAttachHarness();
    auto child_contents = CreateChildWebContents();
    NavigateChildToUrl(child_contents.get(), child_path);
    return child_contents;
  }

  // Setup child with harness navigation and load a blank html page.
  std::unique_ptr<content::WebContents> SetupHarnessAndChild() {
    return SetupHarnessAndChildWithContent(kEmptyUrl);
  }

  // Attach a child to an embed element and wait for SurfaceEmbedHost creation.
  void AttachChildToEmbed(content::WebContents* child_contents) {
    AttachChildToEmbedWithId(child_contents, std::nullopt);
  }

  // Attach a child to an embed element with an optional ID.
  void AttachChildToEmbedWithId(content::WebContents* child_contents,
                                std::optional<std::string> embed_id) {
    guest_contents::GuestContentsHandle* guest_handle =
        guest_contents::GuestContentsHandle::CreateForWebContents(
            child_contents);
    ASSERT_NE(guest_handle, nullptr);
    std::string script = "createEmbed('" + guest_handle->id().ToString();
    if (embed_id.has_value()) {
      script += "', '" + embed_id.value();
    }
    script += "');";
    size_t expected_attachments = GetAttachedHostCount() + 1;
    ASSERT_TRUE(content::ExecJs(web_contents(), script));
    ASSERT_TRUE(WaitForHostAttachment(expected_attachments));
    EXPECT_NE(child_contents->GetSurfaceEmbedConnector(), nullptr);
  }

  SurfaceEmbedHost* GetHost(size_t index) { return tracker_.GetHost(index); }

  size_t GetHostCount() const { return tracker_.GetHostCount(); }

  size_t GetAttachedHostCount() const {
    return tracker_.GetAttachedHostCount();
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
  SurfaceEmbedHostTracker tracker_;
  std::unique_ptr<SurfaceEmbedTestContentBrowserClient> test_browser_client_;
};

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, EmbedTagCreatesPlugin) {
  auto child_contents = SetupHarnessAndChild();
  AttachChildToEmbed(child_contents.get());

  EXPECT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());

  // Verify that the host is created.
  WaitForHostCount(kSingleEmbedCount);
  ASSERT_EQ(kSingleEmbedCount, GetHostCount());
  SurfaceEmbedHost* host = GetHost(0);
  ASSERT_NE(nullptr, host);

  // Expect the stub plugin code to render a red square.
  EXPECT_TRUE(CheckHasPixelInColor(SK_ColorRED));
}

// Make sure we don't crash on invalid content ID.
IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, EmbedTagInvalidContentId) {
  NavigateToAttachHarness();
  ASSERT_TRUE(content::ExecJs(web_contents(), "createEmbed('bad')"));
  EXPECT_EQ(1, CountEmbedElementsInPage());
  // Access an unknown property on the embed to force plugin creation
  // (since otherwise it's on a timer).
  EXPECT_EQ(
      base::Value(),
      content::EvalJs(web_contents(), "document.embeds[0].noSuchProperty"));
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, EmbedPixelTest) {
  auto child_contents = SetupHarnessAndChild();
  AttachChildToEmbed(child_contents.get());

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

  // On Android forcing device scale factor might not work for tests, therefore
  // query the actual scale factor from the view. On other platforms, verify it
  // matches the forced value.
  const float device_scale_factor = view->GetDeviceScaleFactor();
#if !BUILDFLAG(IS_ANDROID)
  ASSERT_FLOAT_EQ(kTestDeviceScaleFactor, device_scale_factor);
#endif
  // Scale the bounds by device scale factor. This is the bounds that we want
  // for the screenshot output.
  gfx::Rect scaled_snapshot_bounds =
      gfx::ScaleToRoundedRect(snapshot_bounds, device_scale_factor);
  gfx::Rect scaled_embed_bounds =
      gfx::ScaleToRoundedRect(embed_bounds, device_scale_factor);

  // On Android CopyFromSurface might not apply scale to input src_subrect.
  // Capture a snapshot and calculate screen capture scale between output and
  // input bounds and use it to calculate input bounds needed if we want to get
  // the desired output bounds. On other platforms, verify screen capture output
  // to input scale factor matches the device scale factor.
  // Wait for the view to be ready before start taking screenshots.
  content::WaitForCopyableViewInWebContents(web_contents());
  const gfx::Rect input_bounds(0, 0, 20, 20);
  SkBitmap bitmap = TakeScreenshot(input_bounds);
  const float capture_scale_factor = static_cast<float>(bitmap.width()) /
                                     static_cast<float>(input_bounds.width());
  // Verify that capture scale factor is consistent between width and height.
  ASSERT_FLOAT_EQ(capture_scale_factor,
                  static_cast<float>(bitmap.height()) /
                      static_cast<float>(input_bounds.height()));
#if !BUILDFLAG(IS_ANDROID)
  ASSERT_FLOAT_EQ(kTestDeviceScaleFactor, capture_scale_factor);
#endif
  gfx::Rect screen_shot_input_bounds = gfx::ScaleToRoundedRect(
      scaled_snapshot_bounds, 1.0f / capture_scale_factor);

  // Verify that we get expected output scaled_snapshot_bounds for the
  // screenshot.
  bitmap = TakeScreenshot(screen_shot_input_bounds);
  EXPECT_EQ(scaled_snapshot_bounds.width(), bitmap.width());
  EXPECT_EQ(scaled_snapshot_bounds.height(), bitmap.height());

  // Wait for the expected pixels to be rendered in the embed area.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    bitmap = TakeScreenshot(screen_shot_input_bounds);

    // Check a pixel inside the embed element.
    return (bitmap.getColor(scaled_embed_bounds.x() + 1,
                            scaled_embed_bounds.y() + 1) == SK_ColorRED);
  }));

  // Check a pixel outside the embed element.
  EXPECT_EQ(bitmap.getColor(1, 1), SK_ColorWHITE);
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest,
                       EmbedMultipleTagsCreatesMultiplePlugins) {
  constexpr size_t kMultipleEmbedCount = 2;

  NavigateToAttachHarness();

  auto child_contents1 = CreateChildWebContents();
  NavigateChildToUrl(child_contents1.get(), kEmptyUrl);
  AttachChildToEmbed(child_contents1.get());

  auto child_contents2 = CreateChildWebContents();
  NavigateChildToUrl(child_contents2.get(), kEmptyUrl);
  AttachChildToEmbed(child_contents2.get());

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
  auto child_contents = SetupHarnessAndChild();
  AttachChildToEmbed(child_contents.get());

  EXPECT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());

  // Verify that the host is created.
  WaitForHostCount(kSingleEmbedCount);
  ASSERT_EQ(kSingleEmbedCount, GetHostCount());

  // Destroy the child contents before the host is destroyed to prevent a
  // dangling pointer in SurfaceEmbedConnectorImpl.
  child_contents.reset();

  // Remove the embed element from the page.
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              "document.querySelector('embed').remove();"));

  // Verify that the host is destroyed.
  WaitForHostCount(0);
  EXPECT_EQ(0u, GetHostCount());
}

}  // namespace surface_embed
