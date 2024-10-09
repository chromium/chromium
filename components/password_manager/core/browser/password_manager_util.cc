// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_util.h"

#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/password_generation_util.h"
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
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "components/password_manager/core/browser/password_sync_util.h"

using password_manager::sync_util::IsSyncFeatureEnabledIncludingPasswords;
#endif

using autofill::password_generation::PasswordGenerationType;
using password_manager::PasswordForm;

namespace password_manager_util {
namespace {

std::tuple<int, base::Time, int> GetPriorityProperties(
    const PasswordForm& form) {
  return std::make_tuple(-static_cast<int>(GetMatchType(form)),
                         form.date_last_used, static_cast<int>(form.in_store));
}

// Consider the following properties:
// 1. Match strength for the original form (Exact > Affiliations > PSL).
// 2. Last time used. Most recent is better.
// 3. Account vs. profile store. Account is better.
bool IsBetterMatch(const PasswordForm& lhs, const PasswordForm& rhs) {
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
  if (!password_manager) {
    return false;
  }

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
    autofill_client->HideAutofillSuggestions(
        autofill::SuggestionHidingReason::
            kOverlappingWithPasswordGenerationPopup);
    autofill_client->HideAutofillFieldIph();
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

bool IsAbleToSavePasswords(password_manager::PasswordManagerClient* client) {
#if BUILDFLAG(IS_ANDROID)
  if (password_manager::UsesSplitStoresAndUPMForLocal(client->GetPrefs()) &&
      password_manager::sync_util::HasChosenToSyncPasswords(
          client->GetSyncService())) {
    // After store split on Android, AccountPasswordStore is a default store for
    // saving passwords when sync is enabled. If either of conditions above is
    // not satisfied fallback to ProfilePasswordStore.
    return client->GetAccountPasswordStore() &&
           client->GetAccountPasswordStore()->IsAbleToSavePasswords();
  }
#endif
  // TODO(b/324054761): Check AccountPasswordStore store when needed.
  return client->GetProfilePasswordStore() &&
         client->GetProfilePasswordStore()->IsAbleToSavePasswords();
}

std::string_view GetSignonRealmWithProtocolExcluded(const PasswordForm& form) {
  std::string_view signon_realm = form.signon_realm;

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
                       PasswordForm::MatchType::kGrouped)) {
    return GetLoginMatchType::kGrouped;
  }

