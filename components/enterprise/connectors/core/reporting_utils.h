// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_UTILS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_UTILS_H_

#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/url_matcher/url_matcher.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
#include "components/enterprise/data_controls/core/browser/verdict.h"
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

namespace base {
class Time;
}

namespace enterprise_connectors {

// The maximum number of referrers to include in the referrer chain.
inline constexpr int kReferrerUserGestureLimit = 5;

using ReferrerChain =
    google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>;

using FrameUrlChain = google::protobuf::RepeatedPtrField<std::string>;

// Helper functions that compiles information into event protos. The
// logic is shared across platforms to ensure event consistency.
//
// Do a best-effort masking of `username`. If it's an email address (such as
// foo@example.com), everything before @ should be masked. Otherwise, the entire
// username should be masked.
std::string MaskUsername(const std::u16string& username);

// Convert base::Time to Timestamp proto.
::google3_protos::Timestamp ToProtoTimestamp(base::Time);

// Verify if the given `matcher` matches the `url`.
bool IsUrlMatched(url_matcher::URLMatcher* matcher, const GURL& url);

// Map `threat_type` to `EventResult`.
EventResult GetEventResultFromThreatType(std::string threat_type);

// Extract triggered rules from `response` and add them to the url filtering
// events.
void AddTriggeredRuleInfoToUrlFilteringInterstitialEvent(
    const safe_browsing::RTLookupResponse& response,
    base::Value::Dict& event);

// Create a URLMatcher representing the filters in
// `settings.enabled_opt_in_events` for `event_type`. This field of the
// reporting settings connector contains a map where keys are event types and
// values are lists of URL patterns specifying on which URLs the events are
// allowed to be reported. An event is generated iff its event type is present
// in the opt-in events field and the URL it relates to matches at least one of
// the event type's filters.
std::unique_ptr<url_matcher::URLMatcher> CreateURLMatcherForOptInEvent(
    const ReportingSettings& settings,
    const char* event_type);

// PasswordBreachEvent could be empty if none of the `identities` matched a
// pattern in the URL filters.
std::optional<chrome::cros::reporting::proto::PasswordBreachEvent>
GetPasswordBreachEvent(
    const std::string& trigger,
    const std::vector<std::pair<GURL, std::u16string>>& identities,
    const ReportingSettings& settings,
    const std::string& profile_identifier,
    const std::string& profile_username);

chrome::cros::reporting::proto::SafeBrowsingPasswordReuseEvent
GetPasswordReuseEvent(const GURL& url,
                      const std::string& user_name,
                      bool is_phishing_url,
                      bool warning_shown,
                      const std::string& profile_identifier,
                      const std::string& profile_username);

chrome::cros::reporting::proto::SafeBrowsingPasswordChangedEvent
GetPasswordChangedEvent(const std::string& user_name,
                        const std::string& profile_identifier,
                        const std::string& profile_username);

chrome::cros::reporting::proto::LoginEvent GetLoginEvent(
    const GURL& url,
    bool is_federated,
    const url::SchemeHostPort& federated_origin,
    const std::u16string& username,
    const std::string& profile_identifier,
    const std::string& profile_username);

chrome::cros::reporting::proto::SafeBrowsingInterstitialEvent
GetInterstitialEvent(const GURL& url,
                     const std::string& reason,
                     int net_error_code,
                     bool clicked_through,
                     EventResult event_result,
                     const std::string& profile_identifier,
                     const std::string& profile_username,
                     const ReferrerChain& referrer_chain);

chrome::cros::reporting::proto::UrlFilteringInterstitialEvent
GetUrlFilteringInterstitialEvent(
    const GURL& url,
    const std::string& threat_type,
    const safe_browsing::RTLookupResponse& response,
    const std::string& profile_identifier,
    const std::string& profile_username,
    const std::string& active_user,
    const ReferrerChain& referrer_chain);

chrome::cros::reporting::proto::UnscannedFileEvent GetUnscannedFileEvent(
    const GURL& url,
    const GURL& tab_url,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& reason,
    const std::string& content_transfer_method,
    const std::string& profile_identifier,
    const std::string& profile_username,
    const int64_t content_size,
    EventResult event_result);

chrome::cros::reporting::proto::DlpSensitiveDataEvent GetDlpSensitiveDataEvent(
    const GURL& url,
    const GURL& tab_url,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& scan_id,
    const std::string& content_transfer_method,
    const std::string& source_active_user_email,
    const std::string& content_area_account_email,
    const std::string& profile_identifier,
    const std::string& profile_username,
    std::optional<std::u16string> user_justification,
    const int64_t content_size,
    const ContentAnalysisResponse::Result& result,
    const ReferrerChain& referrer_chain,
    const FrameUrlChain& frame_url_chain,
    EventResult event_result);

chrome::cros::reporting::proto::SafeBrowsingDangerousDownloadEvent
GetDangerousDownloadEvent(const GURL& url,
                          const GURL& tab_url,
                          const std::string& source,
                          const std::string& destination,
                          const std::string& file_name,
                          const std::string& download_digest_sha256,
                          const std::string& threat_type,
                          const std::string& mime_type,
                          const std::string& trigger,
                          const std::string& scan_id,
                          const std::string& content_transfer_method,
                          const std::string& profile_identifier,
                          const std::string& profile_username,
                          const int64_t content_size,
                          const ReferrerChain& referrer_chain,
                          const FrameUrlChain& frame_url_chain,
                          EventResult event_result);

chrome::cros::reporting::proto::BrowserCrashEvent GetBrowserCrashEvent(
    const std::string& channel,
    const std::string& version,
    const std::string& report_id,
    const std::string& platform);

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
chrome::cros::reporting::proto::DlpSensitiveDataEvent
GetDataControlsSensitiveDataEvent(
    const GURL& url,
    const GURL& tab_url,
    const std::string& source,
    const std::string& destination,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& source_active_user_email,
    const std::string& content_area_account_email,
    const std::string& profile_identifier,
    const std::string& profile_username,
    int64_t content_size,
    const data_controls::Verdict::TriggeredRules& triggered_rules,
    EventResult event_result);
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

// Returns a list of the local IPv4 and IPv6 addresses of the device.
std::vector<std::string> GetLocalIpAddresses();

void AddReferrerChainToEvent(const ReferrerChain& referrer_chain,
                             base::Value::Dict& event);

void AddFrameUrlChainToEvent(
    const google::protobuf::RepeatedPtrField<std::string>& frame_url_chain,
    base::Value::Dict& event);

void MaybeTruncateLongUrls(
    ::chrome::cros::reporting::proto::Event& event_variant);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_UTILS_H_
