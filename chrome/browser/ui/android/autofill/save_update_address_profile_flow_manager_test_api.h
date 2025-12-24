// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_FLOW_MANAGER_TEST_API_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_FLOW_MANAGER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/android/autofill/save_update_address_profile_flow_manager.h"
#include "components/messages/android/message_enums.h"

namespace autofill {

class SaveUpdateAddressProfilePromptController;

class SaveUpdateAddressProfileFlowManagerTestApi {
 public:
  explicit SaveUpdateAddressProfileFlowManagerTestApi(
      SaveUpdateAddressProfileFlowManager& manager)
      : manager_(manager) {}

  SaveUpdateAddressProfilePromptController* GetPromptController() {
    return manager_->save_update_address_profile_prompt_controller_.get();
  }

  bool IsMessageDisplayed() const { return manager_->is_message_displayed_; }

  void OnMessagePrimaryAction() { manager_->OnMessagePrimaryAction(); }

  void OnMessageDismissed(messages::DismissReason dismiss_reason) {
    manager_->OnMessageDismissed(dismiss_reason);
  }

 private:
  raw_ref<SaveUpdateAddressProfileFlowManager> manager_;
};

inline SaveUpdateAddressProfileFlowManagerTestApi test_api(
    SaveUpdateAddressProfileFlowManager& manager) {
  return SaveUpdateAddressProfileFlowManagerTestApi(manager);
}

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_FLOW_MANAGER_TEST_API_H_
