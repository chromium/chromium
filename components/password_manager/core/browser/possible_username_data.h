// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_POSSIBLE_USERNAME_DATA_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_POSSIBLE_USERNAME_DATA_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {

// The maximum time between the user typed in a text field and subsequent
// submission of the password form, such that the typed value is considered to
// be possible to be username.
constexpr auto kMaxDelayBetweenTypingUsernameAndSubmission =
    base::TimeDelta::FromMinutes(1);

// Contains information that the user typed in a text field. It might be the
// username during username first flow.
struct PossibleUsernameData {
  PossibleUsernameData(std::string signon_realm,
                       autofill::FieldRendererId renderer_id,
                       std::u16string value,
                       base::Time last_change,
                       int driver_id);
  PossibleUsernameData(const PossibleUsernameData&);
  ~PossibleUsernameData();

  std::string signon_realm;
  autofill::FieldRendererId renderer_id;
  std::u16string value;
  base::Time last_change;

  // Id of PasswordManagerDriver which corresponds to the frame of this field.
  // Paired with the |renderer_id|, this identifies a field globally.
  int driver_id;

  // Predictions for the form which contains a field with |renderer_id|.
  absl::optional<FormPredictions> form_predictions;
};

// Checks that |possible_username| might represent an username:
// 1.|possible_username.signon_realm| == |submitted_signon_realm|
// 2.|possible_username.value| is contained in |possible_usernames| after
//   canonicalization.
// 3.|possible_username.value.last_change| was not more than
//   |kMaxDelayBetweenTypingUsernameAndSubmission| ago.
bool IsPossibleUsernameValid(
    const PossibleUsernameData& possible_username,
    const std::string& submitted_signon_realm,
    const std::vector<std::u16string>& possible_usernames);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_POSSIBLE_USERNAME_DATA_H_
