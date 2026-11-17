// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_modal_dialog_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/webui/default_browser/default_browser_modal_dialog_delegate.h"
#include "ui/accessibility/platform/ax_platform_tree_manager_delegate.h"
#include "ui/base/base_window.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace default_browser {

DefaultBrowserModalDialogManager::DefaultBrowserModalDialogManager(
    bool use_settings_illustration)
    : use_settings_illustration_(use_settings_illustration) {}

DefaultBrowserModalDialogManager::~DefaultBrowserModalDialogManager() = default;

void DefaultBrowserModalDialogManager::ShowForBrowser(
    BrowserWindowInterface* browser) {
  CHECK(browser);
  ui::BaseWindow* window = browser->GetWindow();
  CHECK(window);

  gfx::NativeWindow parent_window = gfx::NativeWindow();
  bool is_parent_active = false;
  if (views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
          window->GetNativeWindow())) {
    parent_window = widget->GetNativeWindow();
    is_parent_active = widget->IsActive();
  }

  std::unique_ptr<views::Widget> dialog_widget = ::default_browser::Show(
      browser->GetProfile(), parent_window, use_settings_illustration_,
      can_pin_to_taskbar(), is_parent_active);
  dialog_widget->MakeCloseSynchronous(base::BindOnce(
      &DefaultBrowserModalDialogManager::OnDialogWidgetCloseRequested,
      weak_ptr_factory_.GetWeakPtr(), browser));
  dialog_widgets_[browser] = std::move(dialog_widget);
}

void DefaultBrowserModalDialogManager::CloseForBrowser(
    BrowserWindowInterface* browser) {
  dialog_widgets_.erase(browser);
}

void DefaultBrowserModalDialogManager::CloseAllPromptInstances() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  dialog_widgets_.clear();
}

void DefaultBrowserModalDialogManager::RemoveWidget(
    BrowserWindowInterface* browser) {
  dialog_widgets_.erase(browser);
}

default_browser::DefaultBrowserEntrypointType
DefaultBrowserModalDialogManager::GetEntrypointType() const {
  if (IsDefaultBrowserModalSticky()) {
    return use_settings_illustration_
               ? default_browser::DefaultBrowserEntrypointType::
                     kStickyModalDialogWithSettingsIllustration
               : default_browser::DefaultBrowserEntrypointType::
                     kStickyModalDialogWithoutSettingsIllustration;
  }
  return use_settings_illustration_
             ? default_browser::DefaultBrowserEntrypointType::
                   kModalDialogWithSettingsIllustration
             : default_browser::DefaultBrowserEntrypointType::
                   kModalDialogWithoutSettingsIllustration;
}

}  // namespace default_browser
