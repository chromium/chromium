// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/popup_test_base.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

namespace {

// A helper to wait for Browser window bounds changes beyond given thresholds.
class BoundsChangeWaiter final : public views::WidgetObserver {
 public:
  BoundsChangeWaiter(Browser* browser, int move_by, int resize_by)
      : widget_(views::Widget::GetWidgetForNativeWindow(
            browser->window()->GetNativeWindow())),
        move_by_(move_by),
        resize_by_(resize_by),
        initial_bounds_(widget_->GetWindowBoundsInScreen()) {
    widget_->AddObserver(this);
  }

  BoundsChangeWaiter(const BoundsChangeWaiter&) = delete;
  BoundsChangeWaiter& operator=(const BoundsChangeWaiter&) = delete;
  ~BoundsChangeWaiter() final { widget_->RemoveObserver(this); }

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& rect) final {
    if (BoundsChangeMeetsThreshold(rect)) {
      widget_->RemoveObserver(this);
      run_loop_.Quit();
    }
  }

  // Wait for changes to occur, or return immediately if they already have.
  void Wait() {
    if (!BoundsChangeMeetsThreshold(widget_->GetWindowBoundsInScreen())) {
      run_loop_.Run();
    }
  }

 private:
  bool BoundsChangeMeetsThreshold(const gfx::Rect& rect) const {
    return (std::abs(rect.x() - initial_bounds_.x()) >= move_by_ ||
            std::abs(rect.y() - initial_bounds_.y()) >= move_by_) &&
           (std::abs(rect.width() - initial_bounds_.width()) >= resize_by_ ||
            std::abs(rect.height() - initial_bounds_.height()) >= resize_by_);
  }

  const raw_ptr<views::Widget> widget_;
  const int move_by_, resize_by_;
  const gfx::Rect initial_bounds_;
  base::RunLoop run_loop_;
};

}  // namespace

void PopupTestBase::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitch(embedder_support::kDisablePopupBlocking);
}

// static
Browser* PopupTestBase::OpenPopup(Browser* browser, const std::string& script) {
  return OpenPopup(browser->tab_strip_model()->GetActiveWebContents(), script);
}

// static
Browser* PopupTestBase::OpenPopup(const content::ToRenderFrameHost& adapter,
                                  const std::string& script) {
  content::ExecuteScriptAsync(adapter, script);
  Browser* popup = ui_test_utils::WaitForBrowserToOpen();
  content::WebContents* popup_contents =
      popup->tab_strip_model()->GetActiveWebContents();
  // The popup's bounds are initialized after the synchronous window.open().
  // Ideally, this might wait for browser->renderer window bounds init via:
  // blink::mojom::Widget.UpdateVisualProperties, but it seems sufficient to
  // wait for WebContents to load the URL after the initial about:blank doc,
  // and then for that Document's readyState to be 'complete'. Anecdotally,
  // initial bounds seem settled once outerWidth and outerHeight are non-zero.
  EXPECT_TRUE(WaitForLoadStop(popup_contents));
  EXPECT_TRUE(WaitForRenderFrameReady(popup_contents->GetPrimaryMainFrame()));
  EXPECT_NE("0x0", EvalJs(popup_contents, "outerWidth + 'x' + outerHeight"));
  return popup;
}

// static
void PopupTestBase::WaitForBoundsChange(Browser* browser,
                                        int move_by,
                                        int resize_by) {
  BoundsChangeWaiter(browser, move_by, resize_by).Wait();
}

// static
void PopupTestBase::SetUpWindowManagement(Browser* browser) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  // Request and auto-accept the permission request.
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  ASSERT_GT(EvalJs(web_contents,
                   R"JS(getScreenDetails().then(s => {
                          window.screenDetails = s;
                          return s.screens.length; }))JS"),
            0);
  // Do not auto-accept any other permission requests.
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::NONE);
}

// static
display::Display PopupTestBase::GetDisplayNearestBrowser(
    const Browser* browser) {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(
      browser->window()->GetNativeWindow());
}

// static
void PopupTestBase::WaitForHTMLFullscreen(content::WebContents* web_contents) {
  content::WaitForLoadStop(web_contents);
  ASSERT_TRUE(EvalJs(web_contents, R"JS(
        (new Promise((resolve, reject) => {
          if (!!document.fullscreenElement) {
            resolve();
          } else {
            document.addEventListener(`fullscreenchange`,
              () => { if (!!document.fullscreenElement) resolve(); }
            );
            document.addEventListener(`fullscreenerror`, e => { reject(e); });
          }
        })))JS")
                  .error.empty());
}
