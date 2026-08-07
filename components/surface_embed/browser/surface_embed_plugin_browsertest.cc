// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/guest_contents/browser/guest_contents_handle.h"
#include "components/surface_embed/browser/surface_embed_host.h"
#include "components/surface_embed/common/features.h"
#include "components/surface_embed/common/surface_embed.mojom.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/surface_embed_connector.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
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
constexpr std::string_view kMultilevelHarnessUrl =
    "/surface_embed/multilevel_harness.html";
constexpr std::string_view kMultilevelParentUrl =
    "/surface_embed/multilevel_parent.html";
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

  void RegisterAssociatedInterfaceBindersForRenderFrameHost(
      content::RenderFrameHost& render_frame_host,
      blink::AssociatedInterfaceRegistry& associated_registry) override {
    content::ContentBrowserTestContentBrowserClient::
        RegisterAssociatedInterfaceBindersForRenderFrameHost(
            render_frame_host, associated_registry);
    associated_registry.RemoveInterface(mojom::SurfaceEmbedHost::Name_);
    associated_registry.AddInterface<
        mojom::SurfaceEmbedHost>(base::BindRepeating(
        [](SurfaceEmbedHostTracker* tracker,
           content::RenderFrameHost* render_frame_host,
           mojo::PendingAssociatedReceiver<mojom::SurfaceEmbedHost> receiver) {
          SurfaceEmbedHost* host =
              SurfaceEmbedHost::Create(render_frame_host, std::move(receiver));
          host->SetDestructionCallbackForTesting(
              base::BindOnce(&SurfaceEmbedHostTracker::RemoveHost,
                             base::Unretained(tracker), host));
          tracker->AddHost(host);
        },
        base::Unretained(tracker_), &render_frame_host));
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

  // Waits until the active element in |wc| has the expected id.
  void WaitForActiveElement(content::WebContents* wc,
                            const std::string& expected_id) {
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return content::EvalJs(wc, "document.activeElement.id") == expected_id;
    }));
  }

  void WaitForMultilevelFocusState(content::WebContents* parent_contents,
                                   content::WebContents* child_contents,
                                   const std::string& root_active_element_id) {
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return content::GetFocusedWebContents(web_contents()) == child_contents;
    }));

    WaitForActiveElement(child_contents, "inner");
    WaitForActiveElement(parent_contents, "child_embed");
    WaitForActiveElement(web_contents(), root_active_element_id);

    for (content::WebContents* contents :
         {web_contents(), parent_contents, child_contents}) {
      EXPECT_TRUE(base::test::RunUntil([&]() {
        return content::EvalJs(contents, "document.hasFocus()").ExtractBool();
      }));
    }
  }

  void WaitForMultilevelViewSizes(content::WebContents* parent_contents,
                                  content::WebContents* child_contents) {
    EXPECT_TRUE(base::test::RunUntil([&]() {
      auto* parent_view = parent_contents->GetRenderWidgetHostView();
      return parent_view &&
             parent_view->GetViewBounds().size() == gfx::Size(200, 150);
    }));
    EXPECT_TRUE(base::test::RunUntil([&]() {
      auto* child_view = child_contents->GetRenderWidgetHostView();
      return child_view &&
             child_view->GetViewBounds().size() == gfx::Size(100, 100);
    }));
    EXPECT_TRUE(base::test::RunUntil([&]() {
      auto* connector = parent_contents->GetSurfaceEmbedConnector();
      return connector && connector->GetLocalFrameSizeInPixelsForTesting() ==
                              gfx::Size(300, 225);
    }));
    EXPECT_TRUE(base::test::RunUntil([&]() {
      auto* connector = child_contents->GetSurfaceEmbedConnector();
      return connector && connector->GetLocalFrameSizeInPixelsForTesting() ==
                              gfx::Size(150, 150);
    }));
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

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, VisibilityHiddenPropagates) {
  auto child_contents = SetupHarnessAndChild();
  AttachChildToEmbed(child_contents.get());

  EXPECT_EQ(content::Visibility::VISIBLE, child_contents->GetVisibility());

  ASSERT_TRUE(content::ExecJs(
      web_contents(), "document.embeds[0].style.visibility = 'hidden';"));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return child_contents->GetVisibility() == content::Visibility::HIDDEN;
  }));

  ASSERT_TRUE(content::ExecJs(
      web_contents(), "document.embeds[0].style.visibility = 'visible';"));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return child_contents->GetVisibility() == content::Visibility::VISIBLE;
  }));
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, DisplayNonePropagates) {
  auto child_contents = SetupHarnessAndChild();
  AttachChildToEmbed(child_contents.get());

  EXPECT_EQ(content::Visibility::VISIBLE, child_contents->GetVisibility());

  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "document.embeds[0].style.display = 'none';"));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return child_contents->GetVisibility() == content::Visibility::HIDDEN;
  }));

  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "document.embeds[0].style.display = 'block';"));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return child_contents->GetVisibility() == content::Visibility::VISIBLE;
  }));
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest,
                       MultilevelVisibilityHiddenPropagates) {
  NavigateToAttachHarness();

  // Setup Parent (P) WebContents.
  auto parent_contents = CreateChildWebContents();
  NavigateChildToUrl(parent_contents.get(), kAttachHarnessUrl);
  AttachChildToEmbedWithId(parent_contents.get(), "parent_embed");

  // Setup Child (C) WebContents.
  auto child_contents = CreateChildWebContents();
  NavigateChildToUrl(child_contents.get(), kRedBoxUrl);

  // Attach C to P.
  guest_contents::GuestContentsHandle* child_guest_handle =
      guest_contents::GuestContentsHandle::CreateForWebContents(
          child_contents.get());
  ASSERT_NE(child_guest_handle, nullptr);

  std::string attach_child_script = "createEmbed('" +
                                    child_guest_handle->id().ToString() +
                                    "', 'child_embed');";
  size_t expected_attachments = GetAttachedHostCount() + 1;
  ASSERT_TRUE(content::ExecJs(parent_contents.get(), attach_child_script));
  ASSERT_TRUE(WaitForHostAttachment(expected_attachments));
  EXPECT_NE(child_contents->GetSurfaceEmbedConnector(), nullptr);

  EXPECT_EQ(content::Visibility::VISIBLE, parent_contents->GetVisibility());
  EXPECT_EQ(content::Visibility::VISIBLE, child_contents->GetVisibility());

  // Hide Parent embed in Grandparent. P and C should become HIDDEN.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.getElementById('parent_embed').style.visibility = 'hidden';"));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return parent_contents->GetVisibility() == content::Visibility::HIDDEN;
  }));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return child_contents->GetVisibility() == content::Visibility::HIDDEN;
  }));

  // Make Parent embed visible again. P and C should become VISIBLE.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.getElementById('parent_embed').style.visibility = 'visible';"));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return parent_contents->GetVisibility() == content::Visibility::VISIBLE;
  }));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return child_contents->GetVisibility() == content::Visibility::VISIBLE;
  }));

  // Hide Child embed in Parent. Only C should become HIDDEN, P remains VISIBLE.
  ASSERT_TRUE(content::ExecJs(
      parent_contents.get(),
      "document.getElementById('child_embed').style.visibility = 'hidden';"));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return child_contents->GetVisibility() == content::Visibility::HIDDEN;
  }));
  EXPECT_EQ(content::Visibility::VISIBLE, parent_contents->GetVisibility());

  // Make Child embed visible again. C should become VISIBLE.
  ASSERT_TRUE(content::ExecJs(
      parent_contents.get(),
      "document.getElementById('child_embed').style.visibility = 'visible';"));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return child_contents->GetVisibility() == content::Visibility::VISIBLE;
  }));
  EXPECT_EQ(content::Visibility::VISIBLE, parent_contents->GetVisibility());
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest,
                       MultilevelDisplayNonePropagates) {
  NavigateToAttachHarness();

  // Setup Parent (P) WebContents.
  auto parent_contents = CreateChildWebContents();
  NavigateChildToUrl(parent_contents.get(), kAttachHarnessUrl);
  AttachChildToEmbedWithId(parent_contents.get(), "parent_embed");

  // Setup Child (C) WebContents.
  auto child_contents = CreateChildWebContents();
  NavigateChildToUrl(child_contents.get(), kRedBoxUrl);

  // Attach C to P.
  guest_contents::GuestContentsHandle* child_guest_handle =
      guest_contents::GuestContentsHandle::CreateForWebContents(
          child_contents.get());
  ASSERT_NE(child_guest_handle, nullptr);

  std::string attach_child_script = "createEmbed('" +
                                    child_guest_handle->id().ToString() +
                                    "', 'child_embed');";
  size_t expected_attachments = GetAttachedHostCount() + 1;
  ASSERT_TRUE(content::ExecJs(parent_contents.get(), attach_child_script));
  ASSERT_TRUE(WaitForHostAttachment(expected_attachments));
  EXPECT_NE(child_contents->GetSurfaceEmbedConnector(), nullptr);

  EXPECT_EQ(content::Visibility::VISIBLE, parent_contents->GetVisibility());
  EXPECT_EQ(content::Visibility::VISIBLE, child_contents->GetVisibility());

  // Hide Parent embed in Grandparent using display = 'none'. P and C should
  // become HIDDEN.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.getElementById('parent_embed').style.display = 'none';"));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return parent_contents->GetVisibility() == content::Visibility::HIDDEN;
  }));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return child_contents->GetVisibility() == content::Visibility::HIDDEN;
  }));

  // Restore Parent embed in Grandparent. P and C should become VISIBLE.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.getElementById('parent_embed').style.display = 'block';"));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return parent_contents->GetVisibility() == content::Visibility::VISIBLE;
  }));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return child_contents->GetVisibility() == content::Visibility::VISIBLE;
  }));

  // Hide Child embed in Parent using display = 'none'. Only C should become
  // HIDDEN, P remains VISIBLE.
  ASSERT_TRUE(content::ExecJs(
      parent_contents.get(),
      "document.getElementById('child_embed').style.display = 'none';"));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return child_contents->GetVisibility() == content::Visibility::HIDDEN;
  }));
  EXPECT_EQ(content::Visibility::VISIBLE, parent_contents->GetVisibility());

  // Restore Child embed in Parent. C should become VISIBLE.
  ASSERT_TRUE(content::ExecJs(
      parent_contents.get(),
      "document.getElementById('child_embed').style.display = 'block';"));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return child_contents->GetVisibility() == content::Visibility::VISIBLE;
  }));
  EXPECT_EQ(content::Visibility::VISIBLE, parent_contents->GetVisibility());
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

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, CrashThenReload) {
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

  // Reload the child contents.
  child_contents->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(content::WaitForLoadStop(child_contents.get()));

  EXPECT_FALSE(child_contents->IsCrashed());
  EXPECT_TRUE(CheckHasPixelInColor(SK_ColorRED));
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
  // The outer WebContents should be the focused WebContents and child
  // WebContents should not be able to see the focused frame.
  EXPECT_EQ(web_contents(), content::GetFocusedWebContents(web_contents()));
  EXPECT_EQ(web_contents()->GetPrimaryMainFrame(),
            web_contents()->GetFocusedFrame());
  EXPECT_EQ(nullptr, content::GetFocusedWebContents(child_contents.get()));
  EXPECT_EQ(nullptr, child_contents->GetFocusedFrame());
  // The outer WebContents should receive keyboard events.
  content::SimulateKeyPress(web_contents(), ui::DomKey::FromCharacter('a'),
                            ui::DomCode::US_A, ui::VKEY_A, false, false, false,
                            false);
  EXPECT_EQ("a",
            content::EvalJsAfterLifecycleUpdate(
                web_contents(), "", "document.getElementById('outer1').value"));

  // Wait for the child's hit test data to be available so the click can be
  // properly handled by it.
  content::WaitForHitTestData(child_contents.get());
  // Click at the location of <input id="inner"> in the inner page. This should
  // change focus to the embed element in the outer page.
  auto inner_center_point = gfx::ToFlooredPoint(
      GetCenterCoordinatesOfElementWithId(child_contents.get(), "inner"));
  // Embed is at 10, 50 in parent.
  inner_center_point.Offset(10, 50);
  // Simulate a mouse click to parent web contents at the location of the inner
  // input element. The simulated event will be dispatched to the child via the
  // parent WebContents's input event router. This is the same as what happens
  // in real production code.
  content::SimulateMouseClickAt(web_contents(), 0,
                                blink::WebMouseEvent::Button::kLeft,
                                inner_center_point);
  EXPECT_EQ(true, content::EvalJsAfterLifecycleUpdate(web_contents(), "",
                                                      "document.hasFocus()"));
  EXPECT_EQ("my_embed", content::EvalJsAfterLifecycleUpdate(
                            web_contents(), "", "document.activeElement.id"));
  // The inner page should has page focus.
  EXPECT_EQ(true, content::EvalJsAfterLifecycleUpdate(child_contents.get(), "",
                                                      "document.hasFocus()"));
  // The inner page's "inner" element should become the active element.
  EXPECT_EQ("inner",
            content::EvalJsAfterLifecycleUpdate(child_contents.get(), "",
                                                "document.activeElement.id"));
  // The child WebContents should be the focused WebContents and child
  // WebContents should be able to see the focused frame.
  EXPECT_EQ(child_contents.get(),
            content::GetFocusedWebContents(web_contents()));
  EXPECT_EQ(child_contents->GetPrimaryMainFrame(),
            web_contents()->GetFocusedFrame());
  EXPECT_EQ(child_contents.get(),
            content::GetFocusedWebContents(child_contents.get()));
  EXPECT_EQ(child_contents->GetPrimaryMainFrame(),
            child_contents->GetFocusedFrame());

  // The child WebContents should receive keyboard events.
  content::SimulateKeyPress(web_contents(), ui::DomKey::FromCharacter('b'),
                            ui::DomCode::US_A, ui::VKEY_A, false, false, false,
                            false);
  EXPECT_EQ("b", content::EvalJsAfterLifecycleUpdate(
                     child_contents.get(), "",
                     "document.getElementById('inner').value"));

  // Destroy the child WebContents and verify that the focus moves to outer
  // WebContents.
  child_contents.reset();
  EXPECT_EQ(web_contents(), content::GetFocusedWebContents(web_contents()));
  EXPECT_EQ(web_contents()->GetPrimaryMainFrame(),
            web_contents()->GetFocusedFrame());
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest,
                       HtmlPopupOnSurfaceEmbeddedPage) {
  auto child_contents =
      SetupHarnessAndChildWithContent("/surface_embed/inner_page.html");
  AttachChildToEmbed(child_contents.get());

  // Replace child page contents with a select element.
  EXPECT_TRUE(content::ExecJs(child_contents.get(), R"(
    document.body.innerHTML = "<select><option>option1</option></select>";
  )"));

  // Ensure visibility and activation of the child WebContents.
  child_contents->WasShown();
  child_contents->GetPrimaryMainFrame()->GetRenderWidgetHost()->SetActive(true);

  content::ShowPopupWidgetWaiter waiter(child_contents.get(),
                                        child_contents->GetPrimaryMainFrame());
  EXPECT_TRUE(content::ExecJs(child_contents.get(),
                              "document.querySelector('select').showPicker()"));
  // Android sometimes times out when waiting for the popup.
#if !BUILDFLAG(IS_ANDROID)
  waiter.Wait();
#endif
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, FocusPreservedAfterNavigation) {
  NavigateToTestUrl(kFocusHarnessUrl);

  auto child_contents = CreateChildWebContents();
  NavigateChildToUrl(child_contents.get(), kInnerPageUrl);
  content::ReadyForInputObserver(web_contents()).Wait();

  AttachChildToEmbedWithId(child_contents.get(), "my_embed");

  // Click/focus the outer page first to ensure the window is focused.
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "outer1");
  EXPECT_TRUE(
      content::EvalJs(web_contents(), "document.hasFocus()").ExtractBool());

  // Script focus within the child should not move page focus across the
  // WebContents boundary.
  EXPECT_TRUE(content::ExecJs(child_contents.get(),
                              "document.getElementById('inner').focus()"));
  // Wait a little to ensure that focus really does not change.
  base::RunLoop focus_settle_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, focus_settle_loop.QuitClosure(), base::Milliseconds(100));
  focus_settle_loop.Run();
  EXPECT_EQ(web_contents(), content::GetFocusedWebContents(web_contents()));
  EXPECT_TRUE(
      content::EvalJs(web_contents(), "document.hasFocus()").ExtractBool());
  EXPECT_EQ("inner",
            content::EvalJs(child_contents.get(), "document.activeElement.id"));

  // Click the child's input element to focus it.
  content::WaitForHitTestData(child_contents.get());
  auto inner_center = content::GetCenterCoordinatesOfElementWithId(
      child_contents.get(), "inner");
  // Child embed is at (10, 50) in the parent coordinate space.
  gfx::Point click_point(static_cast<int>(inner_center.x()) + 10,
                         static_cast<int>(inner_center.y()) + 50);
  content::SimulateMouseClickAt(
      web_contents(), 0, blink::WebMouseEvent::Button::kLeft, click_point);

  // Wait for the focus change to propagate.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(child_contents.get(), "document.hasFocus()")
        .ExtractBool();
  }));

  EXPECT_TRUE(
      content::EvalJs(web_contents(), "document.hasFocus()").ExtractBool());
  EXPECT_TRUE(content::EvalJs(child_contents.get(), "document.hasFocus()")
                  .ExtractBool());

  // Navigate the child to a different site to force a
  // cross-process/cross-domain navigation.
  GURL cross_site_url = embedded_test_server()->GetURL("a.test", kInnerPageUrl);
  ASSERT_TRUE(content::NavigateToURL(child_contents.get(), cross_site_url));
  ASSERT_TRUE(content::WaitForLoadStop(child_contents.get()));

  // Verify that the focus is preserved on the navigated child page.
  EXPECT_TRUE(
      content::EvalJs(web_contents(), "document.hasFocus()").ExtractBool());
  EXPECT_TRUE(content::EvalJs(child_contents.get(), "document.hasFocus()")
                  .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, FocusByTabKey) {
  NavigateToTestUrl(kFocusHarnessUrl);

  auto child_contents = CreateChildWebContents();
  NavigateChildToUrl(child_contents.get(), kInnerPageUrl);
  content::ReadyForInputObserver(web_contents()).Wait();

  AttachChildToEmbedWithId(child_contents.get(), "my_embed");

  // Focus outer1 which is before the embed tag.
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              "document.getElementById('outer1').focus()"));
  ASSERT_EQ("outer1",
            content::EvalJs(web_contents(), "document.activeElement.id"));

  // Press tab, it should move into the embed element, which focuses the child
  // WebContents.
  content::SimulateKeyPress(web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                            ui::VKEY_TAB, false, false, false, false);

  // Focus should go to the embed element in the parent WebContents.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(web_contents(), "document.activeElement.id") ==
               "my_embed" &&
           content::EvalJs(child_contents.get(), "document.hasFocus()")
               .ExtractBool() &&
           content::EvalJs(child_contents.get(), "document.activeElement.id") ==
               "inner";
  }));

  // Keep pressing tab, it should not crash.
  // TODO(crbug.com/508638062): update this test to traverse to the next element
  // after the embed element.
  content::SimulateKeyPress(web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                            ui::VKEY_TAB, false, false, false, false);
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, FocusByShiftTabKey) {
  NavigateToTestUrl(kFocusHarnessUrl);

  auto child_contents = CreateChildWebContents();
  NavigateChildToUrl(child_contents.get(), kInnerPageUrl);
  content::ReadyForInputObserver(web_contents()).Wait();

  ASSERT_TRUE(content::ExecJs(child_contents.get(), R"(
    const input = document.createElement('input');
    input.id = 'inner2';
    document.body.appendChild(input);
  )"));
  AttachChildToEmbedWithId(child_contents.get(), "my_embed");

  // Focus outer2, which follows the embed element in document order.
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              "document.getElementById('outer2').focus()"));
  ASSERT_EQ("outer2",
            content::EvalJs(web_contents(), "document.activeElement.id"));

  content::SimulateKeyPress(web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                            ui::VKEY_TAB, false, true, false, false);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(web_contents(), "document.activeElement.id") ==
               "my_embed" &&
           content::EvalJs(child_contents.get(), "document.hasFocus()")
               .ExtractBool() &&
           content::EvalJs(child_contents.get(), "document.activeElement.id") ==
               "inner2";
  }));
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, MultilevelDetachIntermediate) {
  NavigateToAttachHarness();

  auto parent_contents = CreateChildWebContents();
  NavigateChildToUrl(parent_contents.get(), kAttachHarnessUrl);
  AttachChildToEmbedWithId(parent_contents.get(), "parent_embed");

  auto child_contents = CreateChildWebContents();
  NavigateChildToUrl(child_contents.get(), kRedBoxUrl);

  guest_contents::GuestContentsHandle* child_guest_handle =
      guest_contents::GuestContentsHandle::CreateForWebContents(
          child_contents.get());
  ASSERT_NE(child_guest_handle, nullptr);

  std::string attach_child_script = "createEmbed('" +
                                    child_guest_handle->id().ToString() +
                                    "', 'child_embed');";
  size_t expected_attachments = GetAttachedHostCount() + 1;
  ASSERT_TRUE(content::ExecJs(parent_contents.get(), attach_child_script));
  ASSERT_TRUE(WaitForHostAttachment(expected_attachments));
  EXPECT_NE(child_contents->GetSurfaceEmbedConnector(), nullptr);
  EXPECT_NE(parent_contents->GetSurfaceEmbedConnector(), nullptr);
  EXPECT_EQ(content::Visibility::VISIBLE, child_contents->GetVisibility());

  // Verify the child's red content renders through the 2-level chain.
  const gfx::Rect embed_bounds(10, 10, 100, 100);
  VerifyRedPixelInBounds(embed_bounds);

  // Remove the parent embed from the grandparent page, detaching the parent.
  EXPECT_TRUE(content::ExecJs(
      web_contents(), "document.getElementById('parent_embed').remove();"));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return parent_contents->GetSurfaceEmbedConnector() == nullptr;
  }));

  // The child remains attached to the parent.
  EXPECT_NE(child_contents->GetSurfaceEmbedConnector(), nullptr);
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, MultilevelFocusAndInput) {
  NavigateToTestUrl(kMultilevelHarnessUrl);

  auto parent_contents = CreateChildWebContents();
  NavigateChildToUrl(parent_contents.get(), kMultilevelParentUrl);
  AttachChildToEmbedWithId(parent_contents.get(), "parent_embed");
  EXPECT_NE(parent_contents->GetSurfaceEmbedConnector(), nullptr);

  auto child_contents = CreateChildWebContents();
  NavigateChildToUrl(child_contents.get(), kInnerPageUrl);

  guest_contents::GuestContentsHandle* child_guest_handle =
      guest_contents::GuestContentsHandle::CreateForWebContents(
          child_contents.get());
  ASSERT_NE(child_guest_handle, nullptr);

  std::string attach_child_script = "createEmbed('" +
                                    child_guest_handle->id().ToString() +
                                    "', 'child_embed');";
  size_t expected_attachments = GetAttachedHostCount() + 1;
  ASSERT_TRUE(content::ExecJs(parent_contents.get(), attach_child_script));
  ASSERT_TRUE(WaitForHostAttachment(expected_attachments));
  EXPECT_NE(child_contents->GetSurfaceEmbedConnector(), nullptr);

  // Focus outer1 in the grandparent and verify keyboard input.
  content::ReadyForInputObserver(web_contents()).Wait();
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "outer1");
  EXPECT_EQ("outer1", content::EvalJsAfterLifecycleUpdate(
                          web_contents(), "", "document.activeElement.id"));
  EXPECT_EQ(web_contents(), content::GetFocusedWebContents(web_contents()));

  content::SimulateKeyPress(web_contents(), ui::DomKey::FromCharacter('g'),
                            ui::DomCode::US_A, ui::VKEY_A, false, false, false,
                            false);
  EXPECT_EQ("g",
            content::EvalJsAfterLifecycleUpdate(
                web_contents(), "", "document.getElementById('outer1').value"));

  // Click the child's input through grandparent -> parent -> child.
  WaitForMultilevelViewSizes(parent_contents.get(), child_contents.get());
  // Parent is at (10, 50) in the root, and child is at (10, 40) in
  // parent, so the child's bounds in root coordinates are (20, 90, 100, 100).
  const gfx::Rect child_embed_bounds(20, 90, 100, 100);
  VerifyRedPixelInBounds(child_embed_bounds);
  content::WaitForHitTestData(parent_contents.get());
  content::WaitForHitTestData(child_contents.get());
  auto inner_center = content::GetCenterCoordinatesOfElementWithId(
      child_contents.get(), "inner");
  // Offset by embed positions: child in parent (10, 40) + parent in
  // grandparent (10, 50).
  gfx::Point click_point(static_cast<int>(inner_center.x()) + 10 + 10,
                         static_cast<int>(inner_center.y()) + 40 + 50);

  content::SimulateMouseClickAt(
      web_contents(), 0, blink::WebMouseEvent::Button::kLeft, click_point);

  WaitForMultilevelFocusState(parent_contents.get(), child_contents.get(),
                              "parent_embed");

  content::SimulateKeyPress(web_contents(), ui::DomKey::FromCharacter('c'),
                            ui::DomCode::US_A, ui::VKEY_A, false, false, false,
                            false);
  EXPECT_EQ("c", content::EvalJsAfterLifecycleUpdate(
                     child_contents.get(), "",
                     "document.getElementById('inner').value"));

  // Detach parent from grandparent and re-attach with child still connected.
  EXPECT_TRUE(content::ExecJs(
      web_contents(), "document.getElementById('parent_embed').remove();"));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return parent_contents->GetSurfaceEmbedConnector() == nullptr;
  }));
  EXPECT_NE(child_contents->GetSurfaceEmbedConnector(), nullptr);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return !HasPixelInColor(SK_ColorRED); }));

  AttachChildToEmbedWithId(parent_contents.get(), "reattached_embed");
  EXPECT_NE(child_contents->GetSurfaceEmbedConnector(), nullptr);

  // Wait for child to become visible again after re-attach.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return child_contents->GetVisibility() == content::Visibility::VISIBLE;
  }));

  // Click the child's input after re-attach.
  WaitForMultilevelViewSizes(parent_contents.get(), child_contents.get());
  VerifyRedPixelInBounds(child_embed_bounds);
  content::WaitForHitTestData(parent_contents.get());
  content::WaitForHitTestData(child_contents.get());
  inner_center = content::GetCenterCoordinatesOfElementWithId(
      child_contents.get(), "inner");
  click_point = gfx::Point(static_cast<int>(inner_center.x()) + 10 + 10,
                           static_cast<int>(inner_center.y()) + 40 + 50);
  content::SimulateMouseClickAt(
      web_contents(), 0, blink::WebMouseEvent::Button::kLeft, click_point);

  WaitForMultilevelFocusState(parent_contents.get(), child_contents.get(),
                              "reattached_embed");

  EXPECT_TRUE(content::ExecJs(child_contents.get(),
                              "document.getElementById('inner').value = '';"));
  content::SimulateKeyPress(web_contents(), ui::DomKey::FromCharacter('r'),
                            ui::DomCode::US_A, ui::VKEY_A, false, false, false,
                            false);
  EXPECT_EQ("r", content::EvalJsAfterLifecycleUpdate(
                     child_contents.get(), "",
                     "document.getElementById('inner').value"));
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest, ThrottlingPropagation) {
  auto child_contents = SetupHarnessAndChild();
  AttachChildToEmbed(child_contents.get());

  auto* connector = child_contents->GetSurfaceEmbedConnector();
  ASSERT_NE(nullptr, connector);

  // Wait for initial visual properties to propagate to the child.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return connector->GetLocalFrameSizeInPixelsForTesting() ==
           gfx::Size(150, 150);
  }));

  EXPECT_FALSE(connector->IsThrottledForTesting());
  EXPECT_FALSE(connector->IsSubtreeThrottledForTesting());
  EXPECT_FALSE(connector->IsDisplayLockedForTesting());

  SurfaceEmbedHost* host = GetHost(0);
  ASSERT_NE(nullptr, host);

  host->OnEmbedElementThrottlingStatusChanged(
      mojom::RenderThrottlingStatus::New(
          /*is_throttled=*/true,
          /*subtree_throttled=*/true,
          /*display_locked=*/true));

  EXPECT_TRUE(connector->IsThrottledForTesting());
  EXPECT_TRUE(connector->IsSubtreeThrottledForTesting());
  EXPECT_TRUE(connector->IsDisplayLockedForTesting());
}

