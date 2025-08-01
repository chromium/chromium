// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/reporting_event_router.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/common/proto/synced_from_google3/chrome_reporting_entity.pb.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/realtime_reporting_client_base.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/url_matcher/url_matcher.h"

namespace enterprise_connectors {

namespace {

bool IsEventInReportingSettings(const std::string& event,
                                std::optional<ReportingSettings> settings) {
  if (!settings.has_value()) {
    return false;
  }
  if (base::Contains(kAllReportingEnabledEvents, event)) {
    return settings->enabled_event_names.count(event) > 0;
  }
  if (base::Contains(kAllReportingOptInEvents, event)) {
    return settings->enabled_opt_in_events.count(event) > 0;
  }
  return false;
}

void AddAnalysisConnectorVerdictToEvent(
    const ContentAnalysisResponse::Result& result,
    base::Value::Dict& event) {
  base::Value::List triggered_rule_info;
  for (const TriggeredRule& trigger : result.triggered_rules()) {
    base::Value::Dict triggered_rule;
    triggered_rule.Set(kKeyTriggeredRuleName, trigger.rule_name());
    int rule_id_int = 0;
    if (base::StringToInt(trigger.rule_id(), &rule_id_int)) {
      triggered_rule.Set(kKeyTriggeredRuleId, rule_id_int);
    }
    triggered_rule.Set(kKeyUrlCategory, trigger.url_category());

    triggered_rule_info.Append(std::move(triggered_rule));
  }
  event.Set(kKeyTriggeredRuleInfo, std::move(triggered_rule_info));
}

std::string MalwareRuleToThreatType(const std::string& rule_name) {
  if (rule_name == "uws") {
    return kPotentiallyUnwantedDownloadThreatType;
  } else if (rule_name == "malware") {
    return kDangerousDownloadThreatType;
  } else {
    return kUnknownDownloadThreatType;
  }
}

std::string DangerTypeToThreatType(download::DownloadDangerType danger_type) {
  switch (danger_type) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return kDangerousFileTypeDownloadThreatType;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      return kDangerousFileTypeDownloadThreatType;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      return kDangerousDownloadThreatType;
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return kUncommonDownloadThreatType;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      return kDangerousHostDownloadThreatType;
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return kPotentiallyUnwantedDownloadThreatType;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
      return kDangerousAccountCompromiseDownloadThreatType;
    default:
      // This can be reached when reporting an opened download that doesn't have
      // a verdict yet.
      return kUnknownDownloadThreatType;
  }
}

}  // namespace

ReportingEventRouter::ReportingEventRouter(
    RealtimeReportingClientBase* reporting_client)
    : reporting_client_(reporting_client) {}

ReportingEventRouter::~ReportingEventRouter() = default;

bool ReportingEventRouter::IsEventEnabled(const std::string& event) {
  if (!reporting_client_) {
    return false;
  }
  return IsEventInReportingSettings(event,
                                    reporting_client_->GetReportingSettings());
}

void ReportingEventRouter::OnLoginEvent(
    const GURL& url,
    bool is_federated,
    const url::SchemeHostPort& federated_origin,
    const std::u16string& username) {
  if (!IsEventEnabled(kKeyLoginEvent)) {
    return;
  }

  std::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  std::unique_ptr<url_matcher::URLMatcher> matcher =
      CreateURLMatcherForOptInEvent(settings.value(), kKeyLoginEvent);
  if (!IsUrlMatched(matcher.get(), url)) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          policy::kUploadRealtimeReportingEventsUsingProto)) {
    chrome::cros::reporting::proto::Event event;
    *event.mutable_login_event() =
        GetLoginEvent(url, is_federated, federated_origin, username,
                      reporting_client_->GetProfileIdentifier(),
                      reporting_client_->GetProfileUserName());
    *event.mutable_time() = ToProtoTimestamp(base::Time::Now());

    reporting_client_->ReportEvent(std::move(event), settings.value());
  } else {
    base::Value::Dict event;
    event.Set(kKeyUrl, url.spec());
    event.Set(kKeyIsFederated, is_federated);
    if (is_federated) {
      event.Set(kKeyFederatedOrigin, federated_origin.Serialize());
    }
    event.Set(kKeyLoginUserName, MaskUsername(username));

    reporting_client_->ReportEventWithTimestampDeprecated(
        kKeyLoginEvent, std::move(settings.value()), std::move(event),
        base::Time::Now(), /*include_profile_user_name=*/true);
  }
}

