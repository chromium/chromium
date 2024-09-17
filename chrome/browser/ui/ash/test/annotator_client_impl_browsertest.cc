// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/annotator/annotator_client_impl.h"

#include <string>

#include "ash/annotator/annotator_controller.h"
#include "ash/public/cpp/annotator/annotations_overlay_view.h"
#include "ash/shell.h"
#include "ash/webui/annotator/annotations_overlay_view_impl.h"
#include "ash/webui/annotator/public/cpp/annotator_client.h"
#include "ash/webui/media_app_ui/buildflags.h"
#include "ash/webui/projector_app/buildflags.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/webview/webview.h"
#include "url/gurl.h"

namespace ash {
class AnnotatorClientTest : public InProcessBrowserTest {
 public:
  AnnotatorClientTest() = default;

  ~AnnotatorClientTest() override = default;
  AnnotatorClientTest(const AnnotatorClientTest&) = delete;
  AnnotatorClientTest& operator=(const AnnotatorClientTest&) = delete;

  // This test helper verifies that navigating to the |url| doesn't result in a
  // 404 error.
  void VerifyUrlValid(const char* url) {
    GURL gurl(url);
    EXPECT_TRUE(gurl.is_valid()) << "url isn't valid: " << url;
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl))
        << "navigating to url failed: " << url;
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(tab->GetController().GetLastCommittedEntry()->GetPageType(),
              content::PAGE_TYPE_NORMAL)
        << "page has unexpected errors: " << url;
  }
};

// This test verifies that the annotator WebUI URL is valid.
IN_PROC_BROWSER_TEST_F(AnnotatorClientTest, AppUrlsValid) {
  VerifyUrlValid(kChromeUIUntrustedAnnotatorUrl);
}

#if BUILDFLAG(ENABLE_CROS_MEDIA_APP) && BUILDFLAG(ENABLE_CROS_PROJECTOR_APP)
// This test verifies that the annotator WebUI URL is valid.
IN_PROC_BROWSER_TEST_F(AnnotatorClientTest, CreateAnnotationsOverlay) {
  // Set callback that is run when the Ink canvas has been initialized.
  base::RunLoop run_loop;
  base::OnceClosure callback = run_loop.QuitClosure();
  Shell::Get()
      ->annotator_controller()
      ->set_canvas_initialized_callback_for_test(std::move(callback));

  // Create overlay. This call loads the ink library inside of the web contents.
  auto overlay = AnnotatorClient::Get()->CreateAnnotationsOverlayView();
  run_loop.Run();

  // Verify that web contents loaded the ink URL.
  AnnotationsOverlayViewImpl* overlay_impl =
      static_cast<AnnotationsOverlayViewImpl*>(overlay.get());
  content::WebContents* web_contents =
      overlay_impl->GetWebViewForTest()->GetWebContents();
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  EXPECT_EQ(web_contents->GetLastCommittedURL(),
            GURL(kChromeUIUntrustedAnnotatorUrl));

  // TODO(b/342104047): Add javascript tests.
}
#endif  // BUILDFLAG(ENABLE_CROS_MEDIA_APP) &&
        // BUILDFLAG(ENABLE_CROS_PROJECTOR_APP)
}  // namespace ash
