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

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/taskbar_manager.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#endif

namespace {

bool IsBrowserValidForShowingDialog(BrowserWindowInterface* browser) {
  return browser->GetType() == BrowserWindowInterface::TYPE_NORMAL &&
         !browser->GetProfile()->IsIncognitoProfile() &&
         !browser->GetProfile()->IsGuestSession();
}

}  // namespace

DefaultBrowserBubbleDialogManager::DefaultBrowserBubbleDialogManager() =
    default;

DefaultBrowserBubbleDialogManager::~DefaultBrowserBubbleDialogManager() =
    default;

void DefaultBrowserBubbleDialogManager::Show(
    std::unique_ptr<default_browser::DefaultBrowserController> controller,
    bool can_pin_to_taskbar) {
  CloseAll();
  can_pin_to_taskbar_ = can_pin_to_taskbar;

  default_browser_controller_ = std::move(controller);
  default_browser_controller_->OnShown();

  auto* global_browser_collection = GlobalBrowserCollection::GetInstance();
  global_browser_collection->ForEach([this](BrowserWindowInterface* bwi) {
    this->OnBrowserCreated(bwi);
    return true;
  });

  browser_collection_observation_.Observe(global_browser_collection);
}

void DefaultBrowserBubbleDialogManager::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  if (!IsBrowserValidForShowingDialog(browser)) {
    return;
  }

  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return;
  }

  auto* anchor_view =
      browser_view->toolbar_button_provider()->GetAppMenuButton();

  dialog_widgets_[browser] = default_browser::ShowDefaultBrowserBubbleDialog(
      anchor_view, can_pin_to_taskbar_,
      base::BindOnce(&DefaultBrowserBubbleDialogManager::OnAccept,
                     base::Unretained(this)),
      base::BindOnce(&DefaultBrowserBubbleDialogManager::OnDismiss,
                     base::Unretained(this)));
}

void DefaultBrowserBubbleDialogManager::OnBrowserClosed(
    BrowserWindowInterface* browser) {
  if (auto widget = dialog_widgets_.extract(browser)) {
    widget.mapped().reset();
  }
}

void DefaultBrowserBubbleDialogManager::OnAccept() {
  if (can_pin_to_taskbar_) {
#if BUILDFLAG(IS_WIN)
    // Attempt the pin to taskbar in parallel with bringing up the Windows
    // settings UI. Serializing the operations is an option, but since the user
    // might not complete the first operation, serializing would probably make
    // the second operation less likely to happen.
    browser_util::PinAppToTaskbar(
        ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall()),
        browser_util::PinAppToTaskbarChannel::kDefaultBrowserInfoBar,
        base::DoNothing());
#else
    NOTREACHED();
#endif  // BUILDFLAG(IS_WIN)
  }
  default_browser_controller_->OnAccepted(
      base::DoNothingWithBoundArgs(std::move(default_browser_controller_)));

  DefaultBrowserPromptManager::GetInstance()->CloseAllPrompts(
      DefaultBrowserPromptManager::CloseReason::kAccept);
}

void DefaultBrowserBubbleDialogManager::OnDismiss() {
  default_browser_controller_->OnDismissed();
  default_browser_controller_.reset();

  DefaultBrowserPromptManager::GetInstance()->CloseAllPrompts(
      DefaultBrowserPromptManager::CloseReason::kDismiss);
}

void DefaultBrowserBubbleDialogManager::CloseAll() {
  can_pin_to_taskbar_ = false;
  dialog_widgets_.clear();
  browser_collection_observation_.Reset();
}

default_browser::DefaultBrowserEntrypointType
DefaultBrowserBubbleDialogManager::GetEntrypointType() const {
  return default_browser::DefaultBrowserEntrypointType::kBubbleDialog;
}
