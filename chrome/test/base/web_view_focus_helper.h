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

// Listens to UI and DOM element focus changes across multiple WebContents and
// a FocusManager.
class FocusChangeObserver : public views::FocusChangeListener {
 public:
  explicit FocusChangeObserver(views::FocusManager* focus_manager);
  FocusChangeObserver(views::FocusManager* focus_manager,
                      const std::vector<content::WebContents*>& web_contents);
  ~FocusChangeObserver() override;

  // Waits until focus changes. Returns true if focus changed, false if it timed
  // out.
  bool WaitForFocusChange(base::TimeDelta timeout = base::TimeDelta::Max());

  // views::FocusChangeListener:
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

 private:
  class WebContentsFocusObserver : public content::WebContentsObserver {
   public:
    WebContentsFocusObserver(FocusChangeObserver* owner,
                             content::WebContents* web_contents);
    ~WebContentsFocusObserver() override;

    // content::WebContentsObserver:
    void OnFocusChangedInPage(
        const content::FocusedNodeDetails& details) override;

   private:
    raw_ptr<FocusChangeObserver> owner_;
  };

  void OnFocusChanged();

  base::ScopedObservation<views::FocusManager, views::FocusChangeListener>
      focus_manager_observation_{this};
  std::vector<std::unique_ptr<WebContentsFocusObserver>>
      web_contents_observers_;
  bool changed_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
};

// A helper to manage focus advancement (Tab/Shift-Tab) when WebViews are
// involved. WebViews often need special handling for focus traversal.
class WebViewFocusManager {
 public:
  WebViewFocusManager(views::FocusManager* focus_manager,
                      content::WebContents* web_contents);
  ~WebViewFocusManager();

  void AdvanceFocus(bool reverse);

 private:
  raw_ptr<views::FocusManager> focus_manager_;
  raw_ptr<content::WebContents> web_contents_;
};

// Global helper to advance focus, potentially using WebViewFocusManager if
// a WebView is focused.
void AdvanceFocus(views::FocusManager* focus_manager, bool reverse);

}  // namespace ui_test_utils

#endif  // CHROME_TEST_BASE_WEB_VIEW_FOCUS_HELPER_H_
