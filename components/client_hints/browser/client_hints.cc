// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <functional>
#include <string>

#include "components/client_hints/browser/client_hints.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/client_hints/common/client_hints.h"
#include "components/client_hints/common/switches.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/embedder_support/user_agent_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace client_hints {

namespace {
base::flat_map<url::Origin, std::vector<network::mojom::WebClientHintsType>>
ParseInitializeClientHintsStroage() {
  auto results =
      base::flat_map<url::Origin,
                     std::vector<network::mojom::WebClientHintsType>>();

  std::string raw_client_hint_json =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kInitializeClientHintsStorage);

  absl::optional<base::Value> maybe_value =
      base::JSONReader::Read(raw_client_hint_json);

  if (!maybe_value || !maybe_value->is_dict()) {
    LOG(WARNING)
        << "The 'initialize-client-hints-storage' switch value could not be "
        << "properly parsed.";
    return {};
  }

  for (auto entry : maybe_value->DictItems()) {
    url::Origin origin = url::Origin::Create(GURL(entry.first));
    if (origin.opaque() || origin.scheme() != url::kHttpsScheme) {
      LOG(WARNING)
          << "The url '" << entry.first
          << "' cannot be associated to client hints and will be ignored.";
      continue;
    }

    if (!entry.second.is_string()) {
      LOG(WARNING) << "The value associated with the origin \""
                   << origin.Serialize() << "\" could not be recognized as a "
                   << "valid string and will be ignored.";
      continue;
    }

    absl::optional<std::vector<network::mojom::WebClientHintsType>>
        maybe_parsed_accept_ch =
            network::ParseClientHintsHeader(entry.second.GetString());

    if (!maybe_parsed_accept_ch) {
      LOG(WARNING) << "Could not parse the following client hint token list: "
                   << entry.second.GetString();
      continue;
    }

    results[origin] = maybe_parsed_accept_ch.value();
  }

  return results;
}

}  // namespace

ClientHints::ClientHints(
    content::BrowserContext* context,
    network::NetworkQualityTracker* network_quality_tracker,
    HostContentSettingsMap* settings_map,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    PrefService* pref_service)
    : context_(context),
      network_quality_tracker_(network_quality_tracker),
      settings_map_(settings_map),
      cookie_settings_(cookie_settings),
      pref_service_(pref_service) {
  DCHECK(context_);
  DCHECK(network_quality_tracker_);
  DCHECK(settings_map_);
  DCHECK(cookie_settings_);

  if (!context->IsOffTheRecord() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInitializeClientHintsStorage)) {
    auto command_line_hints = ParseInitializeClientHintsStroage();

    for (const auto& origin_hints_pair : command_line_hints) {
      PersistClientHints(origin_hints_pair.first, origin_hints_pair.second);
    }
  }
}

ClientHints::~ClientHints() = default;

network::NetworkQualityTracker* ClientHints::GetNetworkQualityTracker() {
  return network_quality_tracker_;
}

void ClientHints::GetAllowedClientHintsFromSource(
    const GURL& url,
    blink::EnabledClientHints* client_hints) {
  ContentSettingsForOneType client_hints_rules;
  settings_map_->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                                       &client_hints_rules);
  client_hints::GetAllowedClientHintsFromSource(url, client_hints_rules,
                                                client_hints);
  for (auto hint : additional_hints_)
    client_hints->SetIsEnabled(hint, true);
}

bool ClientHints::IsJavaScriptAllowed(const GURL& url) {
  return settings_map_->GetContentSetting(url, url,
                                          ContentSettingsType::JAVASCRIPT) !=
         CONTENT_SETTING_BLOCK;
}

bool ClientHints::AreThirdPartyCookiesBlocked(const GURL& url) {
  return settings_map_->GetContentSetting(
             url, url, ContentSettingsType::COOKIES) == CONTENT_SETTING_BLOCK ||
         cookie_settings_->ShouldBlockThirdPartyCookies();
}

blink::UserAgentMetadata ClientHints::GetUserAgentMetadata() {
  return embedder_support::GetUserAgentMetadata(pref_service_);
}

void ClientHints::PersistClientHints(
    const url::Origin& primary_origin,
    const std::vector<network::mojom::WebClientHintsType>& client_hints) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const GURL primary_url = primary_origin.GetURL();

  // TODO(tbansal): crbug.com/735518. Consider killing the renderer that sent
  // the malformed IPC.
  if (!primary_url.is_valid() ||
      !network::IsUrlPotentiallyTrustworthy(primary_url))
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

  base::Value::ListStorage client_hints_list;
  client_hints_list.reserve(client_hints.size());

  for (const auto& entry : client_hints)
    client_hints_list.push_back(base::Value(static_cast<int>(entry)));

  auto client_hints_dictionary = std::make_unique<base::DictionaryValue>();
  client_hints_dictionary->SetKey(kClientHintsSettingKey,
                                  base::Value(std::move(client_hints_list)));

  // TODO(tbansal): crbug.com/735518. Disable updates to client hints settings
  // when cookies are disabled for |primary_origin|.
  settings_map_->SetWebsiteSettingDefaultScope(
      primary_url, GURL(), ContentSettingsType::CLIENT_HINTS,
      std::move(client_hints_dictionary),
      {base::Time(), content_settings::SessionModel::UserSession});

  UMA_HISTOGRAM_EXACT_LINEAR("ClientHints.UpdateEventCount", 1, 2);
  UMA_HISTOGRAM_COUNTS_100("ClientHints.UpdateSize", client_hints.size());
}

void ClientHints::SetAdditionalClientHints(
    const std::vector<network::mojom::WebClientHintsType>& hints) {
  additional_hints_ = hints;
}

void ClientHints::ClearAdditionalClientHints() {
  additional_hints_.clear();
}

}  // namespace client_hints
