// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/browsing_data_helper.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/functional/overloaded.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/origin_trials/common/features.h"
#include "components/prefs/pref_service.h"
#include "components/site_isolation/pref_names.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace browsing_data {

namespace {

bool WebsiteSettingsFilterAdapter(
    const base::RepeatingCallback<bool(const GURL&)> predicate,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  // Ignore the default setting.
  if (primary_pattern == ContentSettingsPattern::Wildcard())
    return false;

  // Website settings only use origin-scoped patterns. The only content setting
  // this filter is used for is DURABLE_STORAGE, which also only uses
  // origin-scoped patterns. Such patterns can be directly translated to a GURL.
  GURL url(primary_pattern.ToString());
  DCHECK(url.is_valid()) << "url: '" << url.possibly_invalid_spec() << "' "
                         << "pattern: '" << primary_pattern.ToString() << "'";
  return predicate.Run(url);
}

// Callback for when cookies have been deleted. Invokes |done|.
// Receiving |cookie_manager| as a parameter so that the receive pipe is
// not deleted before the response is received.
void OnClearedCookies(
    base::OnceClosure done,
    mojo::Remote<network::mojom::CookieManager> cookie_manager,
    uint32_t num_deleted) {
  std::move(done).Run();
}

bool IsSameHost(const std::string& host, const std::string& top_frame_host) {
  return host == top_frame_host;
}

}  // namespace

bool IsWebScheme(const std::string& scheme) {
  const std::vector<std::string>& schemes = url::GetWebStorageSchemes();
  return base::Contains(schemes, scheme);
}

bool HasWebScheme(const GURL& origin) {
  return IsWebScheme(origin.scheme());
}

HostContentSettingsMap::PatternSourcePredicate CreateWebsiteSettingsFilter(
    content::BrowsingDataFilterBuilder* filter_builder) {
  return filter_builder->MatchesAllOriginsAndDomains()
             ? HostContentSettingsMap::PatternSourcePredicate()
             : base::BindRepeating(&WebsiteSettingsFilterAdapter,
                                   filter_builder->BuildUrlFilter());
}

void RemovePrerenderCacheData(
    prerender::NoStatePrefetchManager* no_state_prefetch_manager) {
  // The NoStatePrefetchManager may have a page actively being prerendered,
  // which is essentially a preemptively cached page.
  if (no_state_prefetch_manager) {
    no_state_prefetch_manager->ClearData(
        prerender::NoStatePrefetchManager::CLEAR_PRERENDER_CONTENTS);
  }
}

void RemoveSiteIsolationData(PrefService* prefs) {
  prefs->ClearPref(site_isolation::prefs::kUserTriggeredIsolatedOrigins);
  prefs->ClearPref(site_isolation::prefs::kWebTriggeredIsolatedOrigins);
  // Note that this does not clear these sites from the in-memory map in
  // ChildProcessSecurityPolicy, since that is not supported at runtime. That
  // list of isolated sites is not directly exposed to users, though, and
  // will be cleared on next restart.
}

void RemoveEmbedderCookieData(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    content::BrowsingDataFilterBuilder* filter_builder,
    HostContentSettingsMap* host_content_settings_map,
    network::mojom::NetworkContext* safe_browsing_context,
    base::OnceCallback<base::OnceCallback<void()>()> callback_factory) {
  auto website_settings_filter = CreateWebsiteSettingsFilter(filter_builder);

  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::CLIENT_HINTS, base::Time(), base::Time::Max(),
      website_settings_filter);

  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::REDUCED_ACCEPT_LANGUAGE, base::Time(),
      base::Time::Max(), website_settings_filter);

  // Clear the safebrowsing cookies only if time period is for "all time".  It
  // doesn't make sense to apply the time period of deleting in the last X
  // hours/days to the safebrowsing cookies since they aren't the result of
  // any user action.
  if (delete_begin == base::Time() && delete_end == base::Time::Max() &&
      safe_browsing_context) {
    mojo::Remote<network::mojom::CookieManager> cookie_manager;
    safe_browsing_context->GetCookieManager(
        cookie_manager.BindNewPipeAndPassReceiver());

    network::mojom::CookieManager* manager_ptr = cookie_manager.get();

    network::mojom::CookieDeletionFilterPtr deletion_filter =
        filter_builder->BuildCookieDeletionFilter();
    if (!delete_begin.is_null())
      deletion_filter->created_after_time = delete_begin;
    if (!delete_end.is_null())
      deletion_filter->created_before_time = delete_end;

    manager_ptr->DeleteCookies(
        std::move(deletion_filter),
        base::BindOnce(&OnClearedCookies, std::move(callback_factory).Run(),
                       std::move(cookie_manager)));
  }
}

