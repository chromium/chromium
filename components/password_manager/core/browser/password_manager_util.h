// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_UTIL_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "ui/gfx/native_widget_types.h"

namespace password_manager {
class PasswordManagerDriver;
class PasswordManagerClient;
struct PasswordFormDigest;
}  // namespace password_manager

namespace autofill {
class AutofillClient;
}  // namespace autofill

class PrefService;

namespace password_manager_util {

// For credentials returned from PasswordStore::GetLogins, the enum specifies
// the type of the match for the requested page. Higher value always means
// weaker match.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.password_manager
enum class GetLoginMatchType {
  // Exact origin or Android credentials.
  kExact,
  // A web site affiliated with the requesting page.
  kAffiliated,
  // eTLD + 1 match.
  kPSL,
  // Singon realm is grouped with the requesting page URL as determined by the
  // `AffiliationService`. This relation to the requesting page is weaker than
  // `kAffiliated`.
  kGrouped,
};

// Update |credential| to reflect usage.
void UpdateMetadataForUsage(password_manager::PasswordForm* credential);

// A convenience function for testing that |client| has a non-null LogManager
// and that that LogManager returns true for IsLoggingActive. This function can
// be removed once PasswordManagerClient::GetLogManager is implemented on iOS
// and required to always return non-null.
bool IsLoggingActive(password_manager::PasswordManagerClient* client);

// True iff the manual password generation is enabled for the current site.
bool ManualPasswordGenerationEnabled(
    password_manager::PasswordManagerDriver* driver);

// Returns true iff the "Show all saved passwords" option should be shown in
// Context Menu. Also records metric, that the Context Menu will have "Show all
// saved passwords" option.
bool ShowAllSavedPasswordsContextMenuEnabled(
    password_manager::PasswordManagerDriver* driver);

// Triggers password generation flow and records the metrics. If the user should
// be asked to opt in to account storage, will trigger a reauth flow first and
// generation will only happen on success.
void UserTriggeredManualGenerationFromContextMenu(
    password_manager::PasswordManagerClient* password_manager_client,
    autofill::AutofillClient* autofill_client);

// Checks if password saving is possible at a storage level.
bool IsAbleToSavePasswords(password_manager::PasswordManagerClient* client);

// Excluding protocol from a signon_realm means to remove from the signon_realm
// what is before the web origin (with the protocol excluded as well). For
// example if the signon_realm is "https://www.google.com/", after
// excluding protocol it becomes "www.google.com/".
// This assumes that the |form|'s host is a substring of the signon_realm.
std::string_view GetSignonRealmWithProtocolExcluded(
    const password_manager::PasswordForm& form);

// For credentials returned from PasswordStore::GetLogins, specifies the type of
// the match for the requested page.
GetLoginMatchType GetMatchType(const password_manager::PasswordForm& form);

// Given all non-blocklisted |matches| returns best matches as the result of the
// function. For comparing credentials the following rule is used:
//   - non-psl match is better than psl match,
//   - most recently used match is better than other matches.
//   - In case of tie, an arbitrary credential from the tied ones is chosen for
//     best matches.
// TODO(crbug.com/343879843) FindBestMatches should be part of FormFetcherImpl
// implementation detail as it has a strong coupling to form fetcher's internal
// state.
std::vector<password_manager::PasswordForm> FindBestMatches(
    base::span<password_manager::PasswordForm> matches);

// Returns a form with the given |username_value| from |forms|, or nullptr if
// none exists. If multiple matches exist, returns the first one.
const password_manager::PasswordForm* FindFormByUsername(
    base::span<const password_manager::PasswordForm> forms,
    const std::u16string& username_value);

// If the user submits a form, they may have used existing credentials, new
// credentials, or modified existing credentials that should be updated.
// The function returns a form from |credentials| that is the best candidate to
// use for an update. Returned value is nullptr if |submitted_form| looks like a
// new credential for the site to be saved.
// |submitted_form| is the form being submitted.
// |credentials| are all the credentials relevant for the current site including
// PSL and Android matches.
// |username_updated_in_bubble| indicates whether a username was manually
// updated during the save prompt bubble.
const password_manager::PasswordForm* GetMatchForUpdating(
    const password_manager::PasswordForm& submitted_form,
    const std::vector<raw_ptr<const password_manager::PasswordForm,
                              VectorExperimental>>& credentials,
    bool username_updated_in_bubble = false);

// This method creates a blocklisted form with |digests|'s scheme, signon_realm
// and origin. This is done to avoid storing PII and to have a normalized unique
// key. Furthermore it attempts to normalize the origin by stripping path
// components. In case this fails (e.g. for non-standard origins like Android
// credentials), the original origin is kept.
password_manager::PasswordForm MakeNormalizedBlocklistedForm(
    password_manager::PasswordFormDigest digest);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
bool ShouldBiometricAuthenticationForFillingToggleBeVisible(
    const PrefService* local_state);

bool ShouldShowBiometricAuthenticationBeforeFillingPromo(
    password_manager::PasswordManagerClient* client);
#endif

// Strips any authentication data, as well as query and ref portions of URL.
GURL StripAuthAndParams(const GURL& gurl);

// Helper which checks for scheme in the |url|. If not present, adds "https://"
// by default. For ip-addresses, scheme "http://" is used.
GURL ConstructGURLWithScheme(const std::string& url);

// TODO(crbug.com/40202333): Deduplicate GetSignonRealm implementations.
// Returns the value of PasswordForm::signon_realm for an HTML form with the
// origin |url|.
std::string GetSignonRealm(const GURL& url);

#if BUILDFLAG(IS_IOS)
// Returns a boolean indicating whether the user had enabled the credential
// provider in their iOS settings at startup.
bool IsCredentialProviderEnabledOnStartup(const PrefService* local_state);

// Sets the boolean indicating whether the user had enabled the credential
// provider in their iOS settings at startup.
void SetCredentialProviderEnabledOnStartup(PrefService* local_state,
                                           bool enabled);
#endif

// Contains all special symbols considered for password-generation.
inline constexpr std::u16string_view kSpecialSymbols =
    u"!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

// Helper functions for character type classification. The built-in functions
// depend on locale, platform and other stuff. To make the output more
// predictable, the function are re-implemented here.
bool IsNumeric(char16_t c);

bool IsLetter(char16_t c);

bool IsLowercaseLetter(char16_t c);

bool IsUppercaseLetter(char16_t c);

// Checks if a supplied character |c| is a special symbol.
// Special symbols are defined by the string |kSpecialSymbols|.
bool IsSpecialSymbol(char16_t c);

// Returns true if 'type' is a username in a password-less form.
bool IsSingleUsernameType(autofill::FieldType type);

}  // namespace password_manager_util

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_UTIL_H_