void ReportingEventRouter::OnPasswordBreach(
    const std::string& trigger,
    const std::vector<std::pair<GURL, std::u16string>>& identities) {
  if (!IsEventEnabled(kKeyPasswordBreachEvent)) {
    return;
  }

  std::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  std::unique_ptr<url_matcher::URLMatcher> matcher =
      CreateURLMatcherForOptInEvent(settings.value(), kKeyPasswordBreachEvent);
  if (!matcher) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          policy::kUploadRealtimeReportingEventsUsingProto)) {
    chrome::cros::reporting::proto::Event event;
    std::optional<chrome::cros::reporting::proto::PasswordBreachEvent>
        password_breach_event =
            GetPasswordBreachEvent(trigger, identities, settings.value(),
                                   reporting_client_->GetProfileIdentifier(),
                                   reporting_client_->GetProfileUserName());
    if (!password_breach_event.has_value()) {
      return;
    }

    *event.mutable_password_breach_event() = password_breach_event.value();
    *event.mutable_time() = ToProtoTimestamp(base::Time::Now());

    reporting_client_->ReportEvent(std::move(event), settings.value());
  } else {
    base::Value::List identities_list;
    for (const std::pair<GURL, std::u16string>& i : identities) {
      if (!IsUrlMatched(matcher.get(), i.first)) {
        continue;
      }

      base::Value::Dict identity;
      identity.Set(kKeyPasswordBreachIdentitiesUrl, i.first.spec());
      identity.Set(kKeyPasswordBreachIdentitiesUsername,
                   MaskUsername(i.second));
      identities_list.Append(std::move(identity));
    }

    if (identities_list.empty()) {
      // Don't send an empty event if none of the breached identities matched a
      // pattern in the URL filters.
      return;
    }

    base::Value::Dict event;
    event.Set(kKeyTrigger, trigger);
    event.Set(kKeyPasswordBreachIdentities, std::move(identities_list));

    reporting_client_->ReportEventWithTimestampDeprecated(
        kKeyPasswordBreachEvent, std::move(settings.value()), std::move(event),
        base::Time::Now(), /*include_profile_user_name=*/true);
  }
}

void ReportingEventRouter::OnPasswordReuse(const GURL& url,
                                           const std::string& user_name,
                                           bool is_phishing_url,
                                           bool warning_shown) {
  if (!IsEventEnabled(kKeyPasswordReuseEvent)) {
    return;
  }

  std::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (base::FeatureList::IsEnabled(
          policy::kUploadRealtimeReportingEventsUsingProto)) {
    chrome::cros::reporting::proto::Event event;
    *event.mutable_password_reuse_event() =
        GetPasswordReuseEvent(url, user_name, is_phishing_url, warning_shown,
                              reporting_client_->GetProfileIdentifier(),
                              reporting_client_->GetProfileUserName());
    *event.mutable_time() = ToProtoTimestamp(base::Time::Now());

    reporting_client_->ReportEvent(std::move(event), settings.value());
  } else {
    base::Value::Dict event;
    event.Set(kKeyUrl, url.spec());
    event.Set(kKeyUserName, user_name);
    event.Set(kKeyIsPhishingUrl, is_phishing_url);
    event.Set(kKeyEventResult,
              EventResultToString(warning_shown ? EventResult::WARNED
                                                : EventResult::ALLOWED));

    reporting_client_->ReportEventWithTimestampDeprecated(
        kKeyPasswordReuseEvent, std::move(settings.value()), std::move(event),
        base::Time::Now(),
        /*include_profile_user_name=*/true);
  }
}

