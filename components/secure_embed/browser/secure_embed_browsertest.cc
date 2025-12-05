// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "components/guest_contents/browser/guest_contents_handle.h"
#include "components/secure_embed/browser/secure_embed_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/secure_embed_connector.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/text_input_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace secure_embed {

namespace {
constexpr char kAttachHarnessUrl[] =
    "/secure_embed/single_embed_attach_harness.html";
constexpr char kRedBoxUrl[] = "/secure_embed/red_box.html";
constexpr char kBlueBoxUrl[] = "/secure_embed/blue_box.html";
constexpr char kEmptyUrl[] = "/secure_embed/empty.html";
}  // namespace

class SecureEmbedBrowserTest : public content::ContentBrowserTest {
 public:
  // RAII helper for monitoring data-content-id attribute changes.
  class ScopedDataContentIdMonitor {
   public:
    enum class ExpectZeroChange { kNo, kYes };

    ScopedDataContentIdMonitor(content::WebContents* web_contents,
                               ExpectZeroChange expect_zero)
        : web_contents_(web_contents), expect_zero_(expect_zero) {
      EXPECT_TRUE(
          content::ExecJs(web_contents_, "startMonitoringDataContentId();"));
    }

    ~ScopedDataContentIdMonitor() {
      bool zero_detected =
          content::EvalJs(web_contents_, "wasZeroChangeDetected()")
              .ExtractBool();

      if (expect_zero_ == ExpectZeroChange::kYes) {
        // Wait for zero change if we expect it but haven't seen it yet.
        if (!zero_detected) {
          EXPECT_TRUE(base::test::RunUntil([&]() {
            return content::EvalJs(web_contents_, "wasZeroChangeDetected()")
                .ExtractBool();
          })) << "data-content-id was not set to 0 as expected";
        }
      } else {
        EXPECT_FALSE(zero_detected)
            << "data-content-id was unexpectedly set to 0 (DetachedByHost "
               "incorrectly triggered)";
      }

      EXPECT_TRUE(
          content::ExecJs(web_contents_, "stopMonitoringDataContentId();"));
    }

    ScopedDataContentIdMonitor(const ScopedDataContentIdMonitor&) = delete;
    ScopedDataContentIdMonitor& operator=(const ScopedDataContentIdMonitor&) =
        delete;

