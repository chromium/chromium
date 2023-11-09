// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_util.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

using autofill::password_generation::PasswordGenerationType;
using password_manager::PasswordForm;

namespace password_manager_util {
namespace {

std::tuple<int, base::Time, int> GetPriorityProperties(
    const PasswordForm* form) {
  return std::make_tuple(-static_cast<int>(GetMatchType(*form)),
                         form->date_last_used,
                         static_cast<int>(form->in_store));
}

// Consider the following properties:
// 1. Match strength for the original form (Exact > Affiliations > PSL).
// 2. Last time used. Most recent is better.
// 3. Account vs. profile store. Account is better.
bool IsBetterMatch(const PasswordForm* lhs, const PasswordForm* rhs) {
  return GetPriorityProperties(lhs) > GetPriorityProperties(rhs);
}

}  // namespace

// Update |credential| to reflect usage.
void UpdateMetadataForUsage(PasswordForm* credential) {
  if (credential->scheme == PasswordForm::Scheme::kHtml) {
    ++credential->times_used_in_html_form;
  }

  // Remove alternate usernames. At this point we assume that we have found
  // the right username.
  credential->all_alternative_usernames.clear();
}

bool IsLoggingActive(password_manager::PasswordManagerClient* client) {
  autofill::LogManager* log_manager = client->GetLogManager();
  return log_manager && log_manager->IsLoggingActive();
}

bool ManualPasswordGenerationEnabled(
    password_manager::PasswordManagerDriver* driver) {
  password_manager::PasswordGenerationFrameHelper* password_generation_manager =
      driver ? driver->GetPasswordGenerationHelper() : nullptr;
  if (!password_generation_manager ||
      !password_generation_manager->IsGenerationEnabled(false /*logging*/)) {
    return false;
  }

  LogPasswordGenerationEvent(
      autofill::password_generation::PASSWORD_GENERATION_CONTEXT_MENU_SHOWN);
  return true;
}

bool ShowAllSavedPasswordsContextMenuEnabled(
    password_manager::PasswordManagerDriver* driver) {
  password_manager::PasswordManagerInterface* password_manager =
      driver ? driver->GetPasswordManager() : nullptr;
  if (!password_manager)
    return false;

  password_manager::PasswordManagerClient* client =
      password_manager->GetClient();
  if (!client || !client->IsFillingEnabled(driver->GetLastCommittedURL())) {
    return false;
  }

  return true;
}

void UserTriggeredManualGenerationFromContextMenu(
    password_manager::PasswordManagerClient* password_manager_client,
    autofill::AutofillClient* autofill_client) {
  if (autofill_client) {
    autofill_client->HideAutofillPopup(
        autofill::PopupHidingReason::kOverlappingWithPasswordGenerationPopup);
  }
  if (!password_manager_client->GetPasswordFeatureManager()
           ->ShouldShowAccountStorageOptIn()) {
    password_manager_client->GeneratePassword(PasswordGenerationType::kManual);
    LogPasswordGenerationEvent(autofill::password_generation::
                                   PASSWORD_GENERATION_CONTEXT_MENU_PRESSED);
    return;
  }
  // The client ensures the callback won't be run if it is destroyed, so
  // base::Unretained is safe.
  password_manager_client->TriggerReauthForPrimaryAccount(
      signin_metrics::ReauthAccessPoint::kGeneratePasswordContextMenu,
      base::BindOnce(
          [](password_manager::PasswordManagerClient* client,
             password_manager::PasswordManagerClient::ReauthSucceeded
                 succeeded) {
            if (succeeded) {
              client->GeneratePassword(PasswordGenerationType::kManual);
              LogPasswordGenerationEvent(
                  autofill::password_generation::
                      PASSWORD_GENERATION_CONTEXT_MENU_PRESSED);
            }
          },
          base::Unretained(password_manager_client)));
}

base::StringPiece GetSignonRealmWithProtocolExcluded(const PasswordForm& form) {
  base::StringPiece signon_realm = form.signon_realm;

  // Find the web origin (with protocol excluded) in the signon_realm.
  const size_t after_protocol = signon_realm.find(form.url.host_piece());

  // Keep the string starting with position |after_protocol|.
  return signon_realm.substr(std::min(after_protocol, signon_realm.size()));
}

GetLoginMatchType GetMatchType(const password_manager::PasswordForm& form) {
  CHECK(form.match_type.has_value());
  if (form.match_type.value() == PasswordForm::MatchType::kExact) {
    return GetLoginMatchType::kExact;
  }

  if (static_cast<int>(form.match_type.value() &
                       PasswordForm::MatchType::kAffiliated)) {
    return GetLoginMatchType::kAffiliated;
  }

  if (static_cast<int>(form.match_type.value() &
                       PasswordForm::MatchType::kPSL)) {
    return GetLoginMatchType::kPSL;
  }

  if (static_cast<int>(form.match_type.value() &
                       PasswordForm::MatchType::kGrouped) &&
      base::FeatureList::IsEnabled(
          password_manager::features::kFillingAcrossGroupedSites)) {
    // TODO(crbug.com/1432264): Update after proper handling of grouped matches
    // is implemented.
    return GetLoginMatchType::kAffiliated;
  }

  NOTREACHED_NORETURN();
}

void FindBestMatches(
    const std::vector<const PasswordForm*>& non_federated_matches,
    PasswordForm::Scheme scheme,
    std::vector<const PasswordForm*>* non_federated_same_scheme,
    std::vector<const PasswordForm*>* best_matches) {
  DCHECK(base::ranges::none_of(non_federated_matches,
                               &PasswordForm::blocked_by_user));
  DCHECK(non_federated_same_scheme);
  DCHECK(best_matches);

  best_matches->clear();
  non_federated_same_scheme->clear();

  for (auto* match : non_federated_matches) {
    if (match->scheme == scheme)
      non_federated_same_scheme->push_back(match);
  }

  if (non_federated_same_scheme->empty())
    return;

  std::sort(non_federated_same_scheme->begin(),
            non_federated_same_scheme->end(), IsBetterMatch);

  // Map from usernames to the best matching password forms.
  std::map<std::u16string, std::vector<const PasswordForm*>>
      matches_per_username;
  for (const PasswordForm* match : *non_federated_same_scheme) {
    auto it = matches_per_username.find(match->username_value);
    // The first match for |username_value| in the sorted array is best
    // match.
    if (it == matches_per_username.end()) {
      matches_per_username[match->username_value] = {match};
      best_matches->push_back(match);
    } else {
      // Insert another credential only if the store is different as well as the
      // password value.
      if (base::Contains(it->second, match->in_store,
                         [](const auto* form) { return form->in_store; })) {
        continue;
      };
      if (base::Contains(
              it->second, match->password_value,
              [](const auto* form) { return form->password_value; })) {
        continue;
      };
      best_matches->push_back(match);
      it->second.push_back(match);
    }
  }
}

const PasswordForm* FindFormByUsername(
    const std::vector<const PasswordForm*>& forms,
    const std::u16string& username_value) {
  for (const PasswordForm* form : forms) {
    if (form->username_value == username_value)
      return form;
  }
  return nullptr;
}

const PasswordForm* GetMatchForUpdating(
    const PasswordForm& submitted_form,
    const std::vector<const PasswordForm*>& credentials,
    bool username_updated_in_bubble) {
  // This is the case for the credential management API. It should not depend on
  // form managers. Once that's the case, this should be turned into a DCHECK.
  // TODO(crbug/947030): turn it into a DCHECK.
  if (!submitted_form.federation_origin.opaque())
    return nullptr;

  // Try to return form with matching |username_value|.
  const PasswordForm* username_match =
      FindFormByUsername(credentials, submitted_form.username_value);
  if (username_match) {
    if (GetMatchType(*username_match) != GetLoginMatchType::kPSL)
      return username_match;

    const auto& password_to_save = submitted_form.new_password_value.empty()
                                       ? submitted_form.password_value
                                       : submitted_form.new_password_value;
    // Normally, the copy of the PSL matched credentials, adapted for the
    // current domain, is saved automatically without asking the user, because
    // the copy likely represents the same account, i.e., the one for which
    // the user already agreed to store a password.
    //
    // However, if the user changes the suggested password, it might indicate
    // that the autofilled credentials and |submitted_password_form|
    // actually correspond to two different accounts (see
    // http://crbug.com/385619).
    return password_to_save == username_match->password_value ? username_match
                                                              : nullptr;
  }

  // Next attempt is to find a match by password value. It should not be tried
  // when the username was actually detected.
  if (submitted_form.type == PasswordForm::Type::kApi ||
      !submitted_form.username_value.empty()) {
    return nullptr;
  }

  for (const PasswordForm* stored_match : credentials) {
    if (stored_match->password_value == submitted_form.password_value)
      return stored_match;
  }

  // If the user manually changed the username value: consider this at this
  // point of the heuristic a new credential (didn't match other
  // passwords/usernames).
  if (username_updated_in_bubble)
    return nullptr;

  // Last try. The submitted form had no username but a password. Assume that
  // it's an existing credential.
  return credentials.empty() ? nullptr : credentials.front();
}

PasswordForm MakeNormalizedBlocklistedForm(
    password_manager::PasswordFormDigest digest) {
  PasswordForm result;
  result.blocked_by_user = true;
  result.scheme = std::move(digest.scheme);
  result.signon_realm = std::move(digest.signon_realm);
  // In case |digest| corresponds to an Android credential copy the origin as
  // is, otherwise clear out the path by calling GetOrigin().
  if (password_manager::FacetURI::FromPotentiallyInvalidSpec(digest.url.spec())
          .IsValidAndroidFacetURI()) {
    result.url = std::move(digest.url);
  } else {
    // GetOrigin() will return an empty GURL if the origin is not valid or
    // standard. DCHECK that this will not happen.
    DCHECK(digest.url.is_valid());
    DCHECK(digest.url.IsStandard());
    result.url = digest.url.DeprecatedGetOriginAsURL();
  }
  return result;
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
bool ShouldBiometricAuthenticationForFillingToggleBeVisible(
    const PrefService* local_state) {
  return local_state->GetBoolean(
      password_manager::prefs::kHadBiometricsAvailable);
}

bool ShouldShowBiometricAuthenticationBeforeFillingPromo(
    password_manager::PasswordManagerClient* client) {
  std::unique_ptr<device_reauth::DeviceAuthenticator> device_authenticator =
      client->GetDeviceAuthenticator();
  return client && device_authenticator &&
         device_authenticator->CanAuthenticateWithBiometrics() &&
         !client->GetPrefs()->GetBoolean(
             password_manager::prefs::kBiometricAuthenticationBeforeFilling);
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

bool CanUseBiometricAuth(device_reauth::DeviceAuthenticator* authenticator,
                         password_manager::PasswordManagerClient* client) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  if (!client || !client->GetLocalStatePrefs() || !client->GetPrefs() ||
      !authenticator) {
    return false;
  }
  return client->GetPasswordFeatureManager()
      ->IsBiometricAuthenticationBeforeFillingEnabled();
#elif BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    CHECK(authenticator);
    return true;
  }
  return authenticator && authenticator->CanAuthenticateWithBiometrics() &&
         base::FeatureList::IsEnabled(
             password_manager::features::kBiometricTouchToFill);
#else
  return false;
#endif
}

GURL StripAuthAndParams(const GURL& gurl) {
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  rep.ClearQuery();
  rep.ClearRef();
  return gurl.ReplaceComponents(rep);
}

GURL ConstructGURLWithScheme(const std::string& url) {
  GURL gurl = GURL(url);
  if (!gurl.has_scheme()) {
    GURL https_url(
        base::StrCat({url::kHttpsScheme, url::kStandardSchemeSeparator, url}));
    if (url::HostIsIPAddress(https_url.host())) {
      GURL::Replacements replacements;
      replacements.SetSchemeStr(url::kHttpScheme);
      return https_url.ReplaceComponents(replacements);
    }
    return https_url;
  }
  return gurl;
}

std::string GetSignonRealm(const GURL& url) {
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  rep.ClearQuery();
  rep.ClearRef();
  rep.SetPathStr("");
  return url.ReplaceComponents(rep).spec();
}

#if BUILDFLAG(IS_IOS)
bool IsCredentialProviderEnabledOnStartup(const PrefService* prefs) {
  return prefs->GetBoolean(
      password_manager::prefs::kCredentialProviderEnabledOnStartup);
}

void SetCredentialProviderEnabledOnStartup(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(
      password_manager::prefs::kCredentialProviderEnabledOnStartup, enabled);
}
#endif

bool IsNumeric(char16_t c) {
  return '0' <= c && c <= '9';
}

bool IsLetter(char16_t c) {
  return IsLowercaseLetter(c) || IsUppercaseLetter(c);
}

bool IsLowercaseLetter(char16_t c) {
  return 'a' <= c && c <= 'z';
}

bool IsUppercaseLetter(char16_t c) {
  return 'A' <= c && c <= 'Z';
}

bool IsSpecialSymbol(char16_t c) {
  return base::Contains(kSpecialSymbols, c);
}

bool IsSingleUsernameType(autofill::ServerFieldType type) {
  return type == autofill::SINGLE_USERNAME ||
         (type == autofill::SINGLE_USERNAME_FORGOT_PASSWORD &&
          base::FeatureList::IsEnabled(
              password_manager::features::kForgotPasswordFormSupport)) ||
         (type == autofill::SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES &&
          base::FeatureList::IsEnabled(
              password_manager::features::
                  kUsernameFirstFlowWithIntermediateValuesPredictions));
}

}  // namespace password_manager_util
