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
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace surface_embed {

namespace {
constexpr std::string_view kAttachHarnessUrl =
    "/surface_embed/attach_harness.html";
constexpr std::string_view kBlueBoxUrl = "/surface_embed/blue_box.html";
constexpr std::string_view kRedBoxUrl = "/surface_embed/red_box.html";
constexpr std::string_view kFocusHarnessUrl =
    "/surface_embed/focus_harness.html";
constexpr std::string_view kInnerPageUrl = "/surface_embed/inner_page.html";
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
    content::ContentBrowserTestContentBrowserClient::
        RegisterBrowserInterfaceBindersForFrame(render_frame_host, map);
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
  // If `enable_binder` is true, SurfaceEmbedTestContentBrowserClient will be
  // installed to provide a binder for SurfaceEmbedHost interface.
  explicit SurfaceEmbedBrowserTest(bool enable_binder = true)
      : enable_binder_(enable_binder) {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kSurfaceEmbed);
    content::ContentBrowserTest::SetUp();
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    content::ContentBrowserTest::CreatedBrowserMainParts(browser_main_parts);
    if (enable_binder_) {
      test_browser_client_ =
          std::make_unique<SurfaceEmbedTestContentBrowserClient>(&tracker_);
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    // Enable pixel output in tests to allow CopyFromSurface to capture actual
    // rendered content instead of returning empty/black bitmaps.
    // Note that we force a device scale factor of 1.5 to also test scaling of
    // the surface embed plugin.
    EnablePixelOutput(kTestDeviceScaleFactor);

    command_line->AppendSwitchASCII(blink::switches::kJavaScriptFlags,
                                    "--expose-gc");
  }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
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
    return SetupHarnessAndChildWithContent(kRedBoxUrl);
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

  // Check if the given color is rendered without waiting.
  bool HasPixelInColor(SkColor target_color) {
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

  // Check if the given color is rendered.
  bool CheckHasPixelInColor(SkColor target_color) {
    return CheckHasPixelInColorInBitmapBounds(target_color);
  }

  // Check if the given color is rendered.
  bool CheckHasPixelInColorInBitmapBounds(
      SkColor target_color,
      gfx::Rect check_bitmap_bounds = gfx::Rect(),
      SkBitmap* out_bitmap = nullptr) {
    content::WaitForCopyableViewInWebContents(web_contents());
    // Retry finding the pixel since it might take a moment to propagate.
    return base::test::RunUntil([&]() {
      auto bitmap = TakeScreenshot(gfx::Rect());
      if (check_bitmap_bounds == gfx::Rect()) {
        check_bitmap_bounds.set_width(bitmap.width());
        check_bitmap_bounds.set_height(bitmap.height());
      }
      if (out_bitmap) {
        *out_bitmap = bitmap;
      }
      const int min_x = std::max(0, check_bitmap_bounds.x());
      const int max_x = std::min(bitmap.width(), check_bitmap_bounds.right());
      const int min_y = std::max(0, check_bitmap_bounds.y());
      const int max_y = std::min(bitmap.height(), check_bitmap_bounds.bottom());

      for (int x = min_x; x < max_x; ++x) {
        for (int y = min_y; y < max_y; ++y) {
          if (bitmap.getColor(x, y) == target_color) {
            return true;
          }
        }
      }
      return false;
    });
  }

  void CalculateBitmapBoundsToCheck(const gfx::Rect embed_bounds,
                                    gfx::Rect* out_scaled_bounds) {
    ASSERT_TRUE(out_scaled_bounds);

    content::RenderWidgetHostView* const view =
        web_contents()->GetRenderWidgetHostView();
    ASSERT_TRUE(view);

    // On Android forcing device scale factor might not work for tests,
    // therefore query the actual scale factor from the view. On other
    // platforms, verify it matches the forced value.
    const float device_scale_factor = view->GetDeviceScaleFactor();
#if !BUILDFLAG(IS_ANDROID)
    ASSERT_FLOAT_EQ(kTestDeviceScaleFactor, device_scale_factor);
#endif

    *out_scaled_bounds =
        gfx::ScaleToRoundedRect(embed_bounds, device_scale_factor);
  }

  void VerifyRedPixelInBounds(const gfx::Rect embed_bounds,
                              SkBitmap* out_bitmap = nullptr) {
    gfx::Rect scaled_embed_bounds;
    CalculateBitmapBoundsToCheck(embed_bounds, &scaled_embed_bounds);

    EXPECT_TRUE(CheckHasPixelInColorInBitmapBounds(
        SK_ColorRED, scaled_embed_bounds, out_bitmap));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  SurfaceEmbedHostTracker tracker_;
  bool enable_binder_;
  std::unique_ptr<SurfaceEmbedTestContentBrowserClient> test_browser_client_;
};

// A fixture where the browser-side support isn't provided.
class SurfaceEmbedBrowserTestNoHost : public SurfaceEmbedBrowserTest {
 public:
  SurfaceEmbedBrowserTestNoHost()
      : SurfaceEmbedBrowserTest(/*enable_binder=*/false) {}
};

// Test that trying to create a web plugin w/o providing support via
// SurfaceEmbedTestContentBrowserClient doesn't crash. This will imply
// that content_shell won't crash in similar circumstances.
IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTestNoHost, NoCrash) {
  auto child_contents = SetupHarnessAndChild();

  guest_contents::GuestContentsHandle* guest_handle =
      guest_contents::GuestContentsHandle::CreateForWebContents(
          child_contents.get());
  ASSERT_NE(guest_handle, nullptr);
  std::string script =
      content::JsReplace("createEmbed($1);", guest_handle->id().ToString());
  ASSERT_TRUE(content::ExecJs(web_contents(), script));

  // Access an unknown property on the embed to force plugin creation
  // (since otherwise it's on a timer).
  EXPECT_EQ(
      base::Value(),
      content::EvalJs(web_contents(), "document.embeds[0].noSuchProperty"));

  // Check that the sad plugin page got rendered.
  EXPECT_TRUE(CheckHasPixelInColor(SkColors::kGray.toSkColor()));

  // Try attaching a new child. Still shouldn't crash.
  // (Handling data-content-id changes isn't implemented yet, but once it is,
  //  this should help make sure we don't get confused).
  auto child_contents2 = CreateChildWebContents();
  NavigateChildToUrl(child_contents2.get(), kBlueBoxUrl);
  guest_contents::GuestContentsHandle* guest_handle2 =
      guest_contents::GuestContentsHandle::CreateForWebContents(
          child_contents2.get());
  EXPECT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace(
          "document.embeds[0].setAttribute('data-content-id', $1)",
          guest_handle2->id().ToString())));

  EXPECT_TRUE(CheckHasPixelInColor(SkColors::kGray.toSkColor()));
}

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

  // The embed element is at 10,10 with size 100x100 in red_box.html.
  const gfx::Rect embed_bounds(10, 10, 100, 100);

  SkBitmap last_bitmap;
  VerifyRedPixelInBounds(embed_bounds, &last_bitmap);

  // Check a pixel outside the embed element.
  EXPECT_EQ(last_bitmap.getColor(1, 1), SK_ColorWHITE);
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest,
                       EmbedMultipleTagsCreatesMultiplePlugins) {
  constexpr size_t kMultipleEmbedCount = 2;

  NavigateToAttachHarness();

  auto child_contents1 = CreateChildWebContents();
  NavigateChildToUrl(child_contents1.get(), kRedBoxUrl);
  AttachChildToEmbed(child_contents1.get());

  auto child_contents2 = CreateChildWebContents();
  NavigateChildToUrl(child_contents2.get(), kRedBoxUrl);
  AttachChildToEmbed(child_contents2.get());

  EXPECT_EQ(kMultipleEmbedCount, CountEmbedElementsInPage());

  // Verify that the hosts are created.
  WaitForHostCount(kMultipleEmbedCount);
  ASSERT_EQ(kMultipleEmbedCount, GetHostCount());

  for (size_t i = 0; i < kMultipleEmbedCount; ++i) {
    SurfaceEmbedHost* host = GetHost(i);
    ASSERT_NE(nullptr, host);
  }
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

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, Crash) {
  auto child_contents = SetupHarnessAndChildWithContent(kRedBoxUrl);
  AttachChildToEmbed(child_contents.get());

  EXPECT_TRUE(CheckHasPixelInColor(SK_ColorRED));

  // Simulate a crash.
  content::ScopedAllowRendererCrashes testing_crashes_here(
      child_contents->GetPrimaryMainFrame());
  child_contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      content::RESULT_CODE_KILLED);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return child_contents->IsCrashed(); }));
  // The crashed frame gets drawn with a gray background, with an image
  // in the middle (which doesn't seem configured for tests). The gray in
  // question is a bit different than SK_ColorGRAY.
  EXPECT_TRUE(CheckHasPixelInColor(SkColors::kGray.toSkColor()));
  EXPECT_FALSE(HasPixelInColor(SK_ColorRED));

  // Remove the embed element from the page.
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              "document.querySelector('embed').remove();"));

  // Trigger garbage collection. The C++ side of the heap has
  // blink::PendingLayer which refer to a PictureLayer we use, which was caught
  // with a dangling pointer in review. This call would have raw_ptr catch it
  // if the bug were still there.
  EXPECT_TRUE(content::ExecJs(web_contents(), "gc()"));

  // Verify that the host is destroyed.
  WaitForHostCount(0);
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, CrashThenReattach) {
  auto child_contents = SetupHarnessAndChildWithContent(kRedBoxUrl);
  AttachChildToEmbed(child_contents.get());

  EXPECT_TRUE(CheckHasPixelInColor(SK_ColorRED));

  // Simulate a crash.
  content::ScopedAllowRendererCrashes testing_crashes_here(
      child_contents->GetPrimaryMainFrame());
  child_contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      content::RESULT_CODE_KILLED);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return child_contents->IsCrashed(); }));
  // The crashed frame gets drawn with a gray background, with an image
  // in the middle (which doesn't seem configured for tests). The gray in
  // question is a bit different than SK_ColorGRAY.
  EXPECT_TRUE(CheckHasPixelInColor(SkColors::kGray.toSkColor()));
  EXPECT_FALSE(HasPixelInColor(SK_ColorRED));

  auto child_contents2 = CreateChildWebContents();
  NavigateChildToUrl(child_contents2.get(), kBlueBoxUrl);
  AttachChildToEmbed(child_contents2.get());

  EXPECT_TRUE(CheckHasPixelInColor(SK_ColorBLUE));
}