   private:
    raw_ptr<content::WebContents> web_contents_;
    ExpectZeroChange expect_zero_;
  };

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    // Enable pixel output in tests to allow CopyFromSurface to capture actual
    // rendered content instead of returning empty/black bitmaps. Note that we
    // force a device scale factor of 1.5 to ensure scaling is handled
    // correctly.
    EnablePixelOutput(1.5f);
  }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("b.com", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* web_contents() { return shell()->web_contents(); }

  void NavigateToTestUrl(const char* url) {
    const GURL test_url = embedded_test_server()->GetURL(url);
    ASSERT_TRUE(NavigateToURL(web_contents(), test_url));
  }

  int CountEmbedElementsInPage() {
    return content::EvalJs(web_contents(), "document.embeds.length")
        .ExtractInt();
  }

  bool WaitForHostCreation() {
    // Host creation is asynchronous because it requires mojo IPC between
    // the renderer process (SecureEmbedWebPlugin) and browser process
    // (SecureEmbedHost). Poll until the expected number of hosts are created.
    return base::test::RunUntil(
        [&]() { return SecureEmbedHost::GetInstanceCountForTesting() >= 1; });
  }

  bool WaitForHostAttachment(size_t expected_count) {
    // After host creation, the attachment happens asychronously when
    // SecureEmbedHost::AttachConnector() is called. Poll until the expected
    // number of hosts are attached.
    return base::test::RunUntil([&]() {
      return SecureEmbedHost::GetAttachedInstanceCountForTesting() >=
             expected_count;
    });
  }

  // Helper to verify pixel colors at specific locations within a capture rect.
  // Polls repeatedly until all expected colors match or timeout occurs.
  // The points in `expected_colors` should be specified in device independent
  // pixels relative to the capture_rect origin. They will be scaled by the
  // device scale factor internally since the bitmap from CopyFromSurface is in
  // physical pixels. Note that CopyFromSurface itself takes the capture_rect in
  // device independent pixels. Returns true if all pixels match expected
  // colors.
  bool VerifyPixelColors(
      content::RenderWidgetHostView* rwhv,
      const gfx::Rect& capture_rect,
      const std::vector<std::pair<gfx::Point, SkColor>>& expected_colors) {
    float device_scale_factor = rwhv->GetDeviceScaleFactor();
    bool all_pixels_correct = false;
    return base::test::RunUntil([&]() {
      base::RunLoop run_loop;

      // Passing in gfx::Size() to avoid scaling the captured rect, specified
      // via capture_rect, to a different size if the underlying device is doing
      // pixel scaling.
      rwhv->CopyFromSurface(
          capture_rect, gfx::Size(),
          base::BindLambdaForTesting(
              [quit_closure = run_loop.QuitClosure(), &all_pixels_correct,
               device_scale_factor, &expected_colors](const SkBitmap& bitmap) {
                all_pixels_correct = false;

                if (bitmap.drawsNothing()) {
                  std::move(quit_closure).Run();
                  return;
                }

                bool all_match = true;
                for (const auto& [point, expected_color] : expected_colors) {
                  // Scale the point by device scale factor to get physical
                  // pixel coordinates in the bitmap.
                  int physical_x =
                      static_cast<int>(point.x() * device_scale_factor);
                  int physical_y =
                      static_cast<int>(point.y() * device_scale_factor);
                  SkColor actual_color =
                      bitmap.getColor(physical_x, physical_y);
                  if (actual_color != expected_color) {
                    all_match = false;
                    break;
                  }
                }

                if (all_match) {
                  all_pixels_correct = true;
                }

                std::move(quit_closure).Run();
              }));

      run_loop.Run();
      return all_pixels_correct;
    });
  }

  // Navigate the main WebContents to the attach harness page.
  void NavigateToAttachHarness() {
    const GURL harness_url = embedded_test_server()->GetURL(kAttachHarnessUrl);
    ASSERT_TRUE(NavigateToURL(web_contents(), harness_url));
  }

  std::unique_ptr<content::WebContents> CreateGuestWebContents() {
    content::WebContents::CreateParams create_params(
        shell()->web_contents()->GetBrowserContext());
    std::unique_ptr<content::WebContents> guest_contents =
        content::WebContents::Create(create_params);
    EXPECT_NE(guest_contents, nullptr);
    return guest_contents;
  }

  // Navigate guest WebContents to a URL. Wait for load to complete.
  void NavigateGuestToUrl(content::WebContents* guest_contents,
                          const std::string& path) {
    const GURL guest_url = embedded_test_server()->GetURL(path);
    ASSERT_TRUE(NavigateToURL(guest_contents, guest_url));
    ASSERT_TRUE(content::WaitForLoadStop(guest_contents));
  }

  // Attach a guest to an embed element and wait for SecureEmbedHost creation.
  void AttachGuestToEmbed(content::WebContents* guest_contents) {
    AttachGuestToEmbedWithId(guest_contents, std::nullopt);
  }

  // Attach a guest to an embed element with an optional ID.
  void AttachGuestToEmbedWithId(content::WebContents* guest_contents,
                                std::optional<std::string> embed_id) {
    guest_contents::GuestContentsHandle* guest_handle =
        guest_contents::GuestContentsHandle::CreateForWebContents(
            guest_contents);
    ASSERT_NE(guest_handle, nullptr);
    std::string script =
        "createEmbed(" + base::NumberToString(guest_handle->id());
    if (embed_id.has_value()) {
      script += ", '" + embed_id.value() + "'";
    }
    script += ");";
    ASSERT_TRUE(content::ExecJs(web_contents(), script));
    ASSERT_TRUE(WaitForHostAttachment(1));
    EXPECT_NE(guest_contents->GetSecureEmbedConnector(), nullptr);
  }

  // Get the guest handle for the provided guest WebContents.
  int GetGuestHandleId(content::WebContents* guest_contents) {
    guest_contents::GuestContentsHandle* guest_handle =
        guest_contents::GuestContentsHandle::CreateForWebContents(
            guest_contents);
    EXPECT_NE(guest_handle, nullptr);
    return guest_handle ? guest_handle->id() : -1;
  }

  // Setup guest with harness navigation and content loading.
  std::unique_ptr<content::WebContents> SetupHarnessAndGuestWithContent(
      const std::string& guest_path) {
    NavigateToAttachHarness();
    auto guest_contents = CreateGuestWebContents();
    NavigateGuestToUrl(guest_contents.get(), guest_path);
    return guest_contents;
  }

  // Verify box rendering at standard location with specified color. The box is
  // 50x50 CSS pixels at position (10, 10).
  void VerifyBoxRendering(SkColor box_color) {
    auto* rwhv = web_contents()->GetRenderWidgetHostView();
    ASSERT_NE(rwhv, nullptr);

    gfx::Rect capture_rect(0, 0, 70, 70);
    std::vector<std::pair<gfx::Point, SkColor>> expected_colors = {
        {gfx::Point(0, 0), SK_ColorWHITE},
        {gfx::Point(10, 10), box_color},
        {gfx::Point(59, 59), box_color},
        {gfx::Point(60, 60), SK_ColorWHITE},
    };
    EXPECT_TRUE(VerifyPixelColors(rwhv, capture_rect, expected_colors));
  }
};

