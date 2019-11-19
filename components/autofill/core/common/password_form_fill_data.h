// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_FILL_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_FILL_DATA_H_

#include <map>
#include <vector>

#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"

namespace autofill {

// Contains renderer ids of password related elements found by the form parser.
struct ParsingResult {
  uint32_t username_renderer_id;
  uint32_t password_renderer_id;
  uint32_t new_password_renderer_id;
  uint32_t confirm_password_renderer_id;
};

struct PasswordAndMetadata {
  base::string16 password;
  std::string realm;
  bool uses_account_store = false;
};

// Structure used for autofilling password forms. Note that the realms in this
// struct are only set when the password's realm differs from the realm of the
// form that we are filling.
struct PasswordFormFillData {
  using LoginCollection = std::map<base::string16, PasswordAndMetadata>;

  PasswordFormFillData();

  // Create a FillData structure in preparation for autofilling a form, from
  // basic_data identifying which form to fill, and a collection of matching
  // stored logins to use as username/password values. |preferred_match| should
  // equal (address) one of matches. |wait_for_username| is true if we should
  // not autofill anything until the user typed in a valid username and blurred
  // the field. If |enable_possible_usernames| is true, we will populate
  // possible_usernames.
  PasswordFormFillData(const PasswordForm& form_on_page,
                       const std::vector<const PasswordForm*>& matches,
                       const PasswordForm& preferred_match,
                       bool wait_for_username);

  PasswordFormFillData(const PasswordFormFillData&);
  PasswordFormFillData& operator=(const PasswordFormFillData&);
  PasswordFormFillData(PasswordFormFillData&&);
  PasswordFormFillData& operator=(PasswordFormFillData&&);
  ~PasswordFormFillData();

  // If |has_renderer_ids| == true then |form_renderer_id| contains the unique
  // renderer form id. No special values for |has_renderer_ids| == false case
  // was introduced because the absent of ids is just temprorary situation while
  // the old form parsing still exists.
  // If there is no form tag then |form_renderer_id| ==
  // FormData::kNotSetFormRendererId.
  // Username and Password elements renderer ids are in
  // |username_field.unique_renderer_id| and |password_field.unique_renderer_id|
  // correspondingly.
  uint32_t form_renderer_id = FormData::kNotSetFormRendererId;

  // The name of the form.
  base::string16 name;

  // An origin URL consists of the scheme, host, port and path; the rest is
  // stripped.
  GURL origin;

  // The action target of the form; like |origin| URL consists of the scheme,
  // host, port and path; the rest is stripped.
  GURL action;

  // Username and password input fields in the form.
  FormFieldData username_field;
  FormFieldData password_field;

  // True if the server-side classification believes that the field may be
  // pre-filled with a placeholder in the value attribute.
  bool username_may_use_prefilled_placeholder = false;

  // The signon realm of the preferred user/pass pair.
  std::string preferred_realm;

  // True iff the password originated from the account store rather than the
  // local password store.
  bool uses_account_store = false;

  // A list of other matching username->PasswordAndMetadata pairs for the form.
  LoginCollection additional_logins;

  // Tells us whether we need to wait for the user to enter a valid username
  // before we autofill the password. By default, this is off unless the
  // PasswordManager determined there is an additional risk associated with this
  // form. This can happen, for example, if action URI's of the observed form
  // and our saved representation don't match up.
  bool wait_for_username = false;

  // True if renderer ids for form, username and password fields are present.
  // TODO(https://crbug.com/831123): Remove this field when old parsing is
  // removed and filling by renderer ids is by default.
  bool has_renderer_ids = false;
};

// If |data.wait_for_username| is set, the renderer does not need to receive
// passwords, yet, and this function clears the password values from |data|.
PasswordFormFillData MaybeClearPasswordValues(const PasswordFormFillData& data);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_FILL_DATA_H__
