// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"

#include <utility>

#include "base/callback_list.h"

OmniboxPopupStateManager::OmniboxPopupStateManager() = default;

OmniboxPopupStateManager::~OmniboxPopupStateManager() = default;

void OmniboxPopupStateManager::SetPopupState(OmniboxPopupState new_state) {
  if (popup_state_ == new_state) {
    return;
  }

  OmniboxPopupState old_state = popup_state_;
  popup_state_ = new_state;

  // Notify all subscribers of the state change.
  popup_state_changed_callbacks_.Notify(old_state, new_state);
}

base::CallbackListSubscription
OmniboxPopupStateManager::AddPopupStateChangedCallback(
    PopupStateChangedCallback callback) {
  return popup_state_changed_callbacks_.Add(std::move(callback));
}
