// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_FILL_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_FILL_DATA_H_

#include <map>
#include <vector>

#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

// Contains renderer ids of password related elements found by the form parser.
struct ParsingResult {
  FieldRendererId username_renderer_id;
  FieldRendererId password_renderer_id;
  FieldRendererId new_password_renderer_id;
  FieldRendererId confirm_password_renderer_id;
};

struct PasswordAndMetadata {
  PasswordAndMetadata();
  PasswordAndMetadata(const PasswordAndMetadata&);
  PasswordAndMetadata(PasswordAndMetadata&&);
  PasswordAndMetadata& operator=(const PasswordAndMetadata&);
  PasswordAndMetadata& operator=(PasswordAndMetadata&&);
  ~PasswordAndMetadata();

  std::u16string username_value;
  std::u16string password_value;
  std::string realm;
  bool uses_account_store = false;
};

// Structure used for autofilling password forms. Note that the realms in this
// struct are only set when the password's realm differs from the realm of the
// form that we are filling.
struct PasswordFormFillData {
  using LoginCollection = std::vector<PasswordAndMetadata>;

  PasswordFormFillData();
  PasswordFormFillData(const PasswordFormFillData&);
  PasswordFormFillData& operator=(const PasswordFormFillData&);
  PasswordFormFillData(PasswordFormFillData&&);
  PasswordFormFillData& operator=(PasswordFormFillData&&);
  ~PasswordFormFillData();

  // Contains the unique renderer form id.
  // If there is no form tag then |form_renderer_id|.is_null().
  // Username and Password elements renderer ids are in
  // |username_field.unique_renderer_id| and |password_field.unique_renderer_id|
  // correspondingly.
  FormRendererId form_renderer_id;

  // An URL consisting of the scheme, host, port and path; the rest is stripped.
  GURL url;

  // Identifiers of the username and password fields.
  FieldRendererId username_element_renderer_id;
  FieldRendererId password_element_renderer_id;

  // True if the server-side classification believes that the field may be
  // pre-filled with a placeholder in the value attribute.
  bool username_may_use_prefilled_placeholder = false;

  // The preferred credential. See |IsBetterMatch| for how it is selected.
  PasswordAndMetadata preferred_login;

  // A list of other matching username->PasswordAndMetadata pairs for the form.
  LoginCollection additional_logins;

  // Tells us whether we need to wait for the user to enter a valid username
  // before we autofill the password. By default, this is off unless the
  // PasswordManager determined there is an additional risk associated with this
  // form. This can happen, for example, if action URI's of the observed form
  // and our saved representation don't match up.
  bool wait_for_username = false;
};

// If |data.wait_for_username| is set, the renderer does not need to receive
// passwords, yet, and this function clears the password values from |data|.
PasswordFormFillData MaybeClearPasswordValues(const PasswordFormFillData& data);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_FILL_DATA_H__
