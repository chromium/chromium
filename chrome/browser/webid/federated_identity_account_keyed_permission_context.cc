// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_account_keyed_permission_context.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context.h"
#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context_factory.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/origin.h"

namespace {
constexpr char kAccountIdsKey[] = "account-ids";
constexpr char kRpRequesterKey[] = "rp-requester";
constexpr char kRpEmbedderKey[] = "rp-embedder";
constexpr char kSharingIdpKey[] = "idp-origin";
constexpr char kAccountIdKey[] = "account-id";
constexpr char kTimestampKey[] = "timestamp";

std::string BuildKey(const std::optional<std::string>& relying_party_requester,
                     const std::optional<std::string>& relying_party_embedder,
                     const std::string& identity_provider) {
  if (relying_party_requester && relying_party_embedder &&
      *relying_party_requester != *relying_party_embedder) {
    return base::StringPrintf("%s<%s", identity_provider.c_str(),
                              relying_party_embedder->c_str());
  }
  // Use `identity_provider` as the key when
  // `relying_party_requester` == `relying_party_embedder` for the sake of
  // backwards compatibility with permissions stored prior to addition of
  // `relying_party_embedder` key.
  return identity_provider;
}

std::string BuildKey(const url::Origin& relying_party_requester,
                     const url::Origin& relying_party_embedder,
                     const url::Origin& identity_provider) {
  return BuildKey(relying_party_requester.Serialize(),
                  relying_party_embedder.Serialize(),
                  identity_provider.Serialize());
}

base::Value::List::iterator FindAccount(base::Value::List& account_list,
                                        const std::string& account_id) {
  return std::find_if(account_list.begin(), account_list.end(),
                      [&account_id](const auto& value) {
                        if (value.is_string()) {
                          return value.GetString() == account_id;
                        } else if (value.is_dict()) {
                          const std::string* id =
                              value.GetDict().FindString(kAccountIdKey);
                          return id && *id == account_id;
                        }
                        // This is currently unreachable but we just bail out if
                        // it is not a string or dict to future-proof the code.
                        return false;
                      });
}

}  // namespace

FederatedIdentityAccountKeyedPermissionContext::
    FederatedIdentityAccountKeyedPermissionContext(
        content::BrowserContext* browser_context)
    : FederatedIdentityAccountKeyedPermissionContext(
          browser_context,
          HostContentSettingsMapFactory::GetForProfile(
              Profile::FromBrowserContext(browser_context))) {}

FederatedIdentityAccountKeyedPermissionContext::
    FederatedIdentityAccountKeyedPermissionContext(
        content::BrowserContext* browser_context,
        HostContentSettingsMap* host_content_settings_map)
    : ObjectPermissionContextBase(
          ContentSettingsType::FEDERATED_IDENTITY_SHARING,
          host_content_settings_map),
      browser_context_(
          raw_ref<content::BrowserContext>::from_ptr(browser_context)),
      clock_(base::DefaultClock::GetInstance()) {}

FederatedIdentityAccountKeyedPermissionContext::
    ~FederatedIdentityAccountKeyedPermissionContext() = default;

bool FederatedIdentityAccountKeyedPermissionContext::HasPermission(
    const url::Origin& relying_party_requester) {
  for (const auto& object : GetGrantedObjects(relying_party_requester)) {
    const base::Value::List* account_list =
        object->value.FindList(kAccountIdsKey);
    if (account_list) {
      return true;
    }
  }
  return false;
}

bool FederatedIdentityAccountKeyedPermissionContext::HasPermission(
    const net::SchemefulSite& relying_party_embedder,
    const net::SchemefulSite& identity_provider) {
  return base::ranges::any_of(
      GetAllGrantedObjects(),
      [&](const std::unique_ptr<
          permissions::ObjectPermissionContextBase::Object>& object) -> bool {
        if (!object) {
          return false;
        }

        const std::string* rp_embedder_origin =
            object->value.FindString(kRpEmbedderKey);
        const std::string* idp_origin =
            object->value.FindString(kSharingIdpKey);

        return rp_embedder_origin && idp_origin &&
               net::SchemefulSite(GURL(*rp_embedder_origin)) ==
                   relying_party_embedder &&
               net::SchemefulSite(GURL(*idp_origin)) == identity_provider;
      });
}

