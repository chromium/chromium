// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_POSSIBLE_USERNAME_DATA_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_POSSIBLE_USERNAME_DATA_H_

#include <string>

#include "base/time/time.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {

// The maximum time between the user typed in a text field and subsequent
// submission of the password form, such that the typed value is considered to
// be a possible username.
constexpr auto kPossibleUsernameExpirationTimeout = base::Minutes(1);

// Contains information that the user typed in a text field. It might be the
// username during username first flow.
struct PossibleUsernameData {
  PossibleUsernameData(std::string signon_realm,
                       autofill::FieldRendererId renderer_id,
                       const std::u16string& value,
                       base::Time last_change,
                       int driver_id,
                       bool autocomplete_attribute_has_username,
                       bool is_likely_otp);
  PossibleUsernameData(const PossibleUsernameData&);
  ~PossibleUsernameData();

  std::string signon_realm;
  autofill::FieldRendererId renderer_id;

  std::u16string value;
  base::Time last_change;

  // Id of PasswordManagerDriver which corresponds to the frame of this field.
  // Paired with the |renderer_id|, this identifies a field globally.
  int driver_id;

  // Whether the autocomplete attribute is present and equals to
  // username.
  bool autocomplete_attribute_has_username;

  // Whether the field is likely to be an OTP field, based on its HTML
  // attributes.
  bool is_likely_otp;

  // Predictions for the form which contains a field with |renderer_id|.
  absl::optional<FormPredictions> form_predictions;

  // Returns whether |possible_username| was last edited too far in the past and
  // should not be considered as a possible single username.
  bool IsStale() const;

  // Returns whether the field identified by |renderer_id| has a
  // single username prediction stored in |form_predictions|.
  bool HasSingleUsernameServerPrediction() const;

  // Returns whether the field identified by |renderer_id| has
  // any server prediction stored in |form_predictions|.
  bool HasServerPrediction() const;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_POSSIBLE_USERNAME_DATA_H_
