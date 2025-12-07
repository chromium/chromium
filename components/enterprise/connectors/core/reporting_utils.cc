// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/reporting_utils.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/url_matcher/url_util.h"
#include "net/base/network_interfaces.h"

namespace enterprise_connectors {

namespace {
// Namespace alias to reduce verbosity when using event protos.
namespace proto = ::chrome::cros::reporting::proto;

// Max url size before truncation
constexpr int kMaxUrlLength = 2048;

// Alias to reduce verbosity when using PasswordBreachEvent::TriggerType.
using TriggerType =
    ::chrome::cros::reporting::proto::PasswordBreachEvent::TriggerType;

// Alias to reduce verbosity when using
// SafeBrowsingInterstitialEvent::InterstitialReason;
using InterstitialReason = ::chrome::cros::reporting::proto::
    SafeBrowsingInterstitialEvent::InterstitialReason;

// Alias to reduce verbosity when using
//  UrlFilteringInterstitialEvent::InterstitialThreatType;
using InterstitialThreatType = ::chrome::cros::reporting::proto::
    UrlFilteringInterstitialEvent::InterstitialThreatType;

// Alias to reduce verbosity when using EventResult and to differentiate from
// the EventResult struct.
using ProtoEventResult = ::chrome::cros::reporting::proto::EventResult;

// Alias to reduce verbosity when using DangerousDownloadThreatType.
using ThreatType = ::chrome::cros::reporting::proto::
    SafeBrowsingDangerousDownloadEvent::DangerousDownloadThreatType;

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
    case EventResult::FORCED_SAVE_TO_CLOUD:
      return proto::EventResult::EVENT_RESULT_FORCED_SAVE_TO_CLOUD;
  }
}

std::string ActionFromVerdictType(
    safe_browsing::RTLookupResponse::ThreatInfo::VerdictType verdict_type) {
  switch (verdict_type) {
    case safe_browsing::RTLookupResponse::ThreatInfo::DANGEROUS:
      return "BLOCK";
    case safe_browsing::RTLookupResponse::ThreatInfo::WARN:
      return "WARN";
    case safe_browsing::RTLookupResponse::ThreatInfo::SAFE:
      return "REPORT_ONLY";
    case safe_browsing::RTLookupResponse::ThreatInfo::SUSPICIOUS:
    case safe_browsing::RTLookupResponse::ThreatInfo::VERDICT_TYPE_UNSPECIFIED:
      return "ACTION_UNKNOWN";
  }
}

proto::TriggeredRuleInfo::Action ActionProtoFromVerdictType(
    safe_browsing::RTLookupResponse::ThreatInfo::VerdictType verdict_type) {
  switch (verdict_type) {
    case safe_browsing::RTLookupResponse::ThreatInfo::DANGEROUS:
      return proto::TriggeredRuleInfo::BLOCK;
    case safe_browsing::RTLookupResponse::ThreatInfo::WARN:
      return proto::TriggeredRuleInfo::WARN;
    case safe_browsing::RTLookupResponse::ThreatInfo::SAFE:
      return proto::TriggeredRuleInfo::REPORT_ONLY;
    case safe_browsing::RTLookupResponse::ThreatInfo::SUSPICIOUS:
    case safe_browsing::RTLookupResponse::ThreatInfo::VERDICT_TYPE_UNSPECIFIED:
      return proto::TriggeredRuleInfo::ACTION_UNKNOWN;
  }
}

InterstitialThreatType ConvertThreatTypeToProto(std::string threat_type) {
  if (threat_type == kEnterpriseWarnedSeenThreatType) {
    return proto::UrlFilteringInterstitialEvent::ENTERPRISE_WARNED_SEEN;
  }
  if (threat_type == kEnterpriseWarnedBypassTheatType) {
    return proto::UrlFilteringInterstitialEvent::ENTERPRISE_WARNED_BYPASS;
  }
  if (threat_type == kEnterpriseBlockedSeenThreatType) {
    return proto::UrlFilteringInterstitialEvent::ENTERPRISE_BLOCKED_SEEN;
  }
  if (threat_type.empty()) {
    return proto::UrlFilteringInterstitialEvent::
        UNKNOWN_INTERSTITIAL_THREAT_TYPE;
  }
  NOTREACHED();
}