bool FederatedIdentityAccountKeyedPermissionContext::HasPermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider) {
  // TODO(crbug.com/40846157): This is currently origin-bound, but we would like
  // this grant to apply at the 'site' (aka eTLD+1) level. We should override
  // GetGrantedObject to find a grant that matches the RP's site rather
  // than origin, and also ensure that duplicate sites cannot be added.
  std::string key = BuildKey(relying_party_requester, relying_party_embedder,
                             identity_provider);
  const auto granted_object = GetGrantedObject(relying_party_requester, key);

  if (!granted_object) {
    return false;
  }

  base::Value::List* account_list =
      granted_object->value.FindList(kAccountIdsKey);
  return !!account_list;
}

std::optional<base::Time>
FederatedIdentityAccountKeyedPermissionContext::GetLastUsedTimestamp(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  std::string key = BuildKey(relying_party_requester, relying_party_embedder,
                             identity_provider);
  const auto granted_object = GetGrantedObject(relying_party_requester, key);

  if (!granted_object) {
    return std::nullopt;
  }

  base::Value::List* account_list =
      granted_object->value.FindList(kAccountIdsKey);
  if (!account_list) {
    return std::nullopt;
  }

  auto it = FindAccount(*account_list, account_id);
  if (it == account_list->end()) {
    return std::nullopt;
  }

  if (it->is_string()) {
    // The account is returning but we do not have a timestamp.
    return base::Time();
  } else if (it->is_dict()) {
    base::Value* timestamp = it->GetDict().Find(kTimestampKey);
    CHECK(timestamp);
    std::optional<base::Time> time = base::ValueToTime(timestamp);
    // We stored a time in here, so it shouldn't fail when retrieving it.
    DCHECK(time);
    return time.value_or(base::Time());
  }
  // We do not expect this to happen, but account was found and no timestamp in
  // this case.
  return base::Time();
}

void FederatedIdentityAccountKeyedPermissionContext::GrantPermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  if (RefreshExistingPermission(relying_party_requester, relying_party_embedder,
                                identity_provider, account_id)) {
    return;
  }
  std::string key = BuildKey(relying_party_requester, relying_party_embedder,
                             identity_provider);
  const auto granted_object = GetGrantedObject(relying_party_requester, key);
  if (granted_object) {
    base::Value::Dict new_object = granted_object->value.Clone();
    AddToAccountList(new_object, account_id);
    UpdateObjectPermission(relying_party_requester, granted_object->value,
                           std::move(new_object));
  } else {
    base::Value::Dict new_object;
    new_object.Set(kRpRequesterKey, relying_party_requester.Serialize());
    new_object.Set(kRpEmbedderKey, relying_party_embedder.Serialize());
    new_object.Set(kSharingIdpKey, identity_provider.Serialize());
    AddToAccountList(new_object, account_id);
    GrantObjectPermission(relying_party_requester, std::move(new_object));
  }

  SyncSharingPermissionGrantsToNetworkService(base::DoNothing());
}

bool FederatedIdentityAccountKeyedPermissionContext::RefreshExistingPermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  std::string key = BuildKey(relying_party_requester, relying_party_embedder,
                             identity_provider);
  const auto granted_object = GetGrantedObject(relying_party_requester, key);
  if (!granted_object) {
    return false;
  }

  base::Value::Dict new_object = granted_object->value.Clone();
  base::Value::List* account_list = new_object.FindList(kAccountIdsKey);
  if (!account_list) {
    return false;
  }

  auto it = FindAccount(*account_list, account_id);
  if (it == account_list->end()) {
    return false;
  }
  if (it->is_string()) {
    // Erase the previous string, and add a list (id, timestamp).
    account_list->erase(it);

    base::Value::Dict account_dict;
    account_dict.Set(kAccountIdKey, account_id);
    account_dict.Set(kTimestampKey, base::TimeToValue(clock_->Now()));
    account_list->Append(std::move(account_dict));
  } else if (it->is_dict()) {
    // Update the last used timestamp of the (id, timestamp) pair.
    it->GetDict().Set(kTimestampKey, base::TimeToValue(clock_->Now()));
  }
  UpdateObjectPermission(relying_party_requester, granted_object->value,
                         std::move(new_object));
  return true;
}