void RemoveSiteSettingsData(const base::Time& delete_begin,
                            const base::Time& delete_end,
                            HostContentSettingsMap* host_content_settings_map) {
  const auto* registry =
      content_settings::ContentSettingsRegistry::GetInstance();
  for (const content_settings::ContentSettingsInfo* info : *registry) {
    host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
        info->website_settings_info()->type(), delete_begin, delete_end,
        HostContentSettingsMap::PatternSourcePredicate());
  }

  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::USB_CHOOSER_DATA, delete_begin, delete_end,
      HostContentSettingsMap::PatternSourcePredicate());

  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::BLUETOOTH_CHOOSER_DATA, delete_begin, delete_end,
      HostContentSettingsMap::PatternSourcePredicate());

  RemoveFederatedSiteSettingsData(
      delete_begin, delete_end,
      HostContentSettingsMap::PatternSourcePredicate(),
      host_content_settings_map);

#if !BUILDFLAG(IS_ANDROID)
  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::SERIAL_CHOOSER_DATA, delete_begin, delete_end,
      HostContentSettingsMap::PatternSourcePredicate());

  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::HID_CHOOSER_DATA, delete_begin, delete_end,
      HostContentSettingsMap::PatternSourcePredicate());

  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::FILE_SYSTEM_ACCESS_CHOOSER_DATA, delete_begin,
      delete_end, HostContentSettingsMap::PatternSourcePredicate());

  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION, delete_begin,
      delete_end, HostContentSettingsMap::PatternSourcePredicate());
#endif

#if BUILDFLAG(IS_CHROMEOS)
  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::SMART_CARD_DATA, delete_begin, delete_end,
      HostContentSettingsMap::PatternSourcePredicate());
#endif
}

void RemoveFederatedSiteSettingsData(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    HostContentSettingsMap::PatternSourcePredicate pattern_predicate,
    HostContentSettingsMap* host_content_settings_map) {
  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::FEDERATED_IDENTITY_API, delete_begin, delete_end,
      pattern_predicate);

  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::FEDERATED_IDENTITY_IDENTITY_PROVIDER_SIGNIN_STATUS,
      delete_begin, delete_end, pattern_predicate);

  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::FEDERATED_IDENTITY_SHARING, delete_begin, delete_end,
      pattern_predicate);

  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION,
      delete_begin, delete_end, pattern_predicate);

  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::FEDERATED_IDENTITY_IDENTITY_PROVIDER_REGISTRATION,
      delete_begin, delete_end, pattern_predicate);
}

int GetUniqueHostCount(const BrowsingDataModel& browsing_data_model) {
  std::set<BrowsingDataModel::DataOwner> unique_hosts;
  for (auto entry : browsing_data_model) {
    unique_hosts.insert(*entry.data_owner);
  }

  return unique_hosts.size();
}

int GetUniqueThirdPartyCookiesHostCount(
    const GURL& top_frame_url,
    const BrowsingDataModel& browsing_data_model) {
  std::string top_frame_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          top_frame_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  std::set<BrowsingDataModel::DataOwner> unique_hosts;
  for (auto entry : browsing_data_model) {
    std::string host = BrowsingDataModel::GetHost(entry.data_owner.get());
    if (entry.data_details->blocked_third_party ||
        (top_frame_domain.empty() && !IsSameHost(host, top_frame_url.host())) ||
        (!top_frame_domain.empty() && !url::DomainIs(host, top_frame_domain))) {
      for (auto storage_type : entry.data_details->storage_types) {
        if (browsing_data_model.IsBlockedByThirdPartyCookieBlocking(
                entry.data_key.get(), storage_type)) {
          unique_hosts.insert(*entry.data_owner);
          break;
        }
      }
    }
  }

  return unique_hosts.size();
}

}  // namespace browsing_data