IN_PROC_BROWSER_TEST_F(SecureEmbedBrowserTest,
                       PluginRendersRedSquare_AttachBeforeLoad) {
  NavigateToAttachHarness();
  auto guest_contents = CreateGuestWebContents();

  // Attach before loading content.
  AttachGuestToEmbed(guest_contents.get());

  // Now navigate the guest to the red box page after attachment.
  NavigateGuestToUrl(guest_contents.get(), kRedBoxUrl);

  VerifyBoxRendering(SK_ColorRED);
}

IN_PROC_BROWSER_TEST_F(SecureEmbedBrowserTest,
                       PluginRendersRedSquare_AttachAfterLoad) {
  // Load content before attaching.
  auto guest_contents = SetupHarnessAndGuestWithContent(kRedBoxUrl);
  AttachGuestToEmbed(guest_contents.get());

  VerifyBoxRendering(SK_ColorRED);
}

IN_PROC_BROWSER_TEST_F(SecureEmbedBrowserTest, Crash) {
  auto guest_contents = SetupHarnessAndGuestWithContent(kRedBoxUrl);
  AttachGuestToEmbed(guest_contents.get());

  VerifyBoxRendering(SK_ColorRED);

  // Simulate a crash.
  content::ScopedAllowRendererCrashes testing_crashes_here(
      guest_contents->GetPrimaryMainFrame());
  guest_contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      content::RESULT_CODE_KILLED);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return guest_contents->IsCrashed(); }));
  // The crashed frame gets drawn with a gray background, with an image
  // in the middle (which doesn't seem configured for tests). The gray in
  // question is a bit different than SK_ColorGRAY.
  gfx::Rect capture_rect(0, 0, 10, 10);
  std::vector<std::pair<gfx::Point, SkColor>> expected_colors = {
      {gfx::Point(0, 0), SkColors::kGray.toSkColor()},
      {gfx::Point(5, 5), SkColors::kGray.toSkColor()},
  };
  auto* rwhv = web_contents()->GetRenderWidgetHostView();
  ASSERT_NE(rwhv, nullptr);
  EXPECT_TRUE(VerifyPixelColors(rwhv, capture_rect, expected_colors));
}

IN_PROC_BROWSER_TEST_F(SecureEmbedBrowserTest,
                       PluginRendersRedSquare_MultipleAttachCalls) {
  NavigateToAttachHarness();

  // Create and load both guests.
  auto guest_contents_red = CreateGuestWebContents();
  NavigateGuestToUrl(guest_contents_red.get(), kRedBoxUrl);
  int guest_id_red = GetGuestHandleId(guest_contents_red.get());

  auto guest_contents_blue = CreateGuestWebContents();
  NavigateGuestToUrl(guest_contents_blue.get(), kBlueBoxUrl);
  int guest_id_blue = GetGuestHandleId(guest_contents_blue.get());

  // Attach the red guest first.
  AttachGuestToEmbed(guest_contents_red.get());
  VerifyBoxRendering(SK_ColorRED);

  ScopedDataContentIdMonitor monitor(
      web_contents(), ScopedDataContentIdMonitor::ExpectZeroChange::kNo);

  // Swap to the blue guest by changing the data-content-id attribute.
  std::string script_blue =
      "setDataContentId(" + base::NumberToString(guest_id_blue) + ");";
  ASSERT_TRUE(content::ExecJs(web_contents(), script_blue));
  VerifyBoxRendering(SK_ColorBLUE);
  EXPECT_EQ(guest_contents_red->GetSecureEmbedConnector(), nullptr);

  // Swap back to red guest.
  std::string script_red_again =
      "setDataContentId(" + base::NumberToString(guest_id_red) + ");";
  ASSERT_TRUE(content::ExecJs(web_contents(), script_red_again));
  VerifyBoxRendering(SK_ColorRED);
  EXPECT_EQ(guest_contents_blue->GetSecureEmbedConnector(), nullptr);
}