void ReportingEventRouter::OnPasswordChanged(const std::string& user_name) {
  if (!IsEventEnabled(kKeyPasswordChangedEvent)) {
    return;
  }

  std::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (base::FeatureList::IsEnabled(
          policy::kUploadRealtimeReportingEventsUsingProto)) {
    chrome::cros::reporting::proto::Event event;
    *event.mutable_password_changed_event() = GetPasswordChangedEvent(
        user_name, reporting_client_->GetProfileIdentifier(),
        reporting_client_->GetProfileUserName());
    *event.mutable_time() = ToProtoTimestamp(base::Time::Now());

    reporting_client_->ReportEvent(std::move(event), settings.value());
  } else {
    base::Value::Dict event;
    event.Set(kKeyUserName, user_name);

    reporting_client_->ReportEventWithTimestampDeprecated(
        kKeyPasswordChangedEvent, std::move(settings.value()), std::move(event),
        base::Time::Now(),
        /*include_profile_user_name=*/true);
  }
}

void ReportingEventRouter::OnUrlFilteringInterstitial(
    const GURL& url,
    const std::string& threat_type,
    const safe_browsing::RTLookupResponse& response,
    const ReferrerChain& referrer_chain) {
  if (!IsEventEnabled(kKeyUrlFilteringInterstitialEvent)) {
    return;
  }

  std::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  std::string active_user;
  if (base::FeatureList::IsEnabled(kEnterpriseActiveUserDetection)) {
    active_user = reporting_client_->GetContentAreaAccountEmail(url);
  }

  if (base::FeatureList::IsEnabled(
          policy::kUploadRealtimeReportingEventsUsingProto)) {
    chrome::cros::reporting::proto::Event event;
    *event.mutable_url_filtering_interstitial_event() =
        GetUrlFilteringInterstitialEvent(
            url, threat_type, response,
            reporting_client_->GetProfileIdentifier(),
            reporting_client_->GetProfileUserName(), active_user,
            referrer_chain);

    *event.mutable_time() = ToProtoTimestamp(base::Time::Now());

    reporting_client_->ReportEvent(std::move(event), settings.value());
  } else {
    base::Value::Dict event;
    event.Set(kKeyUrl, url.spec());
    EventResult event_result = GetEventResultFromThreatType(threat_type);
    event.Set(kKeyClickedThrough, event_result == EventResult::BYPASSED);
    if (!threat_type.empty()) {
      event.Set(kKeyThreatType, threat_type);
    }

    if (!active_user.empty()) {
      event.Set(kKeyWebAppSignedInAccount, active_user);
    }
    AddTriggeredRuleInfoToUrlFilteringInterstitialEvent(response, event);
    event.Set(kKeyEventResult, EventResultToString(event_result));

    if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
      AddReferrerChainToEvent(referrer_chain, event);
    }

    reporting_client_->ReportEventWithTimestampDeprecated(
        kKeyUrlFilteringInterstitialEvent, std::move(settings.value()),
        std::move(event), base::Time::Now(),
        /*include_profile_user_name=*/true);
  }
}

void ReportingEventRouter::OnSecurityInterstitialProceeded(
    const GURL& url,
    const std::string& reason,
    int net_error_code,
    const ReferrerChain& referrer_chain) {
  if (!IsEventEnabled(kKeyInterstitialEvent)) {
    return;
  }

  std::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (base::FeatureList::IsEnabled(
          policy::kUploadRealtimeReportingEventsUsingProto)) {
    chrome::cros::reporting::proto::Event event;
    *event.mutable_interstitial_event() = GetInterstitialEvent(
        url, reason, net_error_code,
        /*clicked_through=*/true, EventResult::BYPASSED,
        reporting_client_->GetProfileIdentifier(),
        reporting_client_->GetProfileUserName(), referrer_chain);
    *event.mutable_time() = ToProtoTimestamp(base::Time::Now());

    reporting_client_->ReportEvent(std::move(event), settings.value());
  } else {
    base::Value::Dict event;
    event.Set(kKeyUrl, url.spec());
    event.Set(kKeyReason, reason);
    event.Set(kKeyNetErrorCode, net_error_code);
    event.Set(kKeyClickedThrough, true);
    event.Set(kKeyEventResult, EventResultToString(EventResult::BYPASSED));

    if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
      AddReferrerChainToEvent(referrer_chain, event);
    }

    reporting_client_->ReportEventWithTimestampDeprecated(
        kKeyInterstitialEvent, std::move(settings.value()), std::move(event),
        base::Time::Now(),
        /*include_profile_user_name=*/true);
  }
}

