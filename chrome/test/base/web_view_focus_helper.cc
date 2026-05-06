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

FocusChangeObserver::FocusChangeObserver(views::FocusManager* focus_manager) {
  focus_manager_observation_.Observe(focus_manager);
}

FocusChangeObserver::FocusChangeObserver(
    views::FocusManager* focus_manager,
    const std::vector<content::WebContents*>& web_contents)
    : FocusChangeObserver(focus_manager) {
  for (content::WebContents* wc : web_contents) {
    web_contents_observers_.push_back(
        std::make_unique<WebContentsFocusObserver>(this, wc));
  }
}

FocusChangeObserver::~FocusChangeObserver() = default;

bool FocusChangeObserver::WaitForFocusChange(base::TimeDelta timeout) {
  if (changed_) {
    return true;
  }
  run_loop_ = std::make_unique<base::RunLoop>();
  if (timeout != base::TimeDelta::Max()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop_->QuitClosure(), timeout);
  }
  run_loop_->Run();
  bool result = changed_;
  run_loop_.reset();
  return result;
}

void FocusChangeObserver::OnDidChangeFocus(views::View* focused_before,
                                           views::View* focused_now) {
  OnFocusChanged();
}

void FocusChangeObserver::OnFocusChanged() {
  changed_ = true;
  if (run_loop_ && run_loop_->running()) {
    run_loop_->Quit();
  }
}

FocusChangeObserver::WebContentsFocusObserver::WebContentsFocusObserver(
    FocusChangeObserver* owner,
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents), owner_(owner) {}

FocusChangeObserver::WebContentsFocusObserver::~WebContentsFocusObserver() =
    default;

void FocusChangeObserver::WebContentsFocusObserver::OnFocusChangedInPage(
    const content::FocusedNodeDetails& details) {
  if (details.node_bounds_in_screen.IsEmpty()) {
    // Focus is leaving the web contents
    return;
  }
  owner_->OnFocusChanged();
}

WebViewFocusManager::WebViewFocusManager(views::FocusManager* focus_manager,
                                         content::WebContents* web_contents)
    : focus_manager_(focus_manager), web_contents_(web_contents) {}

WebViewFocusManager::~WebViewFocusManager() = default;

void WebViewFocusManager::AdvanceFocus(bool reverse) {
  base::Time start = base::Time::Now();
  // Try-bots sometimes don't register these key presses (even with if the
  // view is copyable), so we try multiple times to avoid flakes.
  while (base::Time::Now() < start + base::Seconds(5)) {
    FocusChangeObserver obs(focus_manager_, {web_contents_});
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        web_contents_->GetTopLevelNativeWindow(), ui::VKEY_TAB, false, reverse,
        false, false));

    if (obs.WaitForFocusChange(base::Seconds(1))) {
      return;
    }
  }
  GTEST_FAIL() << "Failed to advance focus after 5 seconds.";
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
