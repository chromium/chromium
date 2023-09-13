// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_util.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/credentials_cleaner.h"
#include "components/password_manager/core/browser/credentials_cleaner_runner.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/http_credentials_cleaner.h"
#include "components/password_manager/core/browser/old_google_credentials_cleaner.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/url_util.h"

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

// Appends a new level to the |main_domain| from |full_domain|.
// |main_domain| must be a suffix of |full_domain|.
void IncreaseDomainLevel(const std::string& full_domain,
                         std::string& main_domain) {
  DCHECK_GT(full_domain.size(), main_domain.size());
  auto starting_pos = full_domain.rbegin() + main_domain.size();
  // Verify that we are at '.' and move to the next character.
  DCHECK_EQ(*starting_pos, '.');
  starting_pos++;
  // Find next '.' from |starting_pos|
  auto ending_pos = std::find(starting_pos, full_domain.rend(), '.');
  main_domain = std::string(ending_pos.base(), full_domain.end());
}

// An implementation of the disjoint-set data structure
// (https://en.wikipedia.org/wiki/Disjoint-set_data_structure). This
// implementation uses the path compression and union by rank optimizations,
// achieving near-constant runtime on all operations.
//
// This data structure allows to keep track of disjoin sets. Constructor accepts
// number of elements and initially each element represent an individual set.
// Later by calling MergeSets corresponding sets are merged together.
// Example usage:
//   DisjointSet disjoint_set(5);
//   disjoint_set.GetDisjointSets(); // Returns {{0}, {1}, {2}, {3}, {4}}
//   disjoint_set.MergeSets(0, 2);
//   disjoint_set.GetDisjointSets(); // Returns {{0, 2}, {1}, {3}, {4}}
//   disjoint_set.MergeSets(2, 4);
//   disjoint_set.GetDisjointSets(); // Returns {{0, 2, 4}, {1}, {3}}
class DisjointSet {
 public:
  explicit DisjointSet(size_t size) : parent_id_(size), ranks_(size, 0) {
    for (size_t i = 0; i < size; i++) {
      parent_id_[i] = i;
    }
  }

  // Merges two sets based on their rank. Set with higher rank becomes a parent
  // for another set.
  void MergeSets(int set1, int set2) {
    set1 = GetRoot(set1);
    set2 = GetRoot(set2);
    if (set1 == set2) {
      return;
    }

    // Update parent based on rank.
    if (ranks_[set1] > ranks_[set2]) {
      parent_id_[set2] = set1;
    } else {
      parent_id_[set1] = set2;
      // if ranks were equal increment by one new root's rank.
      if (ranks_[set1] == ranks_[set2]) {
        ranks_[set2]++;
      }
    }
  }

  // Returns disjoin sets after merging. It's guarantee that the result will
  // hold all elements.
  std::vector<std::vector<int>> GetDisjointSets() {
    std::vector<std::vector<int>> disjoint_sets(parent_id_.size());
    for (size_t i = 0; i < parent_id_.size(); i++) {
      // Append all elements to the root.
      int root = GetRoot(i);
      disjoint_sets[root].push_back(i);
    }
    // Clear empty sets.
    base::EraseIf(disjoint_sets, [](const auto& set) { return set.empty(); });
    return disjoint_sets;
  }

 private:
  // Returns root for a given element.
  int GetRoot(int index) {
    if (index == parent_id_[index]) {
      return index;
    }
    // To speed up future lookups flatten the tree along the way.
    return parent_id_[index] = GetRoot(parent_id_[index]);
  }

  // Vector where element at i'th position holds a parent for i.
  std::vector<int> parent_id_;

  // Upper bound depth of a tree for i'th element.
  std::vector<size_t> ranks_;
};

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

password_manager::SyncState GetPasswordSyncState(
    const syncer::SyncService* sync_service) {
  if (!sync_service ||
      !sync_service->GetActiveDataTypes().Has(syncer::PASSWORDS)) {
    return password_manager::SyncState::kNotSyncing;
  }

  if (sync_service->IsSyncFeatureActive()) {
    return sync_service->GetUserSettings()->IsUsingExplicitPassphrase()
               ? password_manager::SyncState::kSyncingWithCustomPassphrase
               : password_manager::SyncState::kSyncingNormalEncryption;
  }

  DCHECK(base::FeatureList::IsEnabled(
      password_manager::features::kEnablePasswordsAccountStorage));

  return sync_service->GetUserSettings()->IsUsingExplicitPassphrase()
             ? password_manager::SyncState::
                   kAccountPasswordsActiveWithCustomPassphrase
             : password_manager::SyncState::
                   kAccountPasswordsActiveNormalEncryption;
}

