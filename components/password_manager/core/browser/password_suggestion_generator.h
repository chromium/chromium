// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SUGGESTION_GENERATOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SUGGESTION_GENERATOR_H_

#include <vector>

#include "base/containers/span.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

namespace password_manager {

class PasswordManagerClient;
class PasswordManagerDriver;

using OffersGeneration = base::StrongAlias<class OffersGenerationTag, bool>;
using ShowAllPasswords = base::StrongAlias<class ShowAllPasswordsTag, bool>;
using ShowPasswordSuggestions =
    base::StrongAlias<class ShowPasswordSuggestionsTag, bool>;
using ShowWebAuthnCredentials =
    base::StrongAlias<class ShowWebAuthnCredentialsTag, bool>;
using IsTriggeredOnPasswordForm =
    base::StrongAlias<class IsTriggeredOnPasswordFormTag, bool>;
using IsCrossDomain = base::StrongAlias<class IsCrossDomainTag, bool>;

// Helper class to generate password suggestions. Calls to the generation do not
// modify the state of this class.
class PasswordSuggestionGenerator {
 public:
  PasswordSuggestionGenerator(PasswordManagerDriver* password_manager_driver,
                              PasswordManagerClient* password_client);

  // Generates password form suggestions. If `fill_data` is empty, no
  // credential suggestions will be generated. `page_favicon` represents the
  // favicon for the credentials offered on the current page. `username_filter`
  // specifies the value typed into the username form field. This value is empty
  // if the suggestions are triggered on a password field. `offers_generation`
  // specifies whether password generation suggestion should be added.
  // `show_password_suggestions` specifies whether suggestions should be
  // specified from the `fill_data`. `show_webauthn_credentials` specifies
  // whether web auth credential suggestion should be added.
  std::vector<autofill::Suggestion> GetSuggestionsForDomain(
      base::optional_ref<const autofill::PasswordFormFillData> fill_data,
      const gfx::Image& page_favicon,
      const std::u16string& username_filter,
      OffersGeneration offers_generation,
      ShowPasswordSuggestions show_password_suggestions,
      ShowWebAuthnCredentials show_webauthn_credentials) const;

  // Generates suggestions shown when user triggers password Autofill from the
  // Chrome context menu. Every suggestion will have several sub suggestions to
  // fill username, password and open credential details dialog. If both
  // `suggested_credentials` and `all_credentials` are non-emtpy, the list of
  // suggestions will have the following structure:
  //
  // Suggested
  //   Suggested password 1
  //   ...
  // All passwords
  //   Password 1
  //   ...
  // ----- separator ------
  // Manage passwords...
  //
  // If either `suggested_credentials` or `all_credentials` is empty, the
  // section titles won't be generated. If both `suggested_credentials` and
  // `all_credentials` are empty, no suggestions will be generated.
  // `suggested_credentials` contains the following types of passwords: exact,
  // strongly affiliated, PSL and weakly affiliated matches. `all_credentials`
  // contains all user passwords as displayed in settings.
  std::vector<autofill::Suggestion> GetManualFallbackSuggestions(
      base::span<const PasswordForm> suggested_credentials,
      base::span<const CredentialUIEntry> all_credentials,
      IsTriggeredOnPasswordForm on_password_form) const;

 private:
  const raw_ptr<PasswordManagerDriver> password_manager_driver_;
  const raw_ptr<PasswordManagerClient> password_client_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SUGGESTION_GENERATOR_H_