IN_PROC_BROWSER_TEST_F(SecureEmbedBrowserTest, VisibilityHiddenStopsRendering) {
  auto guest_contents = SetupHarnessAndGuestWithContent(kRedBoxUrl);
  AttachGuestToEmbed(guest_contents.get());
  VerifyBoxRendering(SK_ColorRED);

  // Set visibility to hidden - guest should stop rendering.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.querySelector('embed').style.visibility = 'hidden';"));
  VerifyBoxRendering(SK_ColorWHITE);

  // Set visibility back to visible - guest should start rendering again.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.querySelector('embed').style.visibility = 'visible';"));
  VerifyBoxRendering(SK_ColorRED);
}

IN_PROC_BROWSER_TEST_F(SecureEmbedBrowserTest, DisplayNoneStopsRendering) {
  auto guest_contents = SetupHarnessAndGuestWithContent(kRedBoxUrl);
  AttachGuestToEmbed(guest_contents.get());
  VerifyBoxRendering(SK_ColorRED);

  // Set display to none - guest should stop rendering.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.querySelector('embed').style.display = 'none';"));
  VerifyBoxRendering(SK_ColorWHITE);

  // Set display back to block - guest should start rendering again.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.querySelector('embed').style.display = 'block';"));
  VerifyBoxRendering(SK_ColorRED);
}

// TODO(secure-embed): This test fails right now because attribute changes do
// not trigger re-attaches.
IN_PROC_BROWSER_TEST_F(SecureEmbedBrowserTest, VisibilityHiddenSwapGuest) {
  NavigateToAttachHarness();

  // Create and load both guests.
  auto guest_contents_red = CreateGuestWebContents();
  NavigateGuestToUrl(guest_contents_red.get(), kRedBoxUrl);

  auto guest_contents_blue = CreateGuestWebContents();
  NavigateGuestToUrl(guest_contents_blue.get(), kBlueBoxUrl);
  int guest_id_blue = GetGuestHandleId(guest_contents_blue.get());

  // Attach the red guest first.
  AttachGuestToEmbed(guest_contents_red.get());
  VerifyBoxRendering(SK_ColorRED);

  // Set visibility to hidden - guest should stop rendering.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.querySelector('embed').style.visibility = 'hidden';"));
  VerifyBoxRendering(SK_ColorWHITE);

  ScopedDataContentIdMonitor monitor(
      web_contents(), ScopedDataContentIdMonitor::ExpectZeroChange::kNo);

  // Swap to the blue guest while hidden.
  std::string script_blue =
      "setDataContentId(" + base::NumberToString(guest_id_blue) + ");";
  ASSERT_TRUE(content::ExecJs(web_contents(), script_blue));
  VerifyBoxRendering(SK_ColorWHITE);

  // Set visibility back to visible - blue guest should now be rendering.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.querySelector('embed').style.visibility = 'visible';"));
  VerifyBoxRendering(SK_ColorBLUE);
}

