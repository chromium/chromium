// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/test/base/in_process_browser_test.h"

#include <stddef.h>
#include <string.h>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "net/dns/public/resolve_error_info.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"
#endif

namespace {

class InProcessBrowserTestP
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<const char*> {
};

IN_PROC_BROWSER_TEST_P(InProcessBrowserTestP, TestP) {
  EXPECT_EQ(0, strcmp("foo", GetParam()));
}

INSTANTIATE_TEST_SUITE_P(IPBTP,
                         InProcessBrowserTestP,
                         ::testing::Values("foo"));

// WebContents observer that can detect provisional load failures.
class LoadFailObserver : public content::WebContentsObserver {
 public:
  explicit LoadFailObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents) {}
  LoadFailObserver(const LoadFailObserver&) = delete;
  LoadFailObserver& operator=(const LoadFailObserver&) = delete;

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetNetErrorCode() == net::OK)
      return;

    failed_load_ = true;
    error_code_ = navigation_handle->GetNetErrorCode();
    resolve_error_info_ = navigation_handle->GetResolveErrorInfo();
    validated_url_ = navigation_handle->GetURL();
  }

  bool failed_load() const { return failed_load_; }
  net::Error error_code() const { return error_code_; }
  net::ResolveErrorInfo resolve_error_info() const {
    return resolve_error_info_;
  }
  const GURL& validated_url() const { return validated_url_; }

 private:
  bool failed_load_ = false;
  net::Error error_code_ = net::OK;
  net::ResolveErrorInfo resolve_error_info_ = net::ResolveErrorInfo(net::OK);
  GURL validated_url_;
};

// Tests that InProcessBrowserTest cannot resolve external host, in this case
// "google.com" and "cnn.com". Using external resources is disabled by default
// in InProcessBrowserTest because it causes flakiness.
IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, ExternalConnectionFail) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  const char* const kURLs[] = {
    "http://www.google.com/",
    "http://www.cnn.com/"
  };
  for (size_t i = 0; i < std::size(kURLs); ++i) {
    GURL url(kURLs[i]);
    LoadFailObserver observer(contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_TRUE(observer.failed_load());
    EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, observer.error_code());
    EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, observer.resolve_error_info().error);
    EXPECT_EQ(url, observer.validated_url());
  }
}

// Verify that AfterStartupTaskUtils considers startup to be complete
// prior to test execution so tasks posted by tests are never deferred.
IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, AfterStartupTaskUtils) {
  EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
}

// On Mac this crashes inside cc::SingleThreadProxy::SetNeedsCommit. See
// https://ci.chromium.org/b/8923336499994443392
#if !BUILDFLAG(IS_MAC)
class SingleProcessBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
  }
};

// TODO(crbug.com/40190525): Flaky / times out on many bots.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
IN_PROC_BROWSER_TEST_F(SingleProcessBrowserTest, MAYBE_Test) {
  // Should not crash.
}

#endif

#if defined(TOOLKIT_VIEWS)

namespace {

class LayoutTrackingView : public views::View {
  METADATA_HEADER(LayoutTrackingView, views::View)

 public:
  LayoutTrackingView() = default;
  ~LayoutTrackingView() override = default;

  void ResetLayoutCount() { layout_count_ = 0; }
  int layout_count() const { return layout_count_; }

  // views::View:
  void Layout(PassKey) override {
    ++layout_count_;
    LayoutSuperclass<views::View>(this);
  }

 private:
  int layout_count_ = 0;
};

BEGIN_METADATA(LayoutTrackingView)
END_METADATA

}  // namespace

IN_PROC_BROWSER_TEST_F(InProcessBrowserTest,
                       RunsScheduledLayoutOnAnchoredBubbles) {
  views::View* const anchor_view =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->app_menu_button();

  // Temporarily owned.
  views::BubbleDialogDelegateView* const bubble =
      new views::BubbleDialogDelegateView(anchor_view,
                                          views::BubbleBorder::TOP_RIGHT);
  LayoutTrackingView* layout_tracker =
      bubble->AddChildView(std::make_unique<LayoutTrackingView>());

  // Takes ownership.
  views::Widget* const bubble_widget =
      views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble_widget->Show();

  layout_tracker->ResetLayoutCount();
  layout_tracker->InvalidateLayout();
  EXPECT_EQ(layout_tracker->layout_count(), 0);

  RunScheduledLayouts();
  EXPECT_GT(layout_tracker->layout_count(), 0);
}

#endif  // defined(TOOLKIT_VIEWS)

}  // namespace
