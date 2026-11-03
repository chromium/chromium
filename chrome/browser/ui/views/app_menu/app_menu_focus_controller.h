// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_FOCUS_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_FOCUS_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/view_tracker.h"

class AppMenuSearchBarView;

namespace ui {
class TextInputClient;
}  // namespace ui

namespace views {
class WebView;
class Widget;
}  // namespace views

// Manages focus and IME state for the owning browser widget and the menu's
// search bar while the ChroMenu is open:
// 1. Clears non-WebView focus on the owning widget (or detaches the active
//    WebView's TextInputClient without firing a DOM blur event) when the menu
//    opens, and restores it when the menu closes without executing an action.
// 2. Preserves focus on the search bar's Textfield across menu item rebuilds
//    triggered by ShowSearchResults() and RestoreMenu().
class AppMenuFocusController : public content::WebContentsObserver {
 public:
  AppMenuFocusController(views::Widget* owner_widget,
                         AppMenuSearchBarView* search_bar);
  AppMenuFocusController(const AppMenuFocusController&) = delete;
  AppMenuFocusController& operator=(const AppMenuFocusController&) = delete;
  ~AppMenuFocusController() override;

  // Restores focus or the detached `ui::TextInputClient` on `owner_widget_`
  // when `should_restore` is true. Skipped when `should_restore` is false
  // (e.g. when a deferred menu action is about to execute and transfer focus
  // to its own target).
  void RestoreOwnerFocus(bool should_restore = true);

  // Returns a runner that restores focus to the search bar's Textfield when
  // destroyed if the Textfield had focus when called.
  [[nodiscard]] base::ScopedClosureRunner PreserveSearchBarFocus();

  // content::WebContentsObserver:
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void WebContentsDestroyed() override;

 private:
  void OnWebContentsDetached(views::WebView* web_view);
  void InvalidateSavedTextInputClient();

  base::WeakPtr<views::Widget> owner_widget_;
  views::ViewTracker search_field_tracker_;
  views::ViewTracker focused_web_view_tracker_;
  base::CallbackListSubscription web_contents_detached_subscription_;
  bool cleared_owner_focus_ = false;
  base::WeakPtr<ui::TextInputClient> saved_text_input_client_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_FOCUS_CONTROLLER_H_
