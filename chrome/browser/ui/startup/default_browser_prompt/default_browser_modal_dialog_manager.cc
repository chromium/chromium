// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_modal_dialog_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/webui/default_browser/default_browser_modal_dialog_delegate.h"
#include "ui/accessibility/platform/ax_platform_tree_manager_delegate.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace default_browser {

DefaultBrowserModalDialogManager::DefaultBrowserModalDialogManager(
    bool use_settings_illustration)
    : use_settings_illustration_(use_settings_illustration) {}

DefaultBrowserModalDialogManager::~DefaultBrowserModalDialogManager() = default;

void DefaultBrowserModalDialogManager::ShowForBrowser(
    BrowserWindowInterface* browser) {
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  CHECK(browser_view);

  gfx::NativeWindow parent_window = gfx::NativeWindow();
  if (views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
          browser_view->GetNativeWindow())) {
    parent_window = widget->GetNativeWindow();
  }

  std::unique_ptr<views::Widget> dialog_widget =
      ::default_browser::Show(browser->GetProfile(), parent_window,
                              use_settings_illustration_, can_pin_to_taskbar());
  dialog_widget->MakeCloseSynchronous(base::BindOnce(
      &DefaultBrowserModalDialogManager::OnDialogWidgetCloseRequested,
      base::Unretained(this), browser));
  dialog_widgets_[browser] = std::move(dialog_widget);
}

void DefaultBrowserModalDialogManager::CloseForBrowser(
    BrowserWindowInterface* browser) {
  if (auto entry = dialog_widgets_.extract(browser)) {
    // The entry mapped value is a std::unique_ptr<views::Widget>.
    if (entry.mapped()) {
      entry.mapped()->MakeCloseSynchronous(base::NullCallback());
    }
  }
}

void DefaultBrowserModalDialogManager::CloseAllPromptInstances() {
  auto widgets = std::move(dialog_widgets_);
  for (auto& [browser, widget] : widgets) {
    if (widget) {
      widget->MakeCloseSynchronous(base::NullCallback());
      widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    }
  }
}

void DefaultBrowserModalDialogManager::OnDialogWidgetCloseRequested(
    BrowserWindowInterface* browser,
    views::Widget::ClosedReason reason) {
  // Note: On Mac, the ESC dismissal is resolved as kUnspecified.
  const bool is_dismiss_action =
      reason == views::Widget::ClosedReason::kEscKeyPressed ||
      reason == views::Widget::ClosedReason::kCloseButtonClicked ||
      reason == views::Widget::ClosedReason::kCancelButtonClicked ||
      reason == views::Widget::ClosedReason::kUnspecified;
  if (is_dismiss_action) {
    HandleDismiss();
    DefaultBrowserPromptManager::GetInstance()->CloseAllPrompts(
        DefaultBrowserPromptManager::CloseReason::kDismiss);
    return;
  }

  dialog_widgets_.erase(browser);
}

default_browser::DefaultBrowserEntrypointType
DefaultBrowserModalDialogManager::GetEntrypointType() const {
  return use_settings_illustration_
             ? default_browser::DefaultBrowserEntrypointType::
                   kModalDialogWithSettingsIllustration
             : default_browser::DefaultBrowserEntrypointType::
                   kModalDialogWithoutSettingsIllustration;
}

}  // namespace default_browser