void ReportingEventRouter::OnSecurityInterstitialShown(
    const GURL& url,
    const std::string& reason,
    int net_error_code,
    bool proceed_anyway_disabled,
    const ReferrerChain& referrer_chain) {
  if (!IsEventEnabled(kKeyInterstitialEvent)) {
    return;
  }

  std::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  EventResult event_result =
      proceed_anyway_disabled ? EventResult::BLOCKED : EventResult::WARNED;

  if (base::FeatureList::IsEnabled(
          policy::kUploadRealtimeReportingEventsUsingProto)) {
    chrome::cros::reporting::proto::Event event;
    *event.mutable_interstitial_event() = GetInterstitialEvent(
        url, reason, net_error_code,
        /*clicked_through=*/false, event_result,
        reporting_client_->GetProfileIdentifier(),
        reporting_client_->GetProfileUserName(), referrer_chain);
    *event.mutable_time() = ToProtoTimestamp(base::Time::Now());

    reporting_client_->ReportEvent(std::move(event), settings.value());
  } else {
    base::Value::Dict event;
    event.Set(kKeyUrl, url.spec());
    event.Set(kKeyReason, reason);
    event.Set(kKeyNetErrorCode, net_error_code);
    event.Set(kKeyClickedThrough, false);
    event.Set(kKeyEventResult, EventResultToString(event_result));

    if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
      AddReferrerChainToEvent(referrer_chain, event);
    }

    reporting_client_->ReportEventWithTimestampDeprecated(
        kKeyInterstitialEvent, std::move(settings.value()), std::move(event),
        base::Time::Now(),
        /*include_profile_user_name=*/true);
  }
}

void ReportingEventRouter::OnUnscannedFileEvent(
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
    const int64_t content_size,
    EventResult event_result) {
  if (!IsEventEnabled(kKeyUnscannedFileEvent)) {
    return;
  }

  std::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  std::string final_file_name = GetFileName(
      file_name,
      reporting_client_->ShouldIncludeDeviceInfo(settings->per_profile));

  if (base::FeatureList::IsEnabled(
          policy::kUploadRealtimeReportingEventsUsingProto)) {
    chrome::cros::reporting::proto::Event event;
    *event.mutable_unscanned_file_event() = GetUnscannedFileEvent(
        url, tab_url, source, destination, final_file_name,
        download_digest_sha256, mime_type, trigger, reason,
        content_transfer_method, reporting_client_->GetProfileIdentifier(),
        reporting_client_->GetProfileUserName(), content_size, event_result);
    *event.mutable_time() = ToProtoTimestamp(base::Time::Now());

    reporting_client_->ReportEvent(std::move(event), settings.value());
  } else {
    base::Value::Dict event;
    event.Set(kKeyUrl, url.spec());
    event.Set(kKeyTabUrl, tab_url.spec());
    event.Set(kKeySource, source);
    event.Set(kKeyDestination, destination);
    event.Set(kKeyFileName, final_file_name);
    event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
    event.Set(kKeyContentType, mime_type);
    event.Set(kKeyUnscannedReason, reason);
    // |content_size| can be set to -1 to indicate an unknown size, in
    // which case the field is not set.
    if (content_size >= 0) {
      event.Set(kKeyContentSize, base::Int64ToValue(content_size));
    }
    event.Set(kKeyTrigger, trigger);
    event.Set(kKeyEventResult, EventResultToString(event_result));
    event.Set(kKeyClickedThrough, event_result == EventResult::BYPASSED);
    if (!content_transfer_method.empty()) {
      event.Set(kKeyContentTransferMethod, content_transfer_method);
    }

    reporting_client_->ReportEventWithTimestampDeprecated(
        kKeyUnscannedFileEvent, std::move(settings.value()), std::move(event),
        base::Time::Now(),
        /*include_profile_user_name=*/true);
  }
}

