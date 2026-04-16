// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utilities related to password manager's UI.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_

#include <string>
#include <utility>

#include "build/branding_buildflags.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "url/origin.h"

namespace autofill {
class FormData;
}

namespace password_manager {

class PasswordFormManagerForUI;
struct PasswordForm;
struct CredentialUIEntry;

// For Web credentials the returned origin is suitable for security display and
// is stripped off common prefixes like "m.", "mobile." or "www.".
//
//  For Android credentials the returned origin is set to the Play Store name
//  if available, otherwise it is the reversed package name (e.g.
//  com.example.android gets transformed to android.example.com).
std::string GetShownOrigin(const CredentialUIEntry& credential);
// Returns URL the full origin of the |credential|. For Android credential the
// link pints to affiliated website or to the Play Store if missing.
GURL GetShownUrl(const CredentialUIEntry& credential);

// Returns a string suitable for security display to the user (just like
// |FormatUrlForSecurityDisplay| with OMIT_HTTP_AND_HTTPS) based on origin of
// |password_form|) and without prefixes "m.", "mobile." or "www.".
std::string GetShownOrigin(const url::Origin& origin);

// Updates the |form_manager| pending credentials with |username| and
// |password|.
void UpdatePasswordFormUsernameAndPassword(
    const std::u16string& username,
    const std::u16string& password,
    PasswordFormManagerForUI* form_manager);

// Returns all the usernames for credentials saved for `signon_realm`. If
// `is_using_account_store` is true, this method will only consider
// credentials saved in the account store. Otherwise it will only consider
// credentials saved in the profile store.
std::vector<std::u16string> GetUsernamesForRealm(
    const std::vector<password_manager::CredentialUIEntry>& credentials,
    const std::string& signon_realm,
    bool is_using_account_store);

// Returns the resource identifier for the label describing the platform
// authenticator, e.g. "Use TouchID".
int GetPlatformAuthenticatorLabel();

// Returns the username or a label appropriate for display if it is empty.
std::u16string ToUsernameString(const std::u16string& username);
std::u16string ToUsernameString(const std::string& username);

// Describes various criteria (e.g. there are empty fields in the form) that
// affect whether a form is ready for submission. Don't change IDs as they are
// used for metrics.
// TODO(crbug.com/40209736): Basically, the browser needs just a boolean: submit
// or not. Once related projects (crbug.com/1393043, crbug.com/1319364) are
// done or archived, this enum can be removed.
enum class SubmissionReadinessState {
  // No information received. Supposed to be unused on Android.
  kNoInformation = 0,
  // Error occurred while assessing submission readiness. Ideally, Chrome
  // should not report such votes. Otherwise, |CalculateSubmissionReadiness|
  // should be corrected.
  kError = 1,

  // Various blockers of forms submission.
  // There is only a sole password field.
  // TODO(crbug.com/40223173): For now this entry doesn't trigger submission,
  // but ideally Touch-To-Fill should be able to log a user in with just one
  // tap, i.e. TTF should submit both single username and single password
  // forms.
  kNoUsernameField = 2,
  // There are fields between username and password fields.
  kFieldBetweenUsernameAndPassword = 3,
  // There is a field right after the password field by focus traversal.
  kFieldAfterPasswordField = 4,
  // There are other empty fields. If the |kFieldBetweenUsernameAndPassword| or
  // |kFieldAfterPasswordField| criteria are matched, they should be reported,
  // not this one.
  kEmptyFields = 5,
  // No empty fields and there are more than two visible fields.
  kMoreThanTwoFields = 6,

  // The most conservative criterion for submission.
  // There are only two visible fields: username and password.
  kTwoFields = 7,

  // There is only a sole username field.
  // TODO(crbug.com/40223173): For now this entry doesn't trigger submission,
  // but ideally Touch-To-Fill should be able to log a user in with just one
  // tap, i.e. TTF should submit both single username and single password
  // forms.
  kNoPasswordField = 8,

  // A child frame which is likely to be CAPTCHA was detected within the
  // password form. Do not trigger submission in this case.
  kLikelyHasCaptcha = 9,

  kMaxValue = kLikelyHasCaptcha,
};

// Infers whether a form should be submitted based on the feature's state and
// the form's structure (submission_readiness).
bool CalculateTriggerSubmission(SubmissionReadinessState submission_readiness);

// Returns a prediction whether the form will be ready for submission after
// filling.
SubmissionReadinessState CalculateSubmissionReadiness(
    const autofill::FormData& form_data,
    const autofill::FieldGlobalId& username_field_id,
    const autofill::FieldGlobalId& password_field_id);

// Returns whether to use Google Chrome branded strings.
constexpr bool UsesPasswordManagerGoogleBranding() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return false;
#endif
}

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_