proto::UnscannedFileEvent::UnscannedReason ToProtoUnscannedReason(
    const std::string& unscanned_reason) {
  if (unscanned_reason == kFilePasswordProtectedUnscannedReason) {
    return proto::UnscannedFileEvent::FILE_PASSWORD_PROTECTED;
  }
  if (unscanned_reason == kFileTooLargeUnscannedReason) {
    return proto::UnscannedFileEvent::FILE_TOO_LARGE;
  }
  if (unscanned_reason == kDlpScanFailedUnscannedReason) {
    return proto::UnscannedFileEvent::DLP_SCAN_FAILED;
  }
  if (unscanned_reason == kMalwareScanFailedUnscannedReason) {
    return proto::UnscannedFileEvent::MALWARE_SCAN_FAILED;
  }
  if (unscanned_reason == kDlpScanUnsupportedFileTypeUnscannedReason) {
    return proto::UnscannedFileEvent::DLP_SCAN_UNSUPPORTED_FILE_TYPE;
  }
  if (unscanned_reason == kMalwareScanUnsupportedFileTypeUnscannedReason) {
    return proto::UnscannedFileEvent::MALWARE_SCAN_UNSUPPORTED_FILE_TYPE;
  }
  if (unscanned_reason == kServiceUnavailableUnscannedReason) {
    return proto::UnscannedFileEvent::SERVICE_UNAVAILABLE;
  }
  if (unscanned_reason == kTooManyRequestsUnscannedReason) {
    return proto::UnscannedFileEvent::TOO_MANY_REQUESTS;
  }
  if (unscanned_reason == kTimeoutUnscannedReason) {
    return proto::UnscannedFileEvent::TIMEOUT;
  }
  if (unscanned_reason.empty()) {
    return proto::UnscannedFileEvent::UNSCANNED_REASON_UNKNOWN;
  }
  NOTREACHED();
}

proto::DataTransferEventTrigger ToProtoDataTransferEventTrigger(
    const std::string& trigger) {
  if (trigger == kFileDownloadDataTransferEventTrigger) {
    return proto::DataTransferEventTrigger::FILE_DOWNLOAD;
  }
  if (trigger == kFileUploadDataTransferEventTrigger) {
    return proto::DataTransferEventTrigger::FILE_UPLOAD;
  }
  if (trigger == kWebContentUploadDataTransferEventTrigger) {
    return proto::DataTransferEventTrigger::WEB_CONTENT_UPLOAD;
  }
  if (trigger == kPagePrintDataTransferEventTrigger) {
    return proto::DataTransferEventTrigger::PAGE_PRINT;
  }
  if (trigger == kClipboardCopyDataTransferEventTrigger) {
    return proto::DataTransferEventTrigger::CLIPBOARD_COPY;
  }
  if (trigger == kUrlVisitedDataTransferEventTrigger) {
    return proto::DataTransferEventTrigger::URL_VISITED;
  }
  if (trigger == kFileTransferDataTransferEventTrigger) {
    return proto::DataTransferEventTrigger::FILE_TRANSFER;
  }
  if (trigger == kPageLoadDataTransferEventTrigger) {
    return proto::DataTransferEventTrigger::PAGE_LOAD;
  }
  if (trigger == kMutationDataTransferEventTrigger) {
    return proto::DataTransferEventTrigger::MUTATION;
  }
  if (trigger == kMouseActionDataTransferEventTrigger) {
    return proto::DataTransferEventTrigger::MOUSE_ACTION;
  }
  if (trigger.empty()) {
    return proto::DataTransferEventTrigger::
        DATA_TRANSFER_EVENT_TRIGGER_TYPE_UNSPECIFIED;
  }
  NOTREACHED();
}

