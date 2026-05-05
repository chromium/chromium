// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/web_view_focus_helper.h"

#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/browser/focused_node_details.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"

namespace ui_test_utils {

WebViewFocusManager::WebViewFocusManager(views::FocusManager* focus_manager,
                                         content::WebContents* web_contents)
    : focus_manager_(focus_manager) {
  focus_manager->AddFocusChangeListener(this);
  Observe(web_contents);
}
WebViewFocusManager::~WebViewFocusManager() {
  focus_manager_->RemoveFocusChangeListener(this);
}

void WebViewFocusManager::AdvanceFocus(bool reverse) {
  base::Time start = base::Time::Now();
  // Try-bots sometimes don't register these key presses (even with if the
  // view is copyable), so we try multiple times to avoid flakes.
  do {
    run_loop_ = std::make_unique<base::RunLoop>();
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        web_contents()->GetTopLevelNativeWindow(), ui::VKEY_TAB, false, reverse,
        false, false));
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop_->QuitClosure(), base::Seconds(1));
    run_loop_->Run();
    run_loop_.reset();
  } while (!done_ && (base::Time::Now() < start + base::Seconds(5)));
}

void WebViewFocusManager::OnFocusChangedInPage(
    const content::FocusedNodeDetails& details) {
  if (details.node_bounds_in_screen.IsEmpty()) {
    // Focus is leaving the web contents
    return;
  }
  Done();
}

void WebViewFocusManager::OnDidChangeFocus(views::View* focused_before,
                                           views::View* focused_now) {
  Done();
}

void WebViewFocusManager::Done() {
  done_ = true;
  if (run_loop_) {
    run_loop_->Quit();
  }
}

void AdvanceFocus(views::FocusManager* focus_manager, bool reverse) {
  views::View* view = focus_manager->GetFocusedView();
  if (views::WebView* web_view = views::AsViewClass<views::WebView>(view)) {
    WebViewFocusManager helper(focus_manager, web_view->web_contents());
    helper.AdvanceFocus(reverse);
  } else {
    focus_manager->AdvanceFocus(reverse);
  }
}

}  // namespace ui_test_utils
