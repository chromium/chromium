// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_bubble_dialog_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_bubble_dialog.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "ui/views/widget/widget.h"

DefaultBrowserBubbleDialogManager::DefaultBrowserBubbleDialogManager() =
    default;

DefaultBrowserBubbleDialogManager::~DefaultBrowserBubbleDialogManager() =
    default;

void DefaultBrowserBubbleDialogManager::ShowForBrowser(
    BrowserWindowInterface* browser) {
  if (!IsBrowserValidForShowing(browser)) {
    return;
  }

  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return;
  }

  auto* anchor_view =
      browser_view->toolbar_button_provider()->GetAppMenuButton();

  dialog_widgets_[browser] = default_browser::ShowDefaultBrowserBubbleDialog(
      anchor_view, can_pin_to_taskbar(),
      base::BindOnce(&DefaultBrowserBubbleDialogManager::OnAccept,
                     base::Unretained(this)),
      base::BindOnce(&DefaultBrowserBubbleDialogManager::OnDismiss,
                     base::Unretained(this)));
}

void DefaultBrowserBubbleDialogManager::CloseForBrowser(
    BrowserWindowInterface* browser) {
  if (auto widget = dialog_widgets_.extract(browser)) {
    widget.mapped().reset();
  }
}

void DefaultBrowserBubbleDialogManager::OnAccept() {
  HandleAccept();

  DefaultBrowserPromptManager::GetInstance()->CloseAllPrompts(
      DefaultBrowserPromptManager::CloseReason::kAccept);
}

void DefaultBrowserBubbleDialogManager::OnDismiss() {
  HandleDismiss();

  DefaultBrowserPromptManager::GetInstance()->CloseAllPrompts(
      DefaultBrowserPromptManager::CloseReason::kDismiss);
}

void DefaultBrowserBubbleDialogManager::CloseAllPromptInstances() {
  dialog_widgets_.clear();
}

default_browser::DefaultBrowserEntrypointType
DefaultBrowserBubbleDialogManager::GetEntrypointType() const {
  return default_browser::DefaultBrowserEntrypointType::kBubbleDialog;
}
