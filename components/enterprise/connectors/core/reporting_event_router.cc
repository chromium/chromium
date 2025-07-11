// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/reporting_event_router.h"

#include "base/containers/contains.h"
#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/common/proto/synced_from_google3/chrome_reporting_entity.pb.h"
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
  std::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value()) {
    return;
  }

  std::unique_ptr<url_matcher::URLMatcher> matcher =
      CreateURLMatcherForOptInEvent(settings.value(),
                                    enterprise_connectors::kKeyLoginEvent);
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
  std::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value()) {
    return;
  }

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
  std::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyPasswordReuseEvent) == 0) {
    return;
  }

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
  std::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyPasswordChangedEvent) == 0) {
    return;
  }

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
  std::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() || settings->enabled_event_names.count(
                                   kKeyUrlFilteringInterstitialEvent) == 0) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          policy::kUploadRealtimeReportingEventsUsingProto)) {
    chrome::cros::reporting::proto::Event event;
    *event.mutable_url_filtering_interstitial_event() =
        GetUrlFilteringInterstitialEvent(
            url, threat_type, response,
            reporting_client_->GetProfileIdentifier(),
            reporting_client_->GetProfileUserName(), referrer_chain);
    *event.mutable_time() = ToProtoTimestamp(base::Time::Now());

    reporting_client_->ReportEvent(std::move(event), settings.value());
  } else {
    base::Value::Dict event;
    event.Set(kKeyUrl, url.spec());
    EventResult event_result = GetEventResultFromThreatType(threat_type);
    event.Set(kKeyClickedThrough,
              event_result == enterprise_connectors::EventResult::BYPASSED);
    if (!threat_type.empty()) {
      event.Set(kKeyThreatType, threat_type);
    }
    AddTriggeredRuleInfoToUrlFilteringInterstitialEvent(response, event);
    event.Set(kKeyEventResult,
              enterprise_connectors::EventResultToString(event_result));

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
  std::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(
          enterprise_connectors::kKeyInterstitialEvent) == 0) {
    return;
  }

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
    event.Set(kKeyEventResult,
              enterprise_connectors::EventResultToString(
                  enterprise_connectors::EventResult::BYPASSED));

    if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
      AddReferrerChainToEvent(referrer_chain, event);
    }

    reporting_client_->ReportEventWithTimestampDeprecated(
        enterprise_connectors::kKeyInterstitialEvent,
        std::move(settings.value()), std::move(event), base::Time::Now(),
        /*include_profile_user_name=*/true);
  }
}

void ReportingEventRouter::OnSecurityInterstitialShown(
    const GURL& url,
    const std::string& reason,
    int net_error_code,
    bool proceed_anyway_disabled,
    const ReferrerChain& referrer_chain) {
  std::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(
          enterprise_connectors::kKeyInterstitialEvent) == 0) {
    return;
  }

  enterprise_connectors::EventResult event_result =
      proceed_anyway_disabled ? enterprise_connectors::EventResult::BLOCKED
                              : enterprise_connectors::EventResult::WARNED;

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
    event.Set(kKeyEventResult,
              enterprise_connectors::EventResultToString(event_result));

    if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
      AddReferrerChainToEvent(referrer_chain, event);
    }

    reporting_client_->ReportEventWithTimestampDeprecated(
        enterprise_connectors::kKeyInterstitialEvent,
        std::move(settings.value()), std::move(event), base::Time::Now(),
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
    const ReferrerChain& referrer_chain,
    EventResult event_result) {
  std::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(
          enterprise_connectors::kKeyUnscannedFileEvent) == 0) {
    return;
  }

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
  event.Set(kKeyUnscannedReason, reason);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, trigger);
  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    enterprise_connectors::AddReferrerChainToEvent(referrer_chain, event);
  }
  event.Set(kKeyEventResult,
            enterprise_connectors::EventResultToString(event_result));
  event.Set(kKeyClickedThrough,
            event_result == enterprise_connectors::EventResult::BYPASSED);
  if (!content_transfer_method.empty()) {
    event.Set(kKeyContentTransferMethod, content_transfer_method);
  }

  reporting_client_->ReportEventWithTimestampDeprecated(
      enterprise_connectors::kKeyUnscannedFileEvent,
      std::move(settings.value()), std::move(event), base::Time::Now(),
      /*include_profile_user_name=*/true);
}

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
