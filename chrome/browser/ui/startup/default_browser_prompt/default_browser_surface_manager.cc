// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_surface_manager.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/default_browser_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/taskbar_manager.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#endif

DefaultBrowserSurfaceManager::DefaultBrowserSurfaceManager() = default;

DefaultBrowserSurfaceManager::~DefaultBrowserSurfaceManager() {
  if (controller_) {
    controller_->OnIgnored();
  }
}

void DefaultBrowserSurfaceManager::Show(bool can_pin_to_taskbar) {
  CloseAll();
  can_pin_to_taskbar_ = can_pin_to_taskbar;

  controller_ = default_browser::DefaultBrowserManager::CreateControllerFor(
      GetEntrypointType());
  CHECK(controller_);
  controller_->OnShown();

  auto* global_browser_collection = GlobalBrowserCollection::GetInstance();
  global_browser_collection->ForEach([this](BrowserWindowInterface* bwi) {
    this->OnBrowserCreated(bwi);
    return true;
  });

  browser_collection_observation_.Observe(global_browser_collection);
}

void DefaultBrowserSurfaceManager::CloseAll() {
  can_pin_to_taskbar_ = false;
  browser_collection_observation_.Reset();
  CloseAllPromptInstances();
}

bool DefaultBrowserSurfaceManager::IsBrowserValidForShowing(
    BrowserWindowInterface* browser) {
  return browser->GetType() == BrowserWindowInterface::TYPE_NORMAL &&
         !browser->GetProfile()->IsIncognitoProfile() &&
         !browser->GetProfile()->IsGuestSession();
}

void DefaultBrowserSurfaceManager::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  if (!IsBrowserValidForShowing(browser)) {
    return;
  }

  ShowForBrowser(browser);
}

void DefaultBrowserSurfaceManager::OnBrowserClosed(
    BrowserWindowInterface* browser) {
  if (!IsBrowserValidForShowing(browser)) {
    return;
  }

  CloseForBrowser(browser);
}

void DefaultBrowserSurfaceManager::HandleAccept() { 
  if (!controller_) {
    return;
  }

  if (can_pin_to_taskbar()) {
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

  controller_->OnAccepted(base::DoNothingWithBoundArgs(std::move(controller_)));
}

void DefaultBrowserSurfaceManager::HandleDismiss() {
  if (!controller_) {
    return;
  }

  controller_->OnDismissed();
  controller_.reset();
}

void DefaultBrowserSurfaceManager::HandleIgnore() {
  if (!controller_) {
    return;
  }

  controller_->OnIgnored();
  controller_.reset();
}