void ReportingEventRouter::OnSensitiveDataEvent(
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
    const std::string& source_email,
    const std::string& content_area_account_email,
    const ContentAnalysisResponse::Result& result,
    const int64_t content_size,
    const ReferrerChain& referrer_chain,
    EventResult event_result) {
  if (!IsEventEnabled(kKeySensitiveDataEvent)) {
    return;
  }

  std::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  std::string final_file_name = GetFileName(
      file_name,
      reporting_client_->ShouldIncludeDeviceInfo(settings->per_profile));

  if (base::FeatureList::IsEnabled(
          policy::kUploadRealtimeReportingEventsUsingProto)) {
    chrome::cros::reporting::proto::Event event;
    *event.mutable_sensitive_data_event() = GetDlpSensitiveDataEvent(
        url, tab_url, source, destination, final_file_name,
        download_digest_sha256, mime_type, trigger, scan_id,
        content_transfer_method, source_email, content_area_account_email,
        reporting_client_->GetProfileIdentifier(),
        reporting_client_->GetProfileUserName(), content_size, result,
        referrer_chain, event_result);
    *event.mutable_time() = ToProtoTimestamp(base::Time::Now());

    reporting_client_->ReportEvent(std::move(event), settings.value());
  } else {
    base::Value::Dict event;
    event.Set(kKeyUrl, url.spec());
    event.Set(kKeyTabUrl, tab_url.spec());
    event.Set(kKeySource, source);
    event.Set(kKeyDestination, destination);
    event.Set(kKeyFileName,
              GetFileName(file_name, reporting_client_->ShouldIncludeDeviceInfo(
                                         settings->per_profile)));
    event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
    event.Set(kKeyContentType, mime_type);
    // |content_size| can be set to -1 to indicate an unknown size, in
    // which case the field is not set.
    if (content_size >= 0) {
      event.Set(kKeyContentSize, base::Int64ToValue(content_size));
    }
    event.Set(kKeyTrigger, trigger);

    if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
      AddReferrerChainToEvent(referrer_chain, event);
    }

    event.Set(kKeyEventResult, EventResultToString(event_result));
    event.Set(kKeyClickedThrough, event_result == EventResult::BYPASSED);
    event.Set(kKeyScanId, scan_id);

    if (!content_transfer_method.empty()) {
      event.Set(kKeyContentTransferMethod, content_transfer_method);
    }
    if (!content_area_account_email.empty()) {
      event.Set(kKeyWebAppSignedInAccount, content_area_account_email);
    }
    if (!source_email.empty()) {
      event.Set(kKeySourceWebAppSignedInAccount, source_email);
    }

    AddAnalysisConnectorVerdictToEvent(result, event);

    reporting_client_->ReportEventWithTimestampDeprecated(
        kKeySensitiveDataEvent, std::move(settings.value()), std::move(event),
        base::Time::Now(),
        /*include_profile_user_name=*/true);
  }
}

void ReportingEventRouter::OnDangerousDownloadEvent(
    const GURL& url,
    const GURL& tab_url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const download::DownloadDangerType danger_type,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& scan_id,
    const int64_t content_size,
    const ReferrerChain& referrer_chain,
    EventResult event_result) {
  OnDangerousDownloadEvent(url, tab_url, /*source=*/"", /*destination=*/"",
                           file_name, download_digest_sha256,
                           DangerTypeToThreatType(danger_type), mime_type,
                           trigger, scan_id,
                           /*content_transfer_method*/ "", content_size,
                           referrer_chain, event_result);
}

