// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <functional>
#include <string>

#include "components/client_hints/browser/client_hints.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/client_hints/common/client_hints.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/loader/network_utils.h"

namespace client_hints {

ClientHints::ClientHints(
    content::BrowserContext* context,
    network::NetworkQualityTracker* network_quality_tracker,
    HostContentSettingsMap* settings_map,
    const blink::UserAgentMetadata& user_agent_metadata,
    PrefService* pref_service)
    : context_(context),
      network_quality_tracker_(network_quality_tracker),
      settings_map_(settings_map),
      user_agent_metadata_(user_agent_metadata),
      pref_service_(pref_service) {
  DCHECK(context_);
  DCHECK(network_quality_tracker_);
  DCHECK(settings_map_);
}

ClientHints::~ClientHints() = default;

network::NetworkQualityTracker* ClientHints::GetNetworkQualityTracker() {
  return network_quality_tracker_;
}

void ClientHints::GetAllowedClientHintsFromSource(
    const GURL& url,
    blink::WebEnabledClientHints* client_hints) {
  ContentSettingsForOneType client_hints_rules;
  settings_map_->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                                       std::string(), &client_hints_rules);
  client_hints::GetAllowedClientHintsFromSource(url, client_hints_rules,
                                                client_hints);
}

bool ClientHints::IsJavaScriptAllowed(const GURL& url) {
  return settings_map_->GetContentSetting(
             url, url, ContentSettingsType::JAVASCRIPT, std::string()) !=
         CONTENT_SETTING_BLOCK;
}

bool ClientHints::UserAgentClientHintEnabled() {
  // TODO(crbug.com/1097591): This extra path check is only here because the
  // pref is not being registered in //weblayer.
  bool pref_enabled = true;
  if (pref_service_->HasPrefPath(
          policy::policy_prefs::kUserAgentClientHintsEnabled)) {
    pref_enabled = pref_service_->GetBoolean(
        policy::policy_prefs::kUserAgentClientHintsEnabled);
  }
  return pref_enabled &&
         base::FeatureList::IsEnabled(features::kUserAgentClientHint);
}

blink::UserAgentMetadata ClientHints::GetUserAgentMetadata() {
  return user_agent_metadata_;
}

void ClientHints::PersistClientHints(
    const url::Origin& primary_origin,
    const std::vector<network::mojom::WebClientHintsType>& client_hints,
    base::TimeDelta expiration_duration) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const GURL primary_url = primary_origin.GetURL();

  // TODO(tbansal): crbug.com/735518. Consider killing the renderer that sent
  // the malformed IPC.
  if (!primary_url.is_valid() ||
      !blink::network_utils::IsOriginSecure(primary_url))
    return;

  if (!IsJavaScriptAllowed(primary_url))
    return;

  DCHECK_LE(
      client_hints.size(),
      static_cast<size_t>(network::mojom::WebClientHintsType::kMaxValue) + 1);

  if (client_hints.size() >
      (static_cast<size_t>(network::mojom::WebClientHintsType::kMaxValue) +
       1)) {
    // Return early if the list does not have the right number of values.
    // Persisting wrong number of values to the disk may cause errors when
    // reading them back in the future.
    return;
  }

  if (expiration_duration <= base::TimeDelta::FromSeconds(0))
    return;

  std::unique_ptr<base::ListValue> expiration_times_list =
      std::make_unique<base::ListValue>();
  expiration_times_list->Reserve(client_hints.size());

  // Use wall clock since the expiration time would be persisted across embedder
  // restarts.
  double expiration_time =
      (base::Time::Now() + expiration_duration).ToDoubleT();

  for (const auto& entry : client_hints)
    expiration_times_list->AppendInteger(static_cast<int>(entry));

  auto expiration_times_dictionary = std::make_unique<base::DictionaryValue>();
  expiration_times_dictionary->SetList("client_hints",
                                       std::move(expiration_times_list));
  expiration_times_dictionary->SetDouble("expiration_time", expiration_time);

  // TODO(tbansal): crbug.com/735518. Disable updates to client hints settings
  // when cookies are disabled for |primary_origin|.
  settings_map_->SetWebsiteSettingDefaultScope(
      primary_url, GURL(), ContentSettingsType::CLIENT_HINTS, std::string(),
      std::move(expiration_times_dictionary),
      {base::Time(), content_settings::SessionModel::UserSession});

  UMA_HISTOGRAM_EXACT_LINEAR("ClientHints.UpdateEventCount", 1, 2);
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "ClientHints.PersistDuration", expiration_duration,
      base::TimeDelta::FromSeconds(1),
      // TODO(crbug.com/949034): Rename and fix this histogram to have some
      // intended max value. We throw away the 32 most-significant bits of the
      // 64-bit time delta in milliseconds. Before it happened silently in
      // histogram.cc, now it is explicit here. The previous value of 365 days
      // effectively turns into roughly 17 days when getting cast to int.
      base::TimeDelta::FromMilliseconds(
          static_cast<int>(base::TimeDelta::FromDays(365).InMilliseconds())),
      100);

  UMA_HISTOGRAM_COUNTS_100("ClientHints.UpdateSize", client_hints.size());
}

}  // namespace client_hints