proto::ContentTransferMethod ToProtoContentTransferMethod(
    const std::string& method) {
  // If `method` is empty, it means the field is not applicable and this method
  // won't be called. Only converts the string to
  // `CONTENT_TRANSFER_METHOD_UNKNOWN`, if it is explicitly set this way.
  if (method == kContentTransferMethodUnknown) {
    return proto::CONTENT_TRANSFER_METHOD_UNKNOWN;
  }
  if (method == kContentTransferMethodFilePicker) {
    return proto::CONTENT_TRANSFER_METHOD_FILE_PICKER;
  }
  if (method == kContentTransferMethodDragAndDrop) {
    return proto::CONTENT_TRANSFER_METHOD_DRAG_AND_DROP;
  }
  if (method == kContentTransferMethodFilePaste) {
    return proto::CONTENT_TRANSFER_METHOD_FILE_PASTE;
  }
  NOTREACHED();
}

ThreatType ToProtoThreatType(const std::string& threat_type) {
  if (threat_type == kDangerousDownloadThreatType) {
    return proto::SafeBrowsingDangerousDownloadEvent::DANGEROUS;
  }
  if (threat_type == kDangerousHostDownloadThreatType) {
    return proto::SafeBrowsingDangerousDownloadEvent::DANGEROUS_HOST;
  }
  if (threat_type == kPotentiallyUnwantedDownloadThreatType) {
    return proto::SafeBrowsingDangerousDownloadEvent::POTENTIALLY_UNWANTED;
  }
  if (threat_type == kUncommonDownloadThreatType) {
    return proto::SafeBrowsingDangerousDownloadEvent::UNCOMMON;
  }
  if (threat_type == kUnknownDownloadThreatType) {
    return proto::SafeBrowsingDangerousDownloadEvent::UNKNOWN;
  }
  if (threat_type == kDangerousFileTypeDownloadThreatType) {
    return proto ::SafeBrowsingDangerousDownloadEvent::DANGEROUS_FILE_TYPE;
  }
  if (threat_type == kDangerousUrlDownloadThreatType) {
    return proto ::SafeBrowsingDangerousDownloadEvent::DANGEROUS_URL;
  }
  if (threat_type == kDangerousAccountCompromiseDownloadThreatType) {
    return proto ::SafeBrowsingDangerousDownloadEvent::
        DANGEROUS_ACCOUNT_COMPROMISE;
  }
  if (threat_type.empty()) {
    return proto::SafeBrowsingDangerousDownloadEvent::
        DANGEROUS_DOWNLOAD_THREAT_TYPE_UNSPECIFIED;
  }
  NOTREACHED();
}

proto::TriggeredRuleInfo::Action ActionProtoFromTriggerRuleAction(
    const TriggeredRule::Action& action) {
  switch (action) {
    case TriggeredRule::Action::
        ContentAnalysisResponse_Result_TriggeredRule_Action_ACTION_UNSPECIFIED:
      return proto::TriggeredRuleInfo::ACTION_UNKNOWN;
    case TriggeredRule::Action::
        ContentAnalysisResponse_Result_TriggeredRule_Action_REPORT_ONLY:
      return proto::TriggeredRuleInfo::REPORT_ONLY;
    case TriggeredRule::Action::
        ContentAnalysisResponse_Result_TriggeredRule_Action_WARN:
      return proto::TriggeredRuleInfo::WARN;
    case TriggeredRule::Action::
        ContentAnalysisResponse_Result_TriggeredRule_Action_FORCE_SAVE_TO_CLOUD:
      return proto::TriggeredRuleInfo::FORCE_SAVE_TO_CLOUD;
    case TriggeredRule::Action::
        ContentAnalysisResponse_Result_TriggeredRule_Action_BLOCK:
      return proto::TriggeredRuleInfo::BLOCK;
  }
}

