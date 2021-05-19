// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUTTON_CONTROLLER_H_

#include "chrome/browser/send_tab_to_self/receiving_ui_handler.h"

class Profile;

namespace send_tab_to_self {

class SendTabToSelfToolbarButtonControllerDelegate;

// Controller for the SendTabToSelfToolbarButtonView that decides when to show
// or hide the icon from the toolbar.
// Owned by send_tab_to_self::ReceivingUiHandlerRegistry.
class SendTabToSelfToolbarButtonController
    : public send_tab_to_self::ReceivingUiHandler {
 public:
  explicit SendTabToSelfToolbarButtonController(Profile* profile);
  SendTabToSelfToolbarButtonController(
      const SendTabToSelfToolbarButtonController&) = delete;
  SendTabToSelfToolbarButtonController& operator=(
      const SendTabToSelfToolbarButtonController&) = delete;
  ~SendTabToSelfToolbarButtonController() override;

  // ReceivingUiHandler implementation.
  void DisplayNewEntries(
      const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
          new_entries) override;
  void DismissEntries(const std::vector<std::string>& guids) override;

  void ShowToolbarButton();

  void SetDelegate(SendTabToSelfToolbarButtonControllerDelegate* delegate);

  Profile* profile() const { return profile_; }

 private:
  // Tracks the current display state of the toolbar button delegate.
  enum class DisplayState {
    kShown,
    kHidden,
  };

  void UpdateToolbarButtonState();

  Profile* profile_;

  SendTabToSelfToolbarButtonControllerDelegate* delegate_;

  // The delegate starts hidden and isn't shown until a STTS
  // notification is received.
  DisplayState delegate_display_state_ = DisplayState::kHidden;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUTTON_CONTROLLER_H_
