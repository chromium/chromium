// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/reporting_utils.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/url_matcher/url_util.h"
#include "net/base/network_interfaces.h"

namespace enterprise_connectors {

namespace {
// Namespace alias to reduce verbosity when using event protos.
namespace proto = ::chrome::cros::reporting::proto;

// Alias to reduce verbosity when using PasswordBreachEvent::TriggerType.
using TriggerType =
    ::chrome::cros::reporting::proto::PasswordBreachEvent::TriggerType;

// Alias to reduce verbosity when using
// SafeBrowsingInterstitialEvent::InterstitialReason;
using InterstitialReason = ::chrome::cros::reporting::proto::
    SafeBrowsingInterstitialEvent::InterstitialReason;

// Alias to reduce verbosity when using EventResult and to differentiate from
// the EventResult struct.
using ProtoEventResult = ::chrome::cros::reporting::proto::EventResult;

const char kMaskedUsername[] = "*****";

TriggerType GetTriggerType(const std::string& trigger) {
  TriggerType type;
  if (proto::PasswordBreachEvent::TriggerType_Parse(trigger, &type)) {
    return type;
  }
  return proto::PasswordBreachEvent::TRIGGER_TYPE_UNSPECIFIED;
}

InterstitialReason GetInterstitialReason(const std::string& reason) {
  InterstitialReason interstitial_reason;
  if (proto::SafeBrowsingInterstitialEvent::InterstitialReason_Parse(
          reason, &interstitial_reason)) {
    return interstitial_reason;
  }
  return proto::SafeBrowsingInterstitialEvent::THREAT_TYPE_UNSPECIFIED;
}

ProtoEventResult GetEventResult(EventResult event_result) {
  switch (event_result) {
    case EventResult::UNKNOWN:
      return proto::EventResult::EVENT_RESULT_UNSPECIFIED;
    case EventResult::ALLOWED:
      return proto::EventResult::EVENT_RESULT_ALLOWED;
    case EventResult::WARNED:
      return proto::EVENT_RESULT_WARNED;
    case EventResult::BLOCKED:
      return proto::EventResult::EVENT_RESULT_BLOCKED;
    case EventResult::BYPASSED:
      return proto::EventResult::EVENT_RESULT_BYPASSED;
  }
}

}  // namespace

std::string MaskUsername(const std::u16string& username) {
  size_t pos = username.find(u"@");
  if (pos == std::string::npos) {
    return kMaskedUsername;
  }

  return base::StrCat(
      {kMaskedUsername, base::UTF16ToUTF8(username.substr(pos))});
}

std::unique_ptr<url_matcher::URLMatcher> CreateURLMatcherForOptInEvent(
    const enterprise_connectors::ReportingSettings& settings,
    const char* event_type) {
  const auto& it = settings.enabled_opt_in_events.find(event_type);
  if (it == settings.enabled_opt_in_events.end()) {
    return nullptr;
  }

  std::unique_ptr<url_matcher::URLMatcher> matcher =
      std::make_unique<url_matcher::URLMatcher>();
  base::MatcherStringPattern::ID unused_id(0);
  url_matcher::util::AddFiltersWithLimit(matcher.get(), true, &unused_id,
                                         it->second);

  return matcher;
}

bool IsUrlMatched(url_matcher::URLMatcher* matcher, const GURL& url) {
  return matcher && !matcher->MatchURL(url).empty();
}

std::optional<proto::PasswordBreachEvent> GetPasswordBreachEvent(
    const std::string& trigger,
    const std::vector<std::pair<GURL, std::u16string>>& identities,
    const enterprise_connectors::ReportingSettings& settings) {
  std::unique_ptr<url_matcher::URLMatcher> matcher =
      CreateURLMatcherForOptInEvent(settings, kKeyPasswordBreachEvent);
  if (!matcher) {
    return std::nullopt;
  }

  proto::PasswordBreachEvent event;
  std::vector<proto::PasswordBreachEvent::Identity> converted_identities;
  for (const std::pair<GURL, std::u16string>& i : identities) {
    if (!IsUrlMatched(matcher.get(), i.first)) {
      continue;
    }
    proto::PasswordBreachEvent::Identity identity;
    identity.set_url(i.first.spec());
    identity.set_username(MaskUsername(i.second));
    converted_identities.push_back(identity);
  }
  if (converted_identities.empty()) {
    // Don't send an empty event if none of the breached identities matched a
    // pattern in the URL filters.
    return std::nullopt;
  } else {
    event.mutable_identities()->Add(converted_identities.begin(),
                                    converted_identities.end());
  }
  event.set_trigger(GetTriggerType(trigger));

  return event;
}

proto::SafeBrowsingPasswordReuseEvent GetPasswordReuseEvent(
    const GURL& url,
    const std::string& user_name,
    bool is_phishing_url,
    bool warning_shown) {
  proto::SafeBrowsingPasswordReuseEvent event;
  event.set_url(url.spec());
  event.set_user_name(user_name);
  event.set_is_phishing_url(is_phishing_url);
  event.set_event_result(warning_shown ? proto::EVENT_RESULT_WARNED
                                       : proto::EVENT_RESULT_ALLOWED);

  return event;
}

proto::SafeBrowsingPasswordChangedEvent GetPasswordChangedEvent(
    const std::string& user_name) {
  proto::SafeBrowsingPasswordChangedEvent event;
  event.set_user_name(user_name);

  return event;
}

proto::LoginEvent GetLoginEvent(const GURL& url,
                                bool is_federated,
                                const url::SchemeHostPort& federated_origin,
                                const std::u16string& username) {
  proto::LoginEvent event;
  event.set_url(url.spec());
  event.set_is_federated(is_federated);
  if (is_federated) {
    event.set_federated_origin(federated_origin.Serialize());
  }
  event.set_login_user_name(MaskUsername(username));

  return event;
}

proto::SafeBrowsingInterstitialEvent GetInterstitialEvent(
    const GURL& url,
    const std::string& reason,
    int net_error_code,
    bool clicked_through,
    EventResult event_result) {
  proto::SafeBrowsingInterstitialEvent event;
  event.set_url(url.spec());
  event.set_reason(GetInterstitialReason(reason));
  event.set_net_error_code(net_error_code);
  event.set_clicked_through(clicked_through);
  event.set_event_result(GetEventResult(event_result));

  return event;
}

proto::BrowserCrashEvent GetBrowserCrashEvent(const std::string& channel,
                                              const std::string& version,
                                              const std::string& report_id,
                                              const std::string& platform) {
  proto::BrowserCrashEvent event;
  event.set_channel(channel);
  event.set_version(version);
  event.set_report_id(report_id);
  event.set_platform(platform);

  return event;
}

std::vector<std::string> GetLocalIpAddresses() {
  net::NetworkInterfaceList list;
  std::vector<std::string> ip_addresses;
  if (!net::GetNetworkList(&list, net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES)) {
    LOG(ERROR) << "GetNetworkList failed";
    return ip_addresses;
  }
  for (const auto& network_interface : list) {
    ip_addresses.push_back(network_interface.address.ToString());
  }
  return ip_addresses;
}

}  // namespace enterprise_connectors