IN_PROC_BROWSER_TEST_F(SecureEmbedBrowserTest, DisplayNoneSwapGuest) {
  NavigateToAttachHarness();

  // Create and load both guests.
  auto guest_contents_red = CreateGuestWebContents();
  NavigateGuestToUrl(guest_contents_red.get(), kRedBoxUrl);

  auto guest_contents_blue = CreateGuestWebContents();
  NavigateGuestToUrl(guest_contents_blue.get(), kBlueBoxUrl);
  int guest_id_blue = GetGuestHandleId(guest_contents_blue.get());

  // Attach the red guest first.
  AttachGuestToEmbed(guest_contents_red.get());
  VerifyBoxRendering(SK_ColorRED);

  // Set display to none - guest should stop rendering.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.querySelector('embed').style.display = 'none';"));
  VerifyBoxRendering(SK_ColorWHITE);

  ScopedDataContentIdMonitor monitor(
      web_contents(), ScopedDataContentIdMonitor::ExpectZeroChange::kNo);

  // Swap to the blue guest while display is none.
  std::string script_blue =
      "setDataContentId(" + base::NumberToString(guest_id_blue) + ");";
  ASSERT_TRUE(content::ExecJs(web_contents(), script_blue));
  VerifyBoxRendering(SK_ColorWHITE);

  // Set display back to block - blue guest should now be rendering.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.querySelector('embed').style.display = 'block';"));
  VerifyBoxRendering(SK_ColorBLUE);
}

// Create 2 <embed>s that use the same content id. The 2nd <embed> should show
// and the 1st <embed> should be blank.
IN_PROC_BROWSER_TEST_F(SecureEmbedBrowserTest, TwoEmbedSameContentId) {
  NavigateToAttachHarness();

  // Create and load both guests.
  auto guest_contents_red = CreateGuestWebContents();
  NavigateGuestToUrl(guest_contents_red.get(), kRedBoxUrl);
  int guest_id_red = GetGuestHandleId(guest_contents_red.get());

  // Attach the red guest first.
  AttachGuestToEmbedWithId(guest_contents_red.get(), "embed1");
  VerifyBoxRendering(SK_ColorRED);

  // Cache the first host's delegate pointer before forced detachment.
  auto* connector = guest_contents_red->GetSecureEmbedConnector();
  ASSERT_NE(connector, nullptr);
  auto* first_host_delegate = connector->GetDelegate();
  ASSERT_NE(first_host_delegate, nullptr);
  EXPECT_TRUE(first_host_delegate->IsAttachedForTesting());

  // The first embed's data-content-id should be reset to 0 since it was
  // forcibly detached when the guest was attached to the 2nd embed.
  ScopedDataContentIdMonitor monitor(
      web_contents(), ScopedDataContentIdMonitor::ExpectZeroChange::kYes);

  // Add a 2nd <embed> with ID 'embed2' that uses the same content id.
  AttachGuestToEmbedWithId(guest_contents_red.get(), "embed2");

  // Then verify the color of the 1st <embed>, which should become blank.
  VerifyBoxRendering(SK_ColorWHITE);

  // The first host should no longer think it's attached due to the forced
  // detachment by being attached to the 2nd <embed>.
  EXPECT_FALSE(first_host_delegate->IsAttachedForTesting());
  auto* second_connector = guest_contents_red->GetSecureEmbedConnector();
  ASSERT_NE(second_connector, nullptr);
  EXPECT_TRUE(second_connector->GetDelegate()->IsAttachedForTesting());

  // Try to reattach to the first embed by setting its data-content-id back to
  // the original guest ID to ensure that a forced detachment doesn't prevent
  // subsequent re-attachment.
  std::string script_reattach_first =
      "setDataContentId(" + base::NumberToString(guest_id_red) + ", 'embed1');";
  ASSERT_TRUE(content::ExecJs(web_contents(), script_reattach_first));
  VerifyBoxRendering(SK_ColorRED);
  EXPECT_TRUE(first_host_delegate->IsAttachedForTesting());
  EXPECT_TRUE(content::EvalJs(web_contents(), "getDataContentId('embed2')")
                  .ExtractString() == "0");
}

IN_PROC_BROWSER_TEST_F(SecureEmbedBrowserTest, ReattachSameGuestToNewEmbed) {
  auto guest_contents = SetupHarnessAndGuestWithContent(kRedBoxUrl);
  int guest_id = GetGuestHandleId(guest_contents.get());

  AttachGuestToEmbed(guest_contents.get());
  VerifyBoxRendering(SK_ColorRED);

  // Destroy the embed element.
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "document.querySelector('embed').remove();"));
  VerifyBoxRendering(SK_ColorWHITE);

  // Create a new embed element and attach the same guest.
  std::string script = "createEmbed(" + base::NumberToString(guest_id) + ");";
  ASSERT_TRUE(content::ExecJs(web_contents(), script));
  VerifyBoxRendering(SK_ColorRED);
}