google::protobuf::RepeatedPtrField<proto::TriggeredRuleInfo>
GetTriggerRulesFromContentAnalysisResult(
    const ContentAnalysisResponse::Result& result) {
  google::protobuf::RepeatedPtrField<proto::TriggeredRuleInfo> triggered_rules;
  for (const TriggeredRule& trigger : result.triggered_rules()) {
    proto::TriggeredRuleInfo triggered_rule;
    triggered_rule.set_rule_name(trigger.rule_name());
    triggered_rule.set_url_category(trigger.url_category());

    int rule_id_int = 0;
    if (base::StringToInt(trigger.rule_id(), &rule_id_int)) {
      triggered_rule.set_rule_id(rule_id_int);
    }
    triggered_rule.set_action(
        ActionProtoFromTriggerRuleAction(trigger.action()));
    *triggered_rules.Add() = triggered_rule;
  }

  return triggered_rules;
}

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
google::protobuf::RepeatedPtrField<proto::TriggeredRuleInfo>
GetTriggerRulesFromDataControlsRules(
    const data_controls::Verdict::TriggeredRules& data_control_rules) {
  google::protobuf::RepeatedPtrField<proto::TriggeredRuleInfo> triggered_rules;
  for (const auto& [index, rule] : data_control_rules) {
    proto::TriggeredRuleInfo triggered_rule;
    triggered_rule.set_rule_name(rule.rule_name);

    int rule_id_int = 0;
    if (base::StringToInt(rule.rule_id, &rule_id_int)) {
      triggered_rule.set_rule_id(rule_id_int);
    }
    *triggered_rules.Add() = triggered_rule;
  }

  return triggered_rules;
}
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

void TruncateUrl(std::string* url) {
  if (url->length() > kMaxUrlLength) {
    url->resize(kMaxUrlLength);
  }
}

void TruncateUrlInfo(::chrome::cros::reporting::proto::UrlInfo* url_info) {
  TruncateUrl(url_info->mutable_url());
}