void ReportingEventRouter::OnDangerousDownloadEvent(
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
    const int64_t content_size,
    const ReferrerChain& referrer_chain,
    EventResult event_result) {
  if (!IsEventEnabled(kKeyDangerousDownloadEvent)) {
    return;
  }

  std::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyTabUrl, tab_url.spec());
  event.Set(kKeySource, source);
  event.Set(kKeyDestination, destination);
  event.Set(kKeyFileName,
            GetFileName(file_name, reporting_client_->ShouldIncludeDeviceInfo(
                                       settings->per_profile)));
  event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
  event.Set(kKeyThreatType, threat_type);
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, trigger);
  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    AddReferrerChainToEvent(referrer_chain, event);
  }
  event.Set(kKeyEventResult, EventResultToString(event_result));
  event.Set(kKeyClickedThrough, event_result == EventResult::BYPASSED);
  // The scan ID can be empty when the reported dangerous download is from a
  // Safe Browsing verdict.
  if (!scan_id.empty()) {
    event.Set(kKeyScanId, scan_id);
  }
  if (!content_transfer_method.empty()) {
    event.Set(kKeyContentTransferMethod, content_transfer_method);
  }

  reporting_client_->ReportEventWithTimestampDeprecated(
      kKeyDangerousDownloadEvent, std::move(settings.value()), std::move(event),
      base::Time::Now(),
      /*include_profile_user_name=*/true);
}

void ReportingEventRouter::OnAnalysisConnectorResult(
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
    const std::string& source_email,
    const std::string& content_area_account_email,
    const ContentAnalysisResponse::Result& result,
    const int64_t content_size,
    const ReferrerChain& referrer_chain,
    EventResult event_result) {
  if (result.tag() == "malware") {
    DCHECK_EQ(1, result.triggered_rules().size());
    OnDangerousDownloadEvent(
        url, tab_url, source, destination, file_name, download_digest_sha256,
        MalwareRuleToThreatType(result.triggered_rules(0).rule_name()),
        mime_type, trigger, scan_id, content_transfer_method, content_size,
        referrer_chain, event_result);
  } else if (result.tag() == "dlp") {
    OnSensitiveDataEvent(url, tab_url, source, destination, file_name,
                         download_digest_sha256, mime_type, trigger, scan_id,
                         content_transfer_method, source_email,
                         content_area_account_email, result, content_size,
                         referrer_chain, event_result);
  }
}

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
void ReportingEventRouter::OnDataControlsSensitiveDataEvent(
    const GURL& url,
    const GURL& tab_url,
    const std::string& source,
    const std::string& destination,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& source_active_user_email,
    const std::string& content_area_account_email,
    const data_controls::Verdict::TriggeredRules& triggered_rules,
    enterprise_connectors::EventResult event_result,
    int64_t content_size) {
  if (!IsEventEnabled(kKeySensitiveDataEvent)) {
    return;
  }

  std::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyTabUrl, tab_url.spec());
  event.Set(kKeySource, source);
  event.Set(kKeyDestination, destination);
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, trigger);
  if (!content_area_account_email.empty()) {
    event.Set(enterprise_connectors::kKeyWebAppSignedInAccount,
              content_area_account_email);
  }
  if (!source_active_user_email.empty()) {
    event.Set(enterprise_connectors::kKeySourceWebAppSignedInAccount,
              source_active_user_email);
  }
  event.Set(kKeyEventResult,
            enterprise_connectors::EventResultToString(event_result));

  base::Value::List triggered_rule_info;
  triggered_rule_info.reserve(triggered_rules.size());
  for (const auto& [index, rule] : triggered_rules) {
    base::Value::Dict triggered_rule;
    int rule_id_int = 0;
    if (base::StringToInt(rule.rule_id, &rule_id_int)) {
      triggered_rule.Set(kKeyTriggeredRuleId, rule_id_int);
    }
    triggered_rule.Set(kKeyTriggeredRuleName, rule.rule_name);

    triggered_rule_info.Append(std::move(triggered_rule));
  }
  event.Set(kKeyTriggeredRuleInfo, std::move(triggered_rule_info));

  reporting_client_->ReportEventWithTimestampDeprecated(
      kKeySensitiveDataEvent, std::move(settings.value()), std::move(event),
      base::Time::Now(),
      /*include_profile_user_name=*/true);
}
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

// static
std::string ReportingEventRouter::GetFileName(const std::string& filename,
                                              const bool include_full_path) {
  base::FilePath::StringType os_filename;
#if BUILDFLAG(IS_WIN)
  os_filename = base::UTF8ToWide(filename);
#else
  os_filename = filename;
#endif

  return include_full_path
             ? filename
             : base::FilePath(os_filename).BaseName().AsUTF8Unsafe();
}

}  // namespace enterprise_connectors
