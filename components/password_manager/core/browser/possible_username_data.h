// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_POSSIBLE_USERNAME_DATA_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_POSSIBLE_USERNAME_DATA_H_

#include <compare>
#include <optional>
#include <string>

#include "base/time/time.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
#include "components/password_manager/core/browser/votes_uploader.h"

namespace password_manager {

// Contains information to uniquely identify the field that is considered to be
// username in Username First Flow.
struct PossibleUsernameFieldIdentifier {
  friend auto operator<=>(const PossibleUsernameFieldIdentifier& lhs,
                          const PossibleUsernameFieldIdentifier& rhs) = default;
  friend bool operator==(const PossibleUsernameFieldIdentifier& lhs,
                         const PossibleUsernameFieldIdentifier& rhs) = default;

  // Id of the `PasswordManagerDriver` which corresponds to the frame of this
  // field. When paired with the `renderer_id`, this pair uniquely identifies a
  // field globally.
  int driver_id;

  // Id of the field within the frame.
  autofill::FieldRendererId renderer_id;
};

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

  // Id of the field within the frame.
  autofill::FieldRendererId renderer_id;

  std::u16string value;
  base::Time last_change;

  // Id of the `PasswordManagerDriver` which corresponds to the frame of this
  // field. When paired with the `renderer_id`, this pair uniquely identifies a
  // field globally.
  int driver_id;

  // Whether the autocomplete attribute is present and equals to
  // username.
  bool autocomplete_attribute_has_username;

  // Whether the field is likely to be an OTP field, based on its HTML
  // attributes.
  bool is_likely_otp;

  // Predictions for the form which contains a field with |renderer_id|.
  std::optional<FormPredictions> form_predictions;

  // Returns whether |possible_username| was last edited too far in the past and
  // should not be considered as a possible single username.
  bool IsStale() const;

  // Returns whether the field identified by |renderer_id| has a
  // single username prediction stored in |form_predictions|.
  bool HasSingleUsernameServerPrediction() const;

  // Returns whether the field identified by |renderer_id| has a
  // single username prediction stored in |form_predictions| that is an
  // override.
  bool HasSingleUsernameOverride() const;

  // Returns whether the field identified by |renderer_id| has
  // any server prediction stored in |form_predictions|.
  bool HasServerPrediction() const;
};

// This enum lists priorities (from highest to lowest) of the possible username
// that is outside of the password form.
enum class UsernameFoundOutsideOfFormType {
  // Text field outside of the password form that was modified by user.
  // If username outside of the form has type `kUserModifiedTextField`, it is
  // not used in the save bubble, but will be used in the voting stage.
  kUserModifiedTextField = 0,
  // Field has autocomplete="username" attribute.
  kUsernameAutocomplete = 1,
  // Field's value matches username found inside the password form that was
  // prefilled by the website.
  // It will not influence username in the save bubble, but it is critical
  // for voting stage.
  kMatchingUsername = 2,
  // Field has `SINGLE_USERNAME` server prediction.
  kSingleUsernamePrediction = 3,
  // Server override. Used even if there is a server prediction of username
  // inside the password form.
  kSingleUsernameOverride = 4,
};

// Combines information about the username field outside password form based on
// what is known about the field.
struct UsernameFoundOutsideOfForm {
  UsernameFoundOutsideOfFormType priority =
      UsernameFoundOutsideOfFormType::kUserModifiedTextField;
  PasswordFormHadMatchingUsername password_form_had_matching_username =
      PasswordFormHadMatchingUsername(false);
  PossibleUsernameData data;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_POSSIBLE_USERNAME_DATA_H_