  NOTREACHED();
}

std::vector<PasswordForm> FindBestMatches(base::span<PasswordForm> matches) {
  CHECK(base::ranges::none_of(matches, &PasswordForm::blocked_by_user));

  base::ranges::sort(matches, IsBetterMatch, {});

  std::vector<PasswordForm> best_matches;

  // Map from usernames to the best matching password forms.
  std::map<std::u16string, std::vector<PasswordForm>> matches_per_username;
  for (auto& match : matches) {
    auto it = matches_per_username.find(match.username_value);
    // The first match for |username_value| in the sorted array is best
    // match.
    if (it == matches_per_username.end()) {
      matches_per_username[match.username_value] = {match};
      best_matches.push_back(match);
    } else {
      // Insert another credential only if the store is different as well as the
      // password value.
      if (base::Contains(it->second, match.in_store,
                         [](const auto& form) { return form.in_store; })) {
        continue;
      };
      // If 2 credential have the same password and the same username, update
      // the in_store value in the best matches.
      auto duplicate_match_it = base::ranges::find_if(
          best_matches, [&match](const PasswordForm& form) {
            return match.username_value == form.username_value &&
                   match.password_value == form.password_value;
          });
      if (duplicate_match_it != best_matches.end()) {
        duplicate_match_it->in_store =
            duplicate_match_it->in_store | match.in_store;
        continue;
      }
      best_matches.push_back(match);
      it->second.push_back(match);
    }
  }
  return best_matches;
}

const PasswordForm* FindFormByUsername(base::span<const PasswordForm> forms,
                                       const std::u16string& username_value) {
  for (const PasswordForm& form : forms) {
    if (form.username_value == username_value) {
      return &form;
    }
  }
  return nullptr;
}

const PasswordForm* FindFormByUsername(
    const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>& forms,
    const std::u16string& username_value) {
  for (const PasswordForm* form : forms) {
    if (form->username_value == username_value) {
      return form;
    }
  }
  return nullptr;
}

const PasswordForm* GetMatchForUpdating(
    const PasswordForm& submitted_form,
    const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&
        credentials,
    bool username_updated_in_bubble) {
  // This is the case for the credential management API. It should not depend on
  // form managers. Once that's the case, this should be turned into a DCHECK.
  // TODO(crbug.com/40620575): turn it into a DCHECK.
  if (submitted_form.IsFederatedCredential()) {
    return nullptr;
  }

  // Try to return form with matching |username_value|.
  const PasswordForm* username_match =
      FindFormByUsername(credentials, submitted_form.username_value);
  if (username_match) {
    if (GetMatchType(*username_match) != GetLoginMatchType::kPSL) {
      return username_match;
    }

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
    if (stored_match->password_value == submitted_form.password_value) {
      return stored_match;
    }
  }

  // If the user manually changed the username value: consider this at this
  // point of the heuristic a new credential (didn't match other
  // passwords/usernames).
  if (username_updated_in_bubble) {
    return nullptr;
  }

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
  if (affiliations::FacetURI::FromPotentiallyInvalidSpec(digest.url.spec())
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

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/367336383): Make the this block Ash only for
// ChromeOS.
bool ShouldBiometricAuthenticationForFillingToggleBeVisible(
    const PrefService* local_state) {
  bool hadBiometricsAvailable =
      local_state->GetBoolean(password_manager::prefs::kHadBiometricsAvailable);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // We only want to check for feature flag if the device supports biometrics,
  // else we dilute experiment population.
  return hadBiometricsAvailable &&
         base::FeatureList::IsEnabled(
             password_manager::features::kBiometricsAuthForPwdFill);
#else
  return hadBiometricsAvailable;
#endif
}

bool ShouldShowBiometricAuthenticationBeforeFillingPromo(
    password_manager::PasswordManagerClient* client) {
  // The following order of preference checks need to happen in order for us to
  // preserve the experiment setup. Specifically, we only want to check for
  // feature flag if the device supports biometrics, else we dilute experiment
  // population.
  if (!client) {
    return false;
  }
  std::unique_ptr<device_reauth::DeviceAuthenticator> device_authenticator =
      client->GetDeviceAuthenticator();
  if (!device_authenticator) {
    return false;
  }

  if (!device_authenticator->CanAuthenticateWithBiometrics()) {
    return false;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Note: Hitting IsEnabled enrolls users in the experiment. Therefore, we only
  // want to limit this call to users who can authenticate with biometrics and
  // if we are here, then we know that to be the case.
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kBiometricsAuthForPwdFill)) {
    return false;
  }
#endif
  return !client->GetPrefs()->GetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling);
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)

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
bool IsCredentialProviderEnabledOnStartup(const PrefService* local_state) {
  return local_state->GetBoolean(
      password_manager::prefs::kCredentialProviderEnabledOnStartup);
}

void SetCredentialProviderEnabledOnStartup(PrefService* local_state,
                                           bool enabled) {
  local_state->SetBoolean(
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
  // The static assert is intended to ensure that the underlying type of
  // `kSpecialSymbols` does not become a char. If that happened, the call to
  // `base::Contains` would lead to (silent) overflow.
  static_assert(sizeof(decltype(kSpecialSymbols)::value_type) == sizeof(c));
  return base::Contains(kSpecialSymbols, c);
}

bool IsSingleUsernameType(autofill::FieldType type) {
  return type == autofill::SINGLE_USERNAME ||
         type == autofill::SINGLE_USERNAME_FORGOT_PASSWORD ||
         (type == autofill::SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES &&
          base::FeatureList::IsEnabled(
              password_manager::features::
                  kUsernameFirstFlowWithIntermediateValuesPredictions));
}

}  // namespace password_manager_util