void FederatedIdentityAccountKeyedPermissionContext::RevokePermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id,
    base::OnceClosure callback) {
  std::string key = BuildKey(relying_party_requester, relying_party_embedder,
                             identity_provider);
  const auto object = GetGrantedObject(relying_party_requester, key);
  if (!object) {
    std::move(callback).Run();
    return;
  }

  base::Value::Dict new_object = object->value.Clone();
  base::Value::List* account_ids = new_object.FindList(kAccountIdsKey);
  if (account_ids) {
    auto it = FindAccount(*account_ids, account_id);
    if (it != account_ids->end()) {
      account_ids->erase(it);
    } else {
      account_ids->clear();
    }
  }

  // Remove the permission object if there is no account left.
  if (!account_ids || account_ids->size() == 0) {
    RevokeObjectPermission(relying_party_requester, key);
  } else {
    UpdateObjectPermission(relying_party_requester, object->value,
                           std::move(new_object));
  }

  // No need to erase from `storage_access_eligible_connections_` here; it's ok
  // if that set is a superset of the actually-granted permissions, since we
  // only send the intersection to the network service anyway. Furthermore, it's
  // tricky to correctly erase here, since there may be multiple permissions
  // with different origins but the same sites.

  SyncSharingPermissionGrantsToNetworkService(std::move(callback));
}

void FederatedIdentityAccountKeyedPermissionContext::MarkStorageAccessEligible(
    const net::SchemefulSite& relying_party_embedder,
    const net::SchemefulSite& identity_provider,
    base::OnceClosure callback) {
  CHECK(HasPermission(relying_party_embedder, identity_provider));
  storage_access_eligible_connections_.insert(
      std::make_pair(relying_party_embedder, identity_provider));

  SyncSharingPermissionGrantsToNetworkService(std::move(callback));
}

void FederatedIdentityAccountKeyedPermissionContext::OnSetRequiresUserMediation(
    const url::Origin& relying_party,
    base::OnceClosure callback) {
  net::SchemefulSite relying_party_site(relying_party);
  if (base::ranges::none_of(
          storage_access_eligible_connections_, [&](const auto& pair) -> bool {
            return net::SchemefulSite(pair.first) == relying_party_site;
          })) {
    std::move(callback).Run();
    return;
  }
  SyncSharingPermissionGrantsToNetworkService(std::move(callback));
}

std::string FederatedIdentityAccountKeyedPermissionContext::GetKeyForObject(
    const base::Value::Dict& object) {
  DCHECK(IsValidObject(object));
  const std::string* rp_requester_origin = object.FindString(kRpRequesterKey);
  const std::string* rp_embedder_origin = object.FindString(kRpEmbedderKey);
  const std::string* idp_origin = object.FindString(kSharingIdpKey);
  return BuildKey(
      rp_requester_origin ? std::optional<std::string>(*rp_requester_origin)
                          : std::nullopt,
      rp_embedder_origin ? std::optional<std::string>(*rp_embedder_origin)
                         : std::nullopt,
      *idp_origin);
}

bool FederatedIdentityAccountKeyedPermissionContext::IsValidObject(
    const base::Value::Dict& object) {
  return object.FindString(kSharingIdpKey);
}

std::u16string
FederatedIdentityAccountKeyedPermissionContext::GetObjectDisplayName(
    const base::Value::Dict& object) {
  DCHECK(IsValidObject(object));
  return base::UTF8ToUTF16(*object.FindString(kSharingIdpKey));
}

void FederatedIdentityAccountKeyedPermissionContext::GetAllDataKeys(
    base::OnceCallback<void(
        std::vector<webid::FederatedIdentityDataModel::DataKey>)> callback) {
  auto granted_objects = GetAllGrantedObjects();
  std::vector<webid::FederatedIdentityDataModel::DataKey> data_keys;
  for (const auto& obj : granted_objects) {
    const base::Value::List* accounts = obj->value.FindList(kAccountIdsKey);
    const std::string* rp_requester_origin =
        obj->value.FindString(kRpRequesterKey);
    const std::string* rp_embedder_origin =
        obj->value.FindString(kRpEmbedderKey);
    const std::string* idp_origin = obj->value.FindString(kSharingIdpKey);

    if (!accounts || accounts->empty()) {
      continue;
    }

    CHECK(idp_origin && rp_requester_origin && rp_embedder_origin);
    for (const auto& account : *accounts) {
      if (account.is_string()) {
        data_keys.emplace_back(url::Origin::Create(GURL(*rp_requester_origin)),
                               url::Origin::Create(GURL(*rp_embedder_origin)),
                               url::Origin::Create(GURL(*idp_origin)),
                               account.GetString());
      } else if (account.is_dict()) {
        data_keys.emplace_back(url::Origin::Create(GURL(*rp_requester_origin)),
                               url::Origin::Create(GURL(*rp_embedder_origin)),
                               url::Origin::Create(GURL(*idp_origin)),
                               *account.GetDict().FindString(kAccountIdKey));
      }
    }
  }
  std::move(callback).Run(std::move(data_keys));
}

