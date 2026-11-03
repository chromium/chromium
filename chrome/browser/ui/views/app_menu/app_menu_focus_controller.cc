// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_focus_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ui/views/app_menu/app_menu_search_bar_view.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

AppMenuFocusController::AppMenuFocusController(views::Widget* owner_widget,
                                               AppMenuSearchBarView* search_bar)
    : owner_widget_(owner_widget ? owner_widget->GetWeakPtr() : nullptr),
      search_field_tracker_(
          search_bar ? search_bar->GetViewByElementId(
                           AppMenuSearchBarView::kSearchFieldElementId)
                     : nullptr) {
  if (!owner_widget_ || !search_bar) {
    return;
  }
  views::FocusManager* const focus_manager = owner_widget_->GetFocusManager();
  if (!focus_manager || !focus_manager->GetFocusedView()) {
    return;
  }
  if (auto* const web_view =
          views::AsViewClass<views::WebView>(focus_manager->GetFocusedView())) {
    // Do not blur WebView (which would fire a DOM blur event on the web
    // page). Instead, detach the owner InputMethod's TextInputClient while
    // the menu is open so IME events do not reach the web page, and
    // restore it when the menu closes.
    if (ui::InputMethod* const input_method = owner_widget_->GetInputMethod()) {
      if (ui::TextInputClient* const client =
              input_method->GetTextInputClient()) {
        saved_text_input_client_ = client->AsWeakPtr();
        focused_web_view_tracker_.SetView(web_view);
        Observe(web_view->web_contents());
        web_contents_detached_subscription_ =
            web_view->AddWebContentsDetachedCallback(base::BindRepeating(
                &AppMenuFocusController::OnWebContentsDetached,
                base::Unretained(this)));
        input_method->SetFocusedTextInputClient(nullptr);
      }
    }
  } else {
    focus_manager->StoreFocusedView(true);
    cleared_owner_focus_ = true;
  }
}

AppMenuFocusController::~AppMenuFocusController() {
  RestoreOwnerFocus();
}

void AppMenuFocusController::RestoreOwnerFocus(bool should_restore) {
  Observe(nullptr);
  web_contents_detached_subscription_ = {};
  const base::WeakPtr<views::Widget> owner_widget = std::move(owner_widget_);
  const bool cleared_owner_focus = std::exchange(cleared_owner_focus_, false);
  const base::WeakPtr<ui::TextInputClient> saved_text_input_client =
      std::move(saved_text_input_client_);
  views::View* const focused_web_view = focused_web_view_tracker_.view();
  focused_web_view_tracker_.SetView(nullptr);
  if (!owner_widget || owner_widget->IsClosed()) {
    return;
  }

  views::FocusManager* const focus_manager = owner_widget->GetFocusManager();
  if (saved_text_input_client && should_restore && focused_web_view &&
      focus_manager && focus_manager->GetFocusedView() == focused_web_view) {
    if (ui::InputMethod* const input_method = owner_widget->GetInputMethod();
        input_method && !input_method->GetTextInputClient()) {
      input_method->SetFocusedTextInputClient(saved_text_input_client.get());
    }
  }

  if (cleared_owner_focus && should_restore && focus_manager &&
      !focus_manager->GetFocusedView()) {
    focus_manager->RestoreFocusedView();
  }
}

base::ScopedClosureRunner AppMenuFocusController::PreserveSearchBarFocus() {
  views::View* const search_field = search_field_tracker_.view();
  if (!search_field || !search_field->HasFocus()) {
    return base::ScopedClosureRunner();
  }
  return base::ScopedClosureRunner(base::BindOnce(
      [](views::ViewTracker* tracker) {
        if (views::View* const view = tracker->view()) {
          view->RequestFocus();
        }
      },
      base::Unretained(&search_field_tracker_)));
}

void AppMenuFocusController::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  InvalidateSavedTextInputClient();
}

void AppMenuFocusController::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  InvalidateSavedTextInputClient();
}

void AppMenuFocusController::WebContentsDestroyed() {
  InvalidateSavedTextInputClient();
}

void AppMenuFocusController::OnWebContentsDetached(views::WebView* web_view) {
  InvalidateSavedTextInputClient();
}

void AppMenuFocusController::InvalidateSavedTextInputClient() {
  saved_text_input_client_.reset();
  focused_web_view_tracker_.SetView(nullptr);
  web_contents_detached_subscription_ = {};
  Observe(nullptr);
}
