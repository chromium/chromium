// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines methods relevant to all code that wants to work with browsing data.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_HELPER_H_

#include <string>

#include "base/functional/callback.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"

class GURL;
class PrefService;

namespace content {
class BrowsingDataFilterBuilder;
}

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace prerender {
class NoStatePrefetchManager;
}

namespace browsing_data {

// TODO(crbug.com/40495069): DEPRECATED. Remove these functions.
// The primary functionality of testing origin type masks has moved to
// Remover. The secondary functionality of recognizing web schemes
// storing browsing data has moved to url::GetWebStorageSchemes();
// or alternatively, it can also be retrieved from Remover by
// testing the ORIGIN_TYPE_UNPROTECTED_ORIGIN | ORIGIN_TYPE_PROTECTED_ORIGIN
// origin mask. This class now merely forwards the functionality to several
// helper classes in the browsing_data codebase.

// Returns true iff the provided scheme is (really) web safe, and suitable
// for treatment as "browsing data". This relies on the definition of web safe
// in ChildProcessSecurityPolicy, but excluding schemes like
// `chrome-extension`.
bool IsWebScheme(const std::string& scheme);
bool HasWebScheme(const GURL& origin);

// Creates a filter for website settings in a HostContentSettingsMap.
HostContentSettingsMap::PatternSourcePredicate CreateWebsiteSettingsFilter(
    content::BrowsingDataFilterBuilder* filter_builder);

// Clears prerendering cache.
void RemovePrerenderCacheData(
    prerender::NoStatePrefetchManager* no_state_prefetch_manager);

// Removes site isolation prefs. Should be called when the user removes
// cookies and other site settings or history.
void RemoveSiteIsolationData(PrefService* prefs);

// Removes data that should be cleared at the same time as cookies, but which
// content/ doesn't know about. e.g. client hints and safe browsing cookie.
// If |safe_browsing_context| is not null and the function decides to clear
// cookies in it then |callback_factory| will be called synchronously to create
// a callback that is run on completion.
void RemoveEmbedderCookieData(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    content::BrowsingDataFilterBuilder* filter_builder,
    HostContentSettingsMap* host_content_settings_map,
    network::mojom::NetworkContext* safe_browsing_context,
    base::OnceCallback<base::OnceCallback<void()>()> callback_factory);

// Removes site settings data.
void RemoveSiteSettingsData(const base::Time& delete_begin,
                            const base::Time& delete_end,
                            HostContentSettingsMap* host_content_settings_map);

// Remove site settings data related to federated sign in.
// This clears:
// - Consent for identity provider to share identity information with
//   relying party.
// - Permission for relying party to silently obtain id token from identity
//   provider via the FedCM JavaScript API.
// - The FedCM auto-sign-in permission.
// - The FedCM front channel logout permission.
void RemoveFederatedSiteSettingsData(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    HostContentSettingsMap::PatternSourcePredicate pattern_predicate,
    HostContentSettingsMap* host_content_settings_map);

int GetUniqueHostCount(const BrowsingDataModel& browsing_data_model);

int GetUniqueThirdPartyCookiesHostCount(
    const GURL& first_party_url,
    const BrowsingDataModel& browsing_data_model);

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_HELPER_H_
