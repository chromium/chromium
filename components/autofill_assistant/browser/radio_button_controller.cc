// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/radio_button_controller.h"

namespace autofill_assistant {

RadioButtonController::RadioButtonController(UserModel* user_model)
    : user_model_(user_model) {}
RadioButtonController::~RadioButtonController() = default;

base::WeakPtr<RadioButtonController> RadioButtonController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void RadioButtonController::AddRadioButtonToGroup(
    const std::string& radio_group,
    const std::string& model_identifier) {
  radio_groups_[radio_group].insert(model_identifier);
}

void RadioButtonController::RemoveRadioButtonFromGroup(
    const std::string& radio_group,
    const std::string& model_identifier) {
  auto it = radio_groups_.find(radio_group);
  if (it != radio_groups_.end()) {
    it->second.erase(model_identifier);
  }
}

bool RadioButtonController::UpdateRadioButtonGroup(
    const std::string& radio_group,
    const std::string& selected_model_identifier) {
  auto radio_group_it = radio_groups_.find(radio_group);
  if (radio_group_it == radio_groups_.end()) {
    return false;
  }

  if (radio_group_it->second.find(selected_model_identifier) ==
      radio_group_it->second.end()) {
    return false;
  }

  for (const auto& model_identifier : radio_group_it->second) {
    user_model_->SetValue(
        model_identifier,
        SimpleValue(model_identifier == selected_model_identifier,
                    /* is_client_side_only = */ false));
  }
  return true;
}

}  // namespace autofill_assistant