// Test case where child process crashed before the attach.
IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, CrashEarly) {
  auto child_contents = SetupHarnessAndChildWithContent(kRedBoxUrl);

  // Simulate a crash.
  content::ScopedAllowRendererCrashes testing_crashes_here(
      child_contents->GetPrimaryMainFrame());
  child_contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      content::RESULT_CODE_KILLED);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return child_contents->IsCrashed(); }));

  // Now try to attach. This doesn't actually attach successfully, so can't
  // use the usual helper.
  guest_contents::GuestContentsHandle* guest_handle =
      guest_contents::GuestContentsHandle::CreateForWebContents(
          child_contents.get());
  ASSERT_NE(guest_handle, nullptr);
  std::string script = "createEmbed('" + guest_handle->id().ToString() + "')";
  EXPECT_TRUE(content::ExecJs(web_contents(), script));

  // Should have a gray background.
  EXPECT_TRUE(CheckHasPixelInColor(SkColors::kGray.toSkColor()));
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, VisualPropertiesSync) {
  auto child_contents = SetupHarnessAndChild();
  AttachChildToEmbed(child_contents.get());

  auto* connector = child_contents->GetSurfaceEmbedConnector();
  ASSERT_NE(nullptr, connector);

  EXPECT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());

  // Wait for the initial size to propagate to the child.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return connector->GetLocalFrameSizeInPixelsForTesting() ==
               gfx::Size(150, 150) &&
           connector->GetCssZoomFactorForTesting() == 1.0;
  }));

  // Change the size of the embed element.
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              "let embed = document.querySelector('embed');"
                              "embed.style.width = '250px';"
                              "embed.style.height = '150px';"));

  // Wait for the new size to propagate to the child.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return connector->GetLocalFrameSizeInPixelsForTesting() ==
           gfx::Size(375, 225);
  }));

  // Change the zoom of the embed element.
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              "let embed = document.querySelector('embed');"
                              "embed.style.zoom = 2.0;"));

  // Wait for the new zoom to propagate to the child's local frame size.
  // The layout size of 250x150 with zoom 2.0 and dsf 1.5 is 250 * 2.0 * 1.5 =
  // 750, 150 * 2.0 * 1.5 = 450.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return connector->GetLocalFrameSizeInPixelsForTesting() ==
           gfx::Size(750, 450);
  }));
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, ResizeEmbedPixelTest) {
  auto child_contents = SetupHarnessAndChild();
  AttachChildToEmbed(child_contents.get());

  EXPECT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());

  // Verify that the host is created.
  WaitForHostCount(kSingleEmbedCount);
  ASSERT_EQ(kSingleEmbedCount, GetHostCount());
  SurfaceEmbedHost* host = GetHost(0);
  ASSERT_NE(nullptr, host);

  // The embed element is at 10,10 with size 100x100 in red_box.html.
  const gfx::Rect embed_bounds(10, 10, 100, 100);

  VerifyRedPixelInBounds(embed_bounds);

  // Resize the embed element to 200x200.
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              "let embed = document.querySelector('embed');"
                              "embed.style.width = '200px';"
                              "embed.style.height = '200px';"));

  // Wait for the new size to propagate to the child's local frame size.
  // The layout size of 200x200 with dsf 1.5 is 200 * 1.5 = 300.
  auto* connector = child_contents->GetSurfaceEmbedConnector();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return connector->GetLocalFrameSizeInPixelsForTesting() ==
           gfx::Size(300, 300);
  }));

  // The bounds should eventually be 200x200, so check a pixel that's outside
  // the original 100x100 but inside 200x200. For example, x=150, y=150.
  gfx::Rect new_embed_pixel_bounds(150, 150, 10, 10);

  VerifyRedPixelInBounds(new_embed_pixel_bounds);
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest,
                       CrossProcessNavigationPixelTest) {
  auto child_contents = SetupHarnessAndChild();
  AttachChildToEmbed(child_contents.get());

  EXPECT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());

  // Verify that the host is created.
  WaitForHostCount(kSingleEmbedCount);
  ASSERT_EQ(kSingleEmbedCount, GetHostCount());
  SurfaceEmbedHost* host = GetHost(0);
  ASSERT_NE(nullptr, host);

  // The embed element is at 10,10 with size 100x100 in red_box.html.
  const gfx::Rect embed_bounds(10, 10, 100, 100);

  VerifyRedPixelInBounds(embed_bounds);

  // Navigate the child to a different site to force a cross-process navigation.
  GURL cross_site_url = embedded_test_server()->GetURL("a.test", kRedBoxUrl);
  ASSERT_TRUE(content::NavigateToURL(child_contents.get(), cross_site_url));
  ASSERT_TRUE(content::WaitForLoadStop(child_contents.get()));

  // We can just verify that it still renders the red box.
  VerifyRedPixelInBounds(embed_bounds);
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, FocusByClick) {
  NavigateToTestUrl(kFocusHarnessUrl);

  auto child_contents = CreateChildWebContents();
  NavigateChildToUrl(child_contents.get(), kInnerPageUrl);
  content::ReadyForInputObserver(web_contents()).Wait();

  AttachChildToEmbedWithId(child_contents.get(), "my_embed");

  // Click to focus outer1 in the outer page.
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "outer1");

  EXPECT_EQ(true, content::EvalJsAfterLifecycleUpdate(web_contents(), "",
                                                      "document.hasFocus()"));
  EXPECT_EQ("outer1", content::EvalJsAfterLifecycleUpdate(
                          web_contents(), "", "document.activeElement.id"));

  // Click an <input id="inner"> in the inner page. This should change focus to
  // the embed element in the outer page.
  content::SimulateMouseClickOrTapElementWithId(child_contents.get(), "inner");

  EXPECT_EQ(true, content::EvalJsAfterLifecycleUpdate(web_contents(), "",
                                                      "document.hasFocus()"));
  EXPECT_EQ("my_embed", content::EvalJsAfterLifecycleUpdate(
                            web_contents(), "", "document.activeElement.id"));
  // TODO(crbug.com/508638062): add expectations for the following behavior.
  //   1. the inner page should has page focus.
  //   2. the inner page's "inner" element should be the active element.
  //   3. the child WebContents should be the focused WebContents, and should
  //      receive keyboard events.
}

}  // namespace surface_embed