#define TRUNCATE_STRING_URL(event_ptr, field_name) \
  TruncateUrl((event_ptr)->mutable_##field_name());

#define TRUNCATE_REPEATED_STRING_URL(event_ptr, field_name)    \
  for (int i = 0; i < (event_ptr)->field_name##_size(); ++i) { \
    TruncateUrl((event_ptr)->mutable_##field_name(i));         \
  }

#define TRUNCATE_URL_INFO(event_ptr, field_name)          \
  if ((event_ptr)->has_##field_name()) {                  \
    TruncateUrlInfo((event_ptr)->mutable_##field_name()); \
  }

#define TRUNCATE_REPEATED_URL_INFO(event_ptr, field_name)      \
  for (int i = 0; i < (event_ptr)->field_name##_size(); ++i) { \
    TruncateUrlInfo((event_ptr)->mutable_##field_name(i));     \
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

::google3_protos::Timestamp ToProtoTimestamp(base::Time time) {
  int64_t millis = time.InMillisecondsFSinceUnixEpoch();
  ::google3_protos::Timestamp timestamp;
  timestamp.set_seconds(millis / 1000);
  timestamp.set_nanos((millis % 1000) * 1000000);
  return timestamp;
}

std::unique_ptr<url_matcher::URLMatcher> CreateURLMatcherForOptInEvent(
    const ReportingSettings& settings,
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

EventResult GetEventResultFromThreatType(std::string threat_type) {
  if (threat_type == kEnterpriseWarnedSeenThreatType) {
    return EventResult::WARNED;
  }
  if (threat_type == kEnterpriseWarnedBypassTheatType) {
    return EventResult::BYPASSED;
  }
  if (threat_type == kEnterpriseBlockedSeenThreatType) {
    return EventResult::BLOCKED;
  }
  if (threat_type.empty()) {
    return EventResult::ALLOWED;
  }
  NOTREACHED();
}

proto::TriggeredRuleInfo ConvertMatchedUrlNavigationRuleToTriggeredRuleInfo(
    const safe_browsing::MatchedUrlNavigationRule& navigation_rule,
    const safe_browsing::RTLookupResponse::ThreatInfo::VerdictType&
        verdict_type) {
  proto::TriggeredRuleInfo triggered_rule_info;
  triggered_rule_info.set_rule_name(navigation_rule.rule_name());
  int rule_id = 0;
  if (base::StringToInt(navigation_rule.rule_id(), &rule_id)) {
    triggered_rule_info.set_rule_id(rule_id);
  }
  triggered_rule_info.set_url_category(navigation_rule.matched_url_category());
  triggered_rule_info.set_action(ActionProtoFromVerdictType(verdict_type));
  triggered_rule_info.set_has_watermarking(
      navigation_rule.has_watermark_message());
  return triggered_rule_info;
}

void AddTriggeredRuleInfoToUrlFilteringInterstitialEvent(
    const safe_browsing::RTLookupResponse& response,
    base::Value::Dict& event) {
  base::Value::List triggered_rule_info;

  for (const safe_browsing::RTLookupResponse::ThreatInfo& threat_info :
       response.threat_info()) {
    base::Value::Dict triggered_rule;
    triggered_rule.Set(kKeyTriggeredRuleName,
                       threat_info.matched_url_navigation_rule().rule_name());
    int rule_id = 0;
    if (base::StringToInt(threat_info.matched_url_navigation_rule().rule_id(),
                          &rule_id)) {
      triggered_rule.Set(kKeyTriggeredRuleId, rule_id);
    }
    triggered_rule.Set(
        kKeyUrlCategory,
        threat_info.matched_url_navigation_rule().matched_url_category());
    triggered_rule.Set(kKeyAction,
                       ActionFromVerdictType(threat_info.verdict_type()));

    if (threat_info.matched_url_navigation_rule().has_watermark_message()) {
      triggered_rule.Set(kKeyHasWatermarking, true);
    }

    triggered_rule_info.Append(std::move(triggered_rule));
  }
  event.Set(kKeyTriggeredRuleInfo, std::move(triggered_rule_info));
}

std::optional<proto::PasswordBreachEvent> GetPasswordBreachEvent(
    const std::string& trigger,
    const std::vector<std::pair<GURL, std::u16string>>& identities,
    const ReportingSettings& settings,
    const std::string& profile_identifier,
    const std::string& profile_username) {
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
  event.set_profile_identifier(profile_identifier);
  event.set_profile_user_name(profile_username);

  return event;
}

proto::SafeBrowsingPasswordReuseEvent GetPasswordReuseEvent(
    const GURL& url,
    const std::string& user_name,
    bool is_phishing_url,
    bool warning_shown,
    const std::string& profile_identifier,
    const std::string& profile_username) {
  proto::SafeBrowsingPasswordReuseEvent event;
  event.set_url(url.spec());
  event.set_user_name(user_name);
  event.set_is_phishing_url(is_phishing_url);
  event.set_event_result(warning_shown ? proto::EVENT_RESULT_WARNED
                                       : proto::EVENT_RESULT_ALLOWED);
  event.set_profile_identifier(profile_identifier);
  event.set_profile_user_name(profile_username);

  return event;
}

proto::SafeBrowsingPasswordChangedEvent GetPasswordChangedEvent(
    const std::string& user_name,
    const std::string& profile_identifier,
    const std::string& profile_username) {
  proto::SafeBrowsingPasswordChangedEvent event;
  event.set_user_name(user_name);
  event.set_profile_identifier(profile_identifier);
  event.set_profile_user_name(profile_username);

  return event;
}

proto::LoginEvent GetLoginEvent(const GURL& url,
                                bool is_federated,
                                const url::SchemeHostPort& federated_origin,
                                const std::u16string& username,
                                const std::string& profile_identifier,
                                const std::string& profile_username) {
  proto::LoginEvent event;
  event.set_url(url.spec());
  event.set_is_federated(is_federated);
  if (is_federated) {
    event.set_federated_origin(federated_origin.Serialize());
  }
  event.set_login_user_name(MaskUsername(username));
  event.set_profile_identifier(profile_identifier);
  event.set_profile_user_name(profile_username);

  return event;
}

proto::SafeBrowsingInterstitialEvent GetInterstitialEvent(
    const GURL& url,
    const std::string& reason,
    int net_error_code,
    bool clicked_through,
    EventResult event_result,
    const std::string& profile_identifier,
    const std::string& profile_username,
    const ReferrerChain& referrer_chain) {
  proto::SafeBrowsingInterstitialEvent event;
  event.set_url(url.spec());
  event.set_reason(GetInterstitialReason(reason));
  event.set_net_error_code(net_error_code);
  event.set_clicked_through(clicked_through);
  event.set_event_result(GetEventResult(event_result));
  event.set_profile_identifier(profile_identifier);
  event.set_profile_user_name(profile_username);

  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    for (const auto& referrer : referrer_chain) {
      proto::UrlInfo url_info;
      if (referrer.ip_addresses().size() > 0) {
        url_info.set_ip(referrer.ip_addresses()[0]);
      }
      url_info.set_url(referrer.url());
      *event.add_referrers() = url_info;
    }
  }

  return event;
}

proto::UrlFilteringInterstitialEvent GetUrlFilteringInterstitialEvent(
    const GURL& url,
    const std::string& threat_type,
    const safe_browsing::RTLookupResponse& response,
    const std::string& profile_identifier,
    const std::string& profile_username,
    const std::string& active_user,
    const ReferrerChain& referrer_chain) {
  proto::UrlFilteringInterstitialEvent event;
  event.set_url(url.spec());
  EventResult event_result = GetEventResultFromThreatType(threat_type);
  event.set_clicked_through(event_result == EventResult::BYPASSED);
  if (!threat_type.empty()) {
    event.set_threat_type(ConvertThreatTypeToProto(threat_type));
  }
  event.set_event_result(GetEventResult(event_result));
  event.set_profile_identifier(profile_identifier);
  event.set_profile_user_name(profile_username);

  if (!active_user.empty()) {
    event.set_web_app_signed_in_account(active_user);
  }

  for (const safe_browsing::RTLookupResponse::ThreatInfo& threat_info :
       response.threat_info()) {
    proto::TriggeredRuleInfo triggered_rule_info =
        ConvertMatchedUrlNavigationRuleToTriggeredRuleInfo(
            threat_info.matched_url_navigation_rule(),
            threat_info.verdict_type());
    *event.add_triggered_rule_info() = triggered_rule_info;
  }

  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    for (const auto& referrer : referrer_chain) {
      proto::UrlInfo url_info;
      if (referrer.ip_addresses().size() > 0) {
        url_info.set_ip(referrer.ip_addresses()[0]);
      }
      url_info.set_url(referrer.url());
      *event.add_referrers() = url_info;
    }
  }

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

proto::UnscannedFileEvent GetUnscannedFileEvent(
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
    EventResult event_result) {
  proto::UnscannedFileEvent event;
  event.set_url(url.spec());
  event.set_tab_url(tab_url.spec());
  event.set_source(source);
  event.set_destination(destination);
  event.set_file_name(file_name);
  event.set_download_digest_sha_256(download_digest_sha256);
  event.set_content_type(mime_type);
  event.set_trigger(ToProtoDataTransferEventTrigger(trigger));
  event.set_unscanned_reason(ToProtoUnscannedReason(reason));

  if (!content_transfer_method.empty()) {
    event.set_content_transfer_method(
        ToProtoContentTransferMethod(content_transfer_method));
  }

  event.set_profile_identifier(profile_identifier);
  event.set_profile_user_name(profile_username);

  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.set_content_size(content_size);
  }

  event.set_event_result(GetEventResult(event_result));
  event.set_clicked_through(event_result == EventResult::BYPASSED);

  return event;
}

proto::DlpSensitiveDataEvent GetDlpSensitiveDataEvent(
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
    EventResult event_result) {
  proto::DlpSensitiveDataEvent event;
  event.set_url(url.spec());
  event.set_tab_url(tab_url.spec());
  event.set_source(source);
  event.set_destination(destination);
  event.set_file_name(file_name);
  event.set_download_digest_sha_256(download_digest_sha256);
  event.set_content_type(mime_type);
  event.set_trigger(ToProtoDataTransferEventTrigger(trigger));
  event.set_scan_id(scan_id);

  if (!content_transfer_method.empty()) {
    event.set_content_transfer_method(
        ToProtoContentTransferMethod(content_transfer_method));
  }

  if (!content_area_account_email.empty()) {
    event.set_web_app_signed_in_account(content_area_account_email);
  }

  if (!source_active_user_email.empty()) {
    event.set_source_web_app_signed_in_account(source_active_user_email);
  }

  event.set_profile_identifier(profile_identifier);
  event.set_profile_user_name(profile_username);

  if (user_justification.has_value()) {
    event.set_user_justification(base::UTF16ToUTF8(user_justification.value()));
  }

  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.set_content_size(content_size);
  }

  *event.mutable_triggered_rule_info() =
      GetTriggerRulesFromContentAnalysisResult(result);

  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    for (const auto& referrer : referrer_chain) {
      proto::UrlInfo url_info;
      if (referrer.ip_addresses().size() > 0) {
        url_info.set_ip(referrer.ip_addresses()[0]);
      }
      url_info.set_url(referrer.url());
      *event.add_referrers() = url_info;
    }
  }

  event.set_event_result(GetEventResult(event_result));
  event.set_clicked_through(event_result == EventResult::BYPASSED);

  *event.mutable_iframe_urls() = frame_url_chain;

  return event;
}

proto::SafeBrowsingDangerousDownloadEvent GetDangerousDownloadEvent(
    const GURL& url,
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
    EventResult event_result) {
  proto::SafeBrowsingDangerousDownloadEvent event;
  event.set_url(url.spec());
  event.set_tab_url(tab_url.spec());
  event.set_source(source);
  event.set_destination(destination);
  event.set_file_name(file_name);
  event.set_download_digest_sha256(download_digest_sha256);
  event.set_threat_type(ToProtoThreatType(threat_type));
  event.set_content_type(mime_type);
  event.set_trigger(ToProtoDataTransferEventTrigger(trigger));
  event.set_scan_id(scan_id);

  if (!content_transfer_method.empty()) {
    event.set_content_transfer_method(
        ToProtoContentTransferMethod(content_transfer_method));
  }

  event.set_profile_identifier(profile_identifier);
  event.set_profile_user_name(profile_username);

  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.set_content_size(content_size);
  }

  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    for (const auto& referrer : referrer_chain) {
      proto::UrlInfo url_info;
      if (referrer.ip_addresses().size() > 0) {
        url_info.set_ip(referrer.ip_addresses()[0]);
      }
      url_info.set_url(referrer.url());
      *event.add_referrers() = url_info;
    }
  }

  event.set_event_result(GetEventResult(event_result));
  event.set_clicked_through(event_result == EventResult::BYPASSED);

  *event.mutable_iframe_urls() = frame_url_chain;

  return event;
}

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
    EventResult event_result) {
  proto::DlpSensitiveDataEvent event;
  event.set_url(url.spec());
  event.set_tab_url(tab_url.spec());
  event.set_source(source);
  event.set_destination(destination);
  event.set_content_type(mime_type);
  event.set_trigger(ToProtoDataTransferEventTrigger(trigger));

  if (!content_area_account_email.empty()) {
    event.set_web_app_signed_in_account(content_area_account_email);
  }

  if (!source_active_user_email.empty()) {
    event.set_source_web_app_signed_in_account(source_active_user_email);
  }

  event.set_profile_identifier(profile_identifier);
  event.set_profile_user_name(profile_username);

  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.set_content_size(content_size);
  }

  *event.mutable_triggered_rule_info() =
      GetTriggerRulesFromDataControlsRules(triggered_rules);
  event.set_event_result(GetEventResult(event_result));

  return event;
}
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

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

