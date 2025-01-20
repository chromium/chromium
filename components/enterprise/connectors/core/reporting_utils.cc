// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/reporting_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"

namespace enterprise_connectors {

namespace {
// Namespace alias to reduce verbosity when using event protos.
namespace proto = ::chrome::cros::reporting::proto;

// Alias to reduce verbosity when using PasswordBreachEvent::TriggerType.
using TriggerType =
    ::chrome::cros::reporting::proto::PasswordBreachEvent::TriggerType;

const char kMaskedUsername[] = "*****";

// Do a best-effort masking of `username`. If it's an email address (such as
// foo@example.com), everything before @ should be masked. Otherwise, the entire
// username should be masked.
std::string MaskUsername(const std::u16string& username) {
  size_t pos = username.find(u"@");
  if (pos == std::string::npos) {
    return kMaskedUsername;
  }

  return base::StrCat(
      {kMaskedUsername, base::UTF16ToUTF8(username.substr(pos))});
}

TriggerType GetTriggerType(const std::string& trigger) {
  TriggerType type;
  if (proto::PasswordBreachEvent::TriggerType_Parse(trigger, &type)) {
    return type;
  }
  return proto::PasswordBreachEvent::TRIGGER_TYPE_UNSPECIFIED;
}

// Create a URLMatcher representing the filters in
// `settings.enabled_opt_in_events` for `event_type`. This field of the
// reporting settings connector contains a map where keys are event types and
// values are lists of URL patterns specifying on which URLs the events are
// allowed to be reported. An event is generated iff its event type is present
// in the opt-in events field and the URL it relates to matches at least one of
// the event type's filters.
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

bool IsOptInEventEnabled(url_matcher::URLMatcher* matcher, const GURL& url) {
  return matcher && !matcher->MatchURL(url).empty();
}

}  // namespace

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
    if (!IsOptInEventEnabled(matcher.get(), i.first)) {
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

chrome::cros::reporting::proto::SafeBrowsingPasswordChangedEvent
GetPasswordChangedEvent(const std::string& user_name) {
  proto::SafeBrowsingPasswordChangedEvent event;
  event.set_user_name(user_name);

  return event;
}

}  // namespace enterprise_connectors