// Setting content-visibility:hidden directly on the <embed> element display-
// locks the element in its own document. This lock is not reflected in the
// containing frame's throttling bits, so it exercises the element-level
// display-lock path in WebPluginContainerImpl.
IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest,
                       DisplayLockThrottlingViaContentVisibility) {
  auto child_contents = SetupHarnessAndChild();
  AttachChildToEmbedWithId(child_contents.get(), "my_embed");

  auto* connector = child_contents->GetSurfaceEmbedConnector();
  ASSERT_NE(nullptr, connector);

  // Wait for initial visual properties to propagate to the child.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return connector->GetLocalFrameSizeInPixelsForTesting() ==
           gfx::Size(150, 150);
  }));

  EXPECT_FALSE(connector->IsDisplayLockedForTesting());

  // Display-lock the embed element itself.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.getElementById('my_embed').style.contentVisibility = "
      "'hidden';"));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return connector->IsDisplayLockedForTesting(); }));

  // Unlock and confirm the display-lock status clears.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.getElementById('my_embed').style.contentVisibility = "
      "'visible';"));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !connector->IsDisplayLockedForTesting(); }));
}

// Display-locking the parent embed in the root document propagates display-lock
// throttling to both the parent (directly, via the element-level path) and the
// child (via the frame-level bit pushed down from the parent frame).
IN_PROC_BROWSER_TEST_F(SurfaceEmbedBrowserTest,
                       MultilevelDisplayLockThrottlingViaContentVisibility) {
  NavigateToAttachHarness();

  // Setup Parent (P) WebContents.
  auto parent_contents = CreateChildWebContents();
  NavigateChildToUrl(parent_contents.get(), kAttachHarnessUrl);
  AttachChildToEmbedWithId(parent_contents.get(), "parent_embed");

  // Setup Child (C) WebContents and attach it to P.
  auto child_contents = CreateChildWebContents();
  NavigateChildToUrl(child_contents.get(), kRedBoxUrl);

  guest_contents::GuestContentsHandle* child_guest_handle =
      guest_contents::GuestContentsHandle::CreateForWebContents(
          child_contents.get());
  ASSERT_NE(child_guest_handle, nullptr);
  std::string attach_child_script = "createEmbed('" +
                                    child_guest_handle->id().ToString() +
                                    "', 'child_embed');";
  size_t expected_attachments = GetAttachedHostCount() + 1;
  ASSERT_TRUE(content::ExecJs(parent_contents.get(), attach_child_script));
  ASSERT_TRUE(WaitForHostAttachment(expected_attachments));

  auto* parent_connector = parent_contents->GetSurfaceEmbedConnector();
  auto* child_connector = child_contents->GetSurfaceEmbedConnector();
  ASSERT_NE(nullptr, parent_connector);
  ASSERT_NE(nullptr, child_connector);

  EXPECT_FALSE(parent_connector->IsDisplayLockedForTesting());
  EXPECT_FALSE(child_connector->IsDisplayLockedForTesting());

  // Display-lock the parent embed in the root document.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.getElementById('parent_embed').style.contentVisibility = "
      "'hidden';"));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return parent_connector->IsDisplayLockedForTesting(); }));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return child_connector->IsDisplayLockedForTesting(); }));

  // Unlock and confirm both connectors clear.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.getElementById('parent_embed').style.contentVisibility = "
      "'visible';"));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !parent_connector->IsDisplayLockedForTesting(); }));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !child_connector->IsDisplayLockedForTesting(); }));
}

}  // namespace surface_embed