#if defined(USE_AURA)  // BoundingBoxUpdateWaiter is aura-specific.

// Test that the proper (top-level) TextInputManager is used for embedded
// WebContents. This tests triggers a selection change at top-level when the
// embed is focused, which used to crash on querying the selection state.
// (In particular, platforms with a selection clipboard buffer --- e.g. X11 ---
//  query TextInputManager's selection state to potentially update the
//  clipboard; this test exercises a similar code path via
//  BoundingBoxUpdateWaiter querying the selection's bounding box).
IN_PROC_BROWSER_TEST_F(SecureEmbedBrowserTest, TextInputManager) {
  auto guest_contents = SetupHarnessAndGuestWithContent(kRedBoxUrl);
  AttachGuestToEmbed(guest_contents.get());
  content::TextInputManagerTester input_tester(web_contents());
  content::TextInputManagerTester guest_input_tester(guest_contents.get());

  const char kAddTextScript[] = R"(
     let text = document.createTextNode("Some text to select");
     document.body.appendChild(text);
  )";
  ASSERT_TRUE(content::ExecJs(web_contents(), kAddTextScript));
  ASSERT_TRUE(content::ExecJs(guest_contents.get(), kAddTextScript));

  const char kFocusScript[] = R"(
    document.getElementsByTagName("embed")[0].focus();
  )";

  ASSERT_TRUE(content::ExecJs(web_contents(), kFocusScript));

  const char kSelectionChange[] = R"(
     let range = document.createRange();
     range.selectNodeContents(document.body);
     window.getSelection().addRange(range);
  )";

  // BoundingBoxUpdateWaiter asks the top-level for active frame's selection
  // change whenever the top-level's selection changes, so we need to have
  // both frames change their selections.
  content::BoundingBoxUpdateWaiter waiter(web_contents());
  ASSERT_TRUE(content::ExecJs(guest_contents.get(), kSelectionChange));
  ASSERT_TRUE(content::ExecJs(web_contents(), kSelectionChange));

  waiter.Wait();

  // If the same TextInputManager is used for the guest as top-level, they'll
  // have the same option of last updated view.
  EXPECT_EQ(input_tester.GetUpdatedView(), guest_input_tester.GetUpdatedView());
}

#endif

IN_PROC_BROWSER_TEST_F(SecureEmbedBrowserTest, Detach) {
  auto guest_contents = SetupHarnessAndGuestWithContent(kRedBoxUrl);
  AttachGuestToEmbed(guest_contents.get());

  VerifyBoxRendering(SK_ColorRED);

  // Change the data-content-id attribute to 0 to trigger a detach.
  std::string script_cause_detach = "setDataContentId(0);";
  ASSERT_TRUE(content::ExecJs(web_contents(), script_cause_detach));
  EXPECT_EQ(guest_contents->GetSecureEmbedConnector(), nullptr);
  VerifyBoxRendering(SK_ColorWHITE);

  // Re-attach the same guest.
  int guest_id = GetGuestHandleId(guest_contents.get());
  std::string script_reattach =
      "setDataContentId(" + base::NumberToString(guest_id) + ");";
  ASSERT_TRUE(content::ExecJs(web_contents(), script_reattach));
  VerifyBoxRendering(SK_ColorRED);
  EXPECT_NE(guest_contents->GetSecureEmbedConnector(), nullptr);
}

IN_PROC_BROWSER_TEST_F(SecureEmbedBrowserTest, GuestWithCrossSiteIframe) {
  // If a OOPIF is present in the guest and sets up its TextInputManager before
  // the embed is attached, we used to hit a DCHECK failure while attaching as
  // all of the views that are part of the guest's frame tree must share the
  // same TextInputManager as the embed's WebContents.
  NavigateToAttachHarness();
  auto guest_contents = CreateGuestWebContents();

  NavigateGuestToUrl(guest_contents.get(), kEmptyUrl);

  GURL iframe_url = embedded_test_server()->GetURL("b.com", kRedBoxUrl);
  std::string create_iframe_script =
      "const iframe = document.createElement('iframe');"
      "iframe.src = '" +
      iframe_url.spec() +
      "';"
      "document.body.appendChild(iframe);";
  ASSERT_TRUE(content::ExecJs(guest_contents.get(), create_iframe_script));

  ASSERT_TRUE(content::WaitForLoadStop(guest_contents.get()));

  content::RenderFrameHost* main_frame = guest_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* iframe = ChildFrameAt(main_frame, 0);
  ASSERT_NE(iframe, nullptr);
  EXPECT_NE(main_frame->GetProcess(), iframe->GetProcess());

  AttachGuestToEmbed(guest_contents.get());
  VerifyBoxRendering(SK_ColorRED);
}

