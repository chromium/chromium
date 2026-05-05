// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_WEB_VIEW_FOCUS_HELPER_H_
#define CHROME_TEST_BASE_WEB_VIEW_FOCUS_HELPER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/focus/focus_manager.h"

namespace ui_test_utils {

// A helper to manage focus advancement (Tab/Shift-Tab) when WebViews are
// involved. WebViews often need special handling for focus traversal.
class WebViewFocusManager : views::FocusChangeListener,
                            content::WebContentsObserver {
 public:
  explicit WebViewFocusManager(views::FocusManager* focus_manager,
                               content::WebContents* web_contents);

  ~WebViewFocusManager() override;

  void AdvanceFocus(bool reverse);

 private:
  // content::WebContentsObserver overrides
  void OnFocusChangedInPage(
      const content::FocusedNodeDetails& details) override;

  // views::FocusChangeListener
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  void Done();

  bool done_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
  raw_ptr<views::FocusManager> focus_manager_;
};

// Global helper to advance focus, potentially using WebViewFocusManager if
// a WebView is focused.
void AdvanceFocus(views::FocusManager* focus_manager, bool reverse);

}  // namespace ui_test_utils

#endif  // CHROME_TEST_BASE_WEB_VIEW_FOCUS_HELPER_H_
