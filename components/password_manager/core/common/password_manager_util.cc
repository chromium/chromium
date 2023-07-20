// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_util.h"

#include <string>

#include "base/ranges/algorithm.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/common/password_manager_constants.h"

namespace password_manager::util {

bool IsRendererRecognizedCredentialForm(const autofill::FormData& form) {
  // TODO(crbug.com/1465793): Consolidate with the parsing logic in
  // form_autofill_util.cc.
  return base::ranges::any_of(
      form.fields, [](const autofill::FormFieldData& field) {
        return field.IsPasswordInputElement() ||
               field.autocomplete_attribute.find(
                   password_manager::constants::kAutocompleteUsername) !=
                   std::string::npos ||
               field.autocomplete_attribute.find(
                   password_manager::constants::kAutocompleteWebAuthn) !=
                   std::string::npos;
      });
}

}  // namespace password_manager::util