IN_PROC_BROWSER_TEST_F(SecureEmbedBrowserTest,
                       DetachAndReattachGuestWithCrossSiteIframe) {
  // If a OOPIF is present in the guest and sets up its TextInputManager before
  // the embed is attached, we used to hit a DCHECK failure while attaching as
  // all of the views that are part of the guest's frame tree must share the
  // same TextInputManager as the embed's WebContents. This test is verifying
  // that detach and re-attach also work correctly in this scenario.
  NavigateToAttachHarness();
  auto guest_contents = CreateGuestWebContents();

  NavigateGuestToUrl(guest_contents.get(), kEmptyUrl);

  GURL iframe_url = embedded_test_server()->GetURL("b.com", kRedBoxUrl);
  std::string create_iframe_script =
      "const iframe = document.createElement('iframe');"
      "iframe.src = '" +
      iframe_url.spec() +
      "';"
      "document.body.appendChild(iframe);";
  ASSERT_TRUE(content::ExecJs(guest_contents.get(), create_iframe_script));

  ASSERT_TRUE(content::WaitForLoadStop(guest_contents.get()));

  content::RenderFrameHost* main_frame = guest_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* iframe = ChildFrameAt(main_frame, 0);
  ASSERT_NE(iframe, nullptr);
  EXPECT_NE(main_frame->GetProcess(), iframe->GetProcess());

  AttachGuestToEmbed(guest_contents.get());
  VerifyBoxRendering(SK_ColorRED);

  // Change the data-content-id attribute to 0 to trigger a detach.
  std::string script_cause_detach = "setDataContentId(0);";
  ASSERT_TRUE(content::ExecJs(web_contents(), script_cause_detach));
  EXPECT_EQ(guest_contents->GetSecureEmbedConnector(), nullptr);
  VerifyBoxRendering(SK_ColorWHITE);

  // Reattach the same guest.
  int guest_id = GetGuestHandleId(guest_contents.get());
  std::string script_reattach =
      "setDataContentId(" + base::NumberToString(guest_id) + ");;";
  ASSERT_TRUE(content::ExecJs(web_contents(), script_reattach));
  VerifyBoxRendering(SK_ColorRED);
}

IN_PROC_BROWSER_TEST_F(SecureEmbedBrowserTest, InvalidContentIdNegative) {
  // Test that specifying an invalid (negative) content_id creates a host but
  // doesn't attach any guest, resulting in white rendering.
  NavigateToAttachHarness();

  std::string script = "createEmbed(-1);";
  ASSERT_TRUE(content::ExecJs(web_contents(), script));

  // Host should be created even with invalid content_id but no attach should
  // happen.
  ASSERT_TRUE(WaitForHostCreation());
  EXPECT_EQ(1u, SecureEmbedHost::GetInstanceCountForTesting());
  EXPECT_EQ(0u, SecureEmbedHost::GetAttachedInstanceCountForTesting());
  VerifyBoxRendering(SK_ColorWHITE);
}

IN_PROC_BROWSER_TEST_F(SecureEmbedBrowserTest,
                       AttachValidGuestThenUpdateToNegativeContentId) {
  // Test that attaching a valid guest shows its content, but updating to a
  // negative content_id detaches and results in white rendering.
  auto guest_contents = SetupHarnessAndGuestWithContent(kRedBoxUrl);
  AttachGuestToEmbed(guest_contents.get());

  VerifyBoxRendering(SK_ColorRED);
  EXPECT_NE(guest_contents->GetSecureEmbedConnector(), nullptr);

  // Update the data-content-id to a negative number to trigger detachment.
  std::string script_detach = "setDataContentId(-5);";
  ASSERT_TRUE(content::ExecJs(web_contents(), script_detach));

  VerifyBoxRendering(SK_ColorWHITE);
  EXPECT_EQ(guest_contents->GetSecureEmbedConnector(), nullptr);
}

}  // namespace secure_embed
