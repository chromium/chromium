// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/client_hints/browser/client_hints.h"

#include <cmath>
#include <functional>
#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "components/client_hints/common/client_hints.h"
#include "components/client_hints/common/switches.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/embedder_support/user_agent_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"
#include "third_party/blink/public/common/features.h"

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

  std::optional<base::Value> maybe_value =
      base::JSONReader::Read(raw_client_hint_json);

  if (!maybe_value || !maybe_value->is_dict()) {
    LOG(WARNING)
        << "The 'initialize-client-hints-storage' switch value could not be "
        << "properly parsed.";
    return {};
  }

  for (auto entry : maybe_value->GetDict()) {
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

    std::optional<std::vector<network::mojom::WebClientHintsType>>
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
      PersistClientHints(origin_hints_pair.first, nullptr,
                         origin_hints_pair.second);
    }
  }
}

ClientHints::~ClientHints() = default;

network::NetworkQualityTracker* ClientHints::GetNetworkQualityTracker() {
  return network_quality_tracker_;
}

void ClientHints::GetAllowedClientHintsFromSource(
    const url::Origin& origin,
    blink::EnabledClientHints* client_hints) {
  if (network::features::ShouldBlockAcceptClientHintsFor(origin)) {
    return;
  }
  const GURL& url = origin.GetURL();
  if (!network::IsUrlPotentiallyTrustworthy(url)) {
    return;
  }

  client_hints::GetAllowedClientHints(
      settings_map_->GetWebsiteSetting(
          url, GURL(), ContentSettingsType::CLIENT_HINTS, nullptr),
      client_hints);

  for (auto hint : additional_hints_) {
    client_hints->SetIsEnabled(hint, true);
  }
}

bool ClientHints::IsJavaScriptAllowed(const GURL& url,
                                      content::RenderFrameHost* parent_rfh) {
  return settings_map_->GetContentSetting(
             parent_rfh ? parent_rfh->GetOutermostMainFrame()
                              ->GetLastCommittedOrigin()
                              .GetURL()
                        : url,
             url, ContentSettingsType::JAVASCRIPT) != CONTENT_SETTING_BLOCK;
}

bool ClientHints::AreThirdPartyCookiesBlocked(const GURL& url,
                                              content::RenderFrameHost* rfh) {
  return !cookie_settings_->IsThirdPartyAccessAllowed(url);
}

blink::UserAgentMetadata ClientHints::GetUserAgentMetadata() {
  return embedder_support::GetUserAgentMetadata(pref_service_);
}

void ClientHints::PersistClientHints(
    const url::Origin& primary_origin,
    content::RenderFrameHost* parent_rfh,
    const std::vector<network::mojom::WebClientHintsType>& client_hints) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const GURL primary_url = primary_origin.GetURL();

  // TODO(tbansal): crbug.com/735518. Consider killing the renderer that sent
  // the malformed IPC.
  if (!primary_url.is_valid() ||
      !network::IsUrlPotentiallyTrustworthy(primary_url)) {
    return;
  }

  if (!IsJavaScriptAllowed(primary_url, parent_rfh)) {
    return;
  }

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

  const auto& persistence_started = base::TimeTicks::Now();
  base::Value::List client_hints_list;
  client_hints_list.reserve(client_hints.size());

  for (const auto& entry : client_hints) {
    client_hints_list.Append(static_cast<int>(entry));
  }

  base::Value::Dict client_hints_dictionary;
  client_hints_dictionary.Set(kClientHintsSettingKey,
                              std::move(client_hints_list));

  // TODO(tbansal): crbug.com/735518. Disable updates to client hints settings
  // when cookies are disabled for |primary_origin|.
  content_settings::ContentSettingConstraints constraints;
  constraints.set_session_model(content_settings::mojom::SessionModel::DURABLE);
  settings_map_->SetWebsiteSettingDefaultScope(
      primary_url, GURL(), ContentSettingsType::CLIENT_HINTS,
      base::Value(std::move(client_hints_dictionary)), constraints);
  network::LogClientHintsPersistenceMetrics(persistence_started,
                                            client_hints.size());
}

void ClientHints::SetAdditionalClientHints(
    const std::vector<network::mojom::WebClientHintsType>& hints) {
  additional_hints_ = hints;
}

void ClientHints::ClearAdditionalClientHints() {
  additional_hints_.clear();
}

void ClientHints::SetMostRecentMainFrameViewportSize(
    const gfx::Size& viewport_size) {
  viewport_size_ = viewport_size;
}

gfx::Size ClientHints::GetMostRecentMainFrameViewportSize() {
  return viewport_size_;
}

void ClientHints::ForceEmptyViewportSizeForTesting(
    bool should_force_empty_viewport_size) {
  should_force_empty_viewport_size_ = should_force_empty_viewport_size;
}

bool ClientHints::ShouldForceEmptyViewportSize() {
  return should_force_empty_viewport_size_;
}

}  // namespace client_hints