void TrimUsernameOnlyCredentials(
    std::vector<std::unique_ptr<PasswordForm>>* android_credentials) {
  // Remove username-only credentials which are not federated.
  base::EraseIf(*android_credentials,
                [](const std::unique_ptr<PasswordForm>& form) {
                  return form->scheme == PasswordForm::Scheme::kUsernameOnly &&
                         form->federation_origin.opaque();
                });

  // Set "skip_zero_click" on federated credentials.
  base::ranges::for_each(
      *android_credentials, [](const std::unique_ptr<PasswordForm>& form) {
        if (form->scheme == PasswordForm::Scheme::kUsernameOnly)
          form->skip_zero_click = true;
      });
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

// TODO(http://crbug.com/890318): Add unitests to check cleaners are correctly
// created.
void RemoveUselessCredentials(
    password_manager::CredentialsCleanerRunner* cleaning_tasks_runner,
    scoped_refptr<password_manager::PasswordStoreInterface> store,
    PrefService* prefs,
    base::TimeDelta delay,
    base::RepeatingCallback<network::mojom::NetworkContext*()>
        network_context_getter) {
  DCHECK(cleaning_tasks_runner);

#if BUILDFLAG(USE_BLINK)
  // Can be null for some unittests.
  if (!network_context_getter.is_null()) {
    cleaning_tasks_runner->MaybeAddCleaningTask(
        std::make_unique<password_manager::HttpCredentialCleaner>(
            store, network_context_getter, prefs));
  }
#endif  // BUILDFLAG(USE_BLINK)

  // TODO(crbug.com/450621): Remove this when enough number of clients switch
  // to the new version of Chrome.
  cleaning_tasks_runner->MaybeAddCleaningTask(
      std::make_unique<password_manager::OldGoogleCredentialCleaner>(store,
                                                                     prefs));

  if (cleaning_tasks_runner->HasPendingTasks()) {
    // The runner will delete itself once the clearing tasks are done, thus we
    // are releasing ownership here.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &password_manager::CredentialsCleanerRunner::StartCleaning,
            cleaning_tasks_runner->GetWeakPtr()),
        delay);
  }
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
             password_manager::prefs::kHadBiometricsAvailable) &&
         base::FeatureList::IsEnabled(
             password_manager::features::kBiometricAuthenticationForFilling);
}

bool ShouldShowBiometricAuthenticationBeforeFillingPromo(
    password_manager::PasswordManagerClient* client) {
  return client && client->GetDeviceAuthenticator() &&
         client->GetDeviceAuthenticator()->CanAuthenticateWithBiometrics() &&
         base::FeatureList::IsEnabled(
             password_manager::features::kBiometricAuthenticationForFilling) &&
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
#else
  return authenticator && authenticator->CanAuthenticateWithBiometrics() &&
         base::FeatureList::IsEnabled(
             password_manager::features::kBiometricTouchToFill);
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

bool IsValidPasswordURL(const GURL& url) {
  return url.is_valid() &&
         (url.SchemeIsHTTPOrHTTPS() ||
          password_manager::IsValidAndroidFacetURI(url.spec()));
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

std::string GetExtendedTopLevelDomain(
    const GURL& url,
    const base::flat_set<std::string>& psl_extensions) {
  std::string main_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  if (main_domain.empty()) {
    return main_domain;
  }

  std::string full_domain = url.host();

  // Something went wrong, and it shouldn't happen. Return early in this case to
  // avoid undefined behaviour.
  if (!base::EndsWith(full_domain, main_domain)) {
    return main_domain;
  }

  // If a domain is contained within the PSL extension list, an additional
  // subdomain is added to that domain. This is done until the domain is not
  // contained within the PSL extension list or fully shown. For multi-level
  // extension, this approach only works if all sublevels are included in the
  // PSL extension list.
  while (main_domain != full_domain && psl_extensions.contains(main_domain)) {
    IncreaseDomainLevel(full_domain, main_domain);
  }
  return main_domain;
}

std::vector<password_manager::GroupedFacets> MergeRelatedGroups(
    const base::flat_set<std::string>& psl_extensions,
    const std::vector<password_manager::GroupedFacets>& groups) {
  DisjointSet unions(groups.size());
  std::map<std::string, int> main_domain_to_group;

  for (size_t i = 0; i < groups.size(); i++) {
    for (auto& facet : groups[i].facets) {
      if (facet.uri.IsValidAndroidFacetURI()) {
        continue;
      }

      // If domain is empty - compute it manually.
      std::string main_domain =
          facet.main_domain.empty()
              ? GetExtendedTopLevelDomain(
                    GURL(facet.uri.potentially_invalid_spec()), psl_extensions)
              : facet.main_domain;

      if (main_domain.empty()) {
        continue;
      }

      auto it = main_domain_to_group.find(main_domain);
      if (it == main_domain_to_group.end()) {
        main_domain_to_group[main_domain] = i;
        continue;
      }
      unions.MergeSets(i, it->second);
    }
  }

  std::vector<password_manager::GroupedFacets> result;
  for (const auto& merged_groups : unions.GetDisjointSets()) {
    password_manager::GroupedFacets group;
    for (int group_id : merged_groups) {
      // Move all the elements into a new vector.
      group.facets.insert(group.facets.end(), groups[group_id].facets.begin(),
                          groups[group_id].facets.end());
      // Use non-empty name for a combined group.
      if (!groups[group_id].branding_info.icon_url.is_empty()) {
        group.branding_info = groups[group_id].branding_info;
      }
    }

    result.push_back(std::move(group));
  }
  return result;
}

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
              password_manager::features::kForgotPasswordFormSupport));
}

}  // namespace password_manager_util