void AddReferrerChainToEvent(
    const google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>&
        referrer_chain,
    base::Value::Dict& event) {
  base::Value::List referrers;
  for (const auto& referrer : referrer_chain) {
    if (!referrer.url().empty() || !referrer.ip_addresses().empty()) {
      base::Value::Dict referrer_dict;
      referrer_dict.Set("url", referrer.url());
      if (referrer.ip_addresses().size() > 0) {
        referrer_dict.Set("ip", referrer.ip_addresses()[0]);
      }
      referrers.Append(std::move(referrer_dict));
    }
  }
  event.Set(kKeyReferrers, std::move(referrers));
}

void AddFrameUrlChainToEvent(
    const google::protobuf::RepeatedPtrField<std::string>& frame_url_chain,
    base::Value::Dict& event) {
  base::Value::List iframe_urls;
  for (const auto& frame_url : frame_url_chain) {
    iframe_urls.Append(frame_url);
  }
  event.Set(kKeyIframeUrls, std::move(iframe_urls));
}

void MaybeTruncateLongUrls(proto::Event& event_variant) {
  switch (event_variant.event_case()) {
    case proto::Event::kPasswordReuseEvent: {
      auto* event = event_variant.mutable_password_reuse_event();
      TRUNCATE_STRING_URL(event, url);
      TRUNCATE_REPEATED_STRING_URL(event, referral_urls);
      break;
    }
    case proto::Event::kDangerousDownloadEvent: {
      auto* event = event_variant.mutable_dangerous_download_event();
      TRUNCATE_STRING_URL(event, url);
      TRUNCATE_STRING_URL(event, tab_url);
      TRUNCATE_URL_INFO(event, url_info);
      TRUNCATE_URL_INFO(event, tab_url_info);
      TRUNCATE_REPEATED_STRING_URL(event, referral_urls);
      TRUNCATE_REPEATED_URL_INFO(event, referrers);
      TRUNCATE_REPEATED_STRING_URL(event, iframe_urls);
      break;
    }
    case proto::Event::kInterstitialEvent: {
      auto* event = event_variant.mutable_interstitial_event();
      TRUNCATE_STRING_URL(event, url);
      TRUNCATE_REPEATED_STRING_URL(event, referral_urls);
      TRUNCATE_URL_INFO(event, url_info);
      TRUNCATE_REPEATED_URL_INFO(event, referrers);
      break;
    }
    case proto::Event::kSensitiveDataEvent: {
      auto* event = event_variant.mutable_sensitive_data_event();
      TRUNCATE_STRING_URL(event, url);
      TRUNCATE_STRING_URL(event, tab_url);
      TRUNCATE_URL_INFO(event, url_info);
      TRUNCATE_REPEATED_STRING_URL(event, referral_urls);
      TRUNCATE_REPEATED_URL_INFO(event, referrers);
      TRUNCATE_REPEATED_STRING_URL(event, iframe_urls);
      break;
    }
    case proto::Event::kUnscannedFileEvent: {
      auto* event = event_variant.mutable_unscanned_file_event();
      TRUNCATE_STRING_URL(event, url);
      TRUNCATE_STRING_URL(event, tab_url);
      TRUNCATE_REPEATED_STRING_URL(event, referral_urls);
      break;
    }
    case proto::Event::kLoginEvent: {
      auto* event = event_variant.mutable_login_event();
      TRUNCATE_STRING_URL(event, url);
      TRUNCATE_STRING_URL(event, federated_origin);
      break;
    }
    case proto::Event::kPasswordBreachEvent: {
      auto* event = event_variant.mutable_password_breach_event();
      for (int i = 0; i < event->identities_size(); ++i) {
        TruncateUrl(event->mutable_identities(i)->mutable_url());
      }
      break;
    }
    case proto::Event::kUrlFilteringInterstitialEvent: {
      auto* event = event_variant.mutable_url_filtering_interstitial_event();
      TRUNCATE_STRING_URL(event, url);
      TRUNCATE_URL_INFO(event, url_info);
      TRUNCATE_REPEATED_STRING_URL(event, referrer_urls);
      TRUNCATE_REPEATED_URL_INFO(event, referrers);
      break;
    }
    case proto::Event::kPasswordChangedEvent:
    case proto::Event::kPolicyValidationReportEvent:
    case proto::Event::kExtensionAppInstallEvent:
    case proto::Event::kReportingRecordEvent:
    case proto::Event::kContentTransferEvent:
    case proto::Event::kBrowserExtensionInstallEvent:
    case proto::Event::kBrowserCrashEvent:
    case proto::Event::kExtensionTelemetryEvent:
    case proto::Event::kUrlNavigationEvent:
    case proto::Event::kSuspiciousUrlEvent:
    case proto::Event::kPrototypeRawEvent:
    case proto::Event::kTelomereEvent:
    case proto::Event::EVENT_NOT_SET:
      break;
  }
}

}  // namespace enterprise_connectors