void FederatedIdentityAccountKeyedPermissionContext::
    RemoveFederatedIdentityDataByDataKey(
        const webid::FederatedIdentityDataModel::DataKey& data_key,
        base::OnceClosure callback) {
  RevokePermission(
      data_key.relying_party_requester(), data_key.relying_party_embedder(),
      data_key.identity_provider(), data_key.account_id(), std::move(callback));
}

ContentSettingsForOneType FederatedIdentityAccountKeyedPermissionContext::
    GetSharingPermissionGrantsAsContentSettings() {
  if (!base::FeatureList::IsEnabled(
          blink::features::kFedCmWithStorageAccessAPI)) {
    return ContentSettingsForOneType();
  }
  // ObjectPermissionContext stores its settings in the HostContentSettingsMap
  // keyed by <origin, null> with a value of `base::Value` (which is translated
  // to CONTENT_SETTING_DEFAULT). It's not possible to reconstruct the actual
  // <RP requester, RP embedder, IDP> grants from this, so we use the raw
  // objects instead, and construct the corresponding list of settings with
  // appropriate primary/secondary keys.
  //
  // Note that these settings patterns are keyed by <site, site> rather than
  // <origin, origin>.

  ContentSettingsForOneType settings;
  FederatedIdentityAutoReauthnPermissionContext* reauth_context =
      FederatedIdentityAutoReauthnPermissionContextFactory::GetForProfile(
          &*browser_context_);

  for (const std::unique_ptr<Object>& object : GetAllGrantedObjects()) {
    if (!object) {
      continue;
    }

    const std::string* rp_embedder_origin =
        object->value.FindString(kRpEmbedderKey);
    const std::string* idp_origin = object->value.FindString(kSharingIdpKey);

    if (!rp_embedder_origin || !idp_origin) {
      continue;
    }

    const url::Origin rp_embedder =
        url::Origin::Create(GURL(*rp_embedder_origin));
    if (reauth_context->RequiresUserMediation(rp_embedder)) {
      continue;
    }

    const net::SchemefulSite rp_embedder_site(rp_embedder);
    const net::SchemefulSite idp_site((GURL(*idp_origin)));
    if (!storage_access_eligible_connections_.contains(
            std::make_pair(rp_embedder_site, idp_site))) {
      continue;
    }

    settings.emplace_back(
        ContentSettingsPattern::FromURLToSchemefulSitePattern(
            GURL(*idp_origin)),
        ContentSettingsPattern::FromURLToSchemefulSitePattern(
            GURL(*rp_embedder_origin)),
        content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW),
        content_settings::ProviderType::kNone,
        browser_context_->IsOffTheRecord());
  }
  return settings;
}

void FederatedIdentityAccountKeyedPermissionContext::
    SyncSharingPermissionGrantsToNetworkService(base::OnceClosure callback) {
  // Note: ProfileNetworkContextService::OnContentSettingChanged also updates
  // the network service's content settings. But we explicitly sync the
  // permissions here and then invoke the callback, to avoid a race condition
  // between the callback and the unsynchronized
  // ProfileNetworkContextService::OnContentSettingChanged invocation.
  browser_context_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetContentSettings(ContentSettingsType::FEDERATED_IDENTITY_SHARING,
                           GetSharingPermissionGrantsAsContentSettings(),
                           std::move(callback));
}

void FederatedIdentityAccountKeyedPermissionContext::AddToAccountList(
    base::Value::Dict& dict,
    const std::string& account_id) {
  base::Value::Dict account_dict;
  account_dict.Set(kAccountIdKey, account_id);
  account_dict.Set(kTimestampKey, base::TimeToValue(clock_->Now()));

  base::Value::List* account_list = dict.FindList(kAccountIdsKey);
  if (account_list) {
    account_list->Append(std::move(account_dict));
    return;
  }

  base::Value::List new_list;
  new_list.Append(std::move(account_dict));
  dict.Set(kAccountIdsKey, base::Value(std::move(new_list)));
}
