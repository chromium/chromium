// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_STATE_MANAGER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_STATE_MANAGER_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"

// Represents the state of omnibox popup currently visible.
enum class OmniboxPopupState {
  kNone,     // No popup is visible
  kClassic,  // Classic popup is visible
  kAim       // AI Mode popup is visible
};

// Manages the visibility state of omnibox popups and notifies subscribers.
// This provides a single source of truth for popup state that is accessible
// from both the model layer and the view layer.
class OmniboxPopupStateManager {
 public:
  // Callback signature for popup state changes (old_state, new_state).
  using PopupStateChangedCallback =
      base::RepeatingCallback<void(OmniboxPopupState, OmniboxPopupState)>;

  OmniboxPopupStateManager();
  OmniboxPopupStateManager(const OmniboxPopupStateManager&) = delete;
  OmniboxPopupStateManager& operator=(const OmniboxPopupStateManager&) = delete;
  ~OmniboxPopupStateManager();

  // Returns the current popup state.
  OmniboxPopupState popup_state() const { return popup_state_; }

  // Sets the current popup state. Will notify subscribers if the state changes.
  void SetPopupState(OmniboxPopupState new_state);

  // Adds a callback that will be called when the popup state changes.
  // The callback will be unregistered when the subscription is destroyed.
  [[nodiscard]] base::CallbackListSubscription AddPopupStateChangedCallback(
      PopupStateChangedCallback callback);

 private:
  OmniboxPopupState popup_state_ = OmniboxPopupState::kNone;
  base::RepeatingCallbackList<void(OmniboxPopupState, OmniboxPopupState)>
      popup_state_changed_callbacks_;
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_STATE_MANAGER_H_
