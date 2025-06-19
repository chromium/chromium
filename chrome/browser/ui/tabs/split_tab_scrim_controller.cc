// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_tab_scrim_controller.h"

#include <memory>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/split_tab_scrim_delegate.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "components/tabs/public/tab_interface.h"

namespace split_tabs {
SplitTabScrimController::SplitTabScrimController(
    std::unique_ptr<SplitTabScrimDelegate> split_tab_scrim_delegate,
    BrowserWindowInterface* browser_window_interface)
    : split_tab_scrim_delegate_(std::move(split_tab_scrim_delegate)),
      browser_window_interface_(browser_window_interface) {
  active_tab_change_subscription_ =
      browser_window_interface_->RegisterActiveTabDidChange(base::BindRepeating(
          &SplitTabScrimController::OnActiveTabChange, base::Unretained(this)));
}

SplitTabScrimController::~SplitTabScrimController() = default;

void SplitTabScrimController::OnOmniboxFocusChanged(
    OmniboxFocusState state,
    OmniboxFocusChangeReason reason) {
  UpdateScrimVisibility();
}

void SplitTabScrimController::OnActiveTabChange(
    BrowserWindowInterface* browser_window_interface) {
  omnibox_tab_helper_observation_.Reset();
  tabs::TabInterface* const active_tab =
      browser_window_interface->GetActiveTabInterface();
  if (active_tab) {
    tab_will_detach_subscription_ =
        active_tab->RegisterWillDetach(base::BindRepeating(
            &SplitTabScrimController::OnTabWillDetach, base::Unretained(this)));
    OmniboxTabHelper* const tab_helper =
        OmniboxTabHelper::FromWebContents(active_tab->GetContents());
    CHECK(tab_helper);
    omnibox_tab_helper_observation_.Observe(tab_helper);
  }
  // Need to update the scrim visibility because the omnibox focus state
  // event might have already been triggered before the active tab change.
  UpdateScrimVisibility();
}

void SplitTabScrimController::OnTabWillDetach(
    tabs::TabInterface* tab_interface,
    tabs::TabInterface::DetachReason reason) {
  // Reset the omnibox tab helper observation to ensure that it doesn't live
  // longer than the web contents it is observing.
  omnibox_tab_helper_observation_.Reset();
  tab_will_detach_subscription_ = base::CallbackListSubscription();
}

void SplitTabScrimController::UpdateScrimVisibility() {
  tabs::TabInterface* const active_tab =
      browser_window_interface_->GetActiveTabInterface();
  if (active_tab &&
      (OmniboxTabHelper::FromWebContents(active_tab->GetContents())
           ->focus_state() != OmniboxFocusState::OMNIBOX_FOCUS_NONE)) {
    split_tab_scrim_delegate_->ShowScrim();
  } else {
    split_tab_scrim_delegate_->HideScrim();
  }
}
}  // namespace split_tabs
