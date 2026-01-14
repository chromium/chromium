// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/common.h"

#include <algorithm>

#include "base/containers/map_util.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/gfx/range/range.h"

#if BUILDFLAG(USE_BLINK)
#include "components/download/public/common/download_item.h"  // nogncheck
#endif  // BUILDFLAG(USE_BLINK)

namespace enterprise_connectors {

namespace {

inline constexpr auto kUmaEnumToStringMap =
    base::MakeFixedFlatMap<EnterpriseReportingEventType, std::string_view>({
        {EnterpriseReportingEventType::kPasswordReuseEvent,
         kPasswordReuseUmaMetricName},
        {EnterpriseReportingEventType::kPasswordChangedEvent,
         kPasswordChangedUmaMetricName},
        {EnterpriseReportingEventType::kDangerousDownloadEvent,
         kDangerousDownloadUmaMetricName},
        {EnterpriseReportingEventType::kInterstitialEvent,
         kInterstitialUmaMetricName},
        {EnterpriseReportingEventType::kSensitiveDataEvent,
         kSensitiveDataUmaMetricName},
        {EnterpriseReportingEventType::kUnscannedFileEvent,
         kUnscannedFileUmaMetricName},
        {EnterpriseReportingEventType::kLoginEvent, kLoginUmaMetricName},
        {EnterpriseReportingEventType::kPasswordBreachEvent,
         kPasswordBreachUmaMetricName},
        {EnterpriseReportingEventType::kUrlFilteringInterstitialEvent,
         kUrlFilteringInterstitialUmaMetricName},
        {EnterpriseReportingEventType::kExtensionInstallEvent,
         kExtensionInstallUmaMetricName},
        {EnterpriseReportingEventType::kBrowserCrashEvent,
         kBrowserCrashUmaMetricName},
        {EnterpriseReportingEventType::kExtensionTelemetryEvent,
         kExtensionTelemetryUmaMetricName},
    });

inline constexpr auto kEventCaseToUmaEnumMap =
    base::MakeFixedFlatMap<EventCase, EnterpriseReportingEventType>({
        {EventCase::kPasswordReuseEvent,
         EnterpriseReportingEventType::kPasswordReuseEvent},
        {EventCase::kPasswordChangedEvent,
         EnterpriseReportingEventType::kPasswordChangedEvent},
        {EventCase::kDangerousDownloadEvent,
         EnterpriseReportingEventType::kDangerousDownloadEvent},
        {EventCase::kInterstitialEvent,
         EnterpriseReportingEventType::kInterstitialEvent},
        {EventCase::kSensitiveDataEvent,
         EnterpriseReportingEventType::kSensitiveDataEvent},
        {EventCase::kUnscannedFileEvent,
         EnterpriseReportingEventType::kUnscannedFileEvent},
        {EventCase::kLoginEvent, EnterpriseReportingEventType::kLoginEvent},
        {EventCase::kPasswordBreachEvent,
         EnterpriseReportingEventType::kPasswordBreachEvent},
        {EventCase::kUrlFilteringInterstitialEvent,
         EnterpriseReportingEventType::kUrlFilteringInterstitialEvent},
        {EventCase::kBrowserExtensionInstallEvent,
         EnterpriseReportingEventType::kExtensionInstallEvent},
        {EventCase::kBrowserCrashEvent,
         EnterpriseReportingEventType::kBrowserCrashEvent},
        {EventCase::kExtensionTelemetryEvent,
         EnterpriseReportingEventType::kExtensionTelemetryEvent},
    });

ContentAnalysisAcknowledgement::FinalAction RuleActionToAckAction(
    TriggeredRule::Action action) {
  switch (action) {
    case TriggeredRule::ACTION_UNSPECIFIED:
      return ContentAnalysisAcknowledgement::ACTION_UNSPECIFIED;
    case TriggeredRule::REPORT_ONLY:
      return ContentAnalysisAcknowledgement::REPORT_ONLY;
    case TriggeredRule::WARN:
      return ContentAnalysisAcknowledgement::WARN;
    case TriggeredRule::FORCE_SAVE_TO_CLOUD:
    case TriggeredRule::BLOCK:
      return ContentAnalysisAcknowledgement::BLOCK;
  }
}

}  // namespace

ReportingSettings::ReportingSettings() = default;
ReportingSettings::ReportingSettings(const std::string& dm_token,
                                     bool per_profile)
    : dm_token(dm_token), per_profile(per_profile) {}
ReportingSettings::ReportingSettings(ReportingSettings&&) = default;
ReportingSettings::ReportingSettings(const ReportingSettings&) = default;
ReportingSettings& ReportingSettings::operator=(ReportingSettings&&) = default;
ReportingSettings::~ReportingSettings() = default;

const char* AnalysisConnectorPref(AnalysisConnector connector) {
  switch (connector) {
    case AnalysisConnector::BULK_DATA_ENTRY:
      return kOnBulkDataEntryPref;
    case AnalysisConnector::FILE_DOWNLOADED:
      return kOnFileDownloadedPref;
    case AnalysisConnector::FILE_ATTACHED:
      return kOnFileAttachedPref;
    case AnalysisConnector::PRINT:
      return kOnPrintPref;
    case AnalysisConnector::FILE_TRANSFER:
#if BUILDFLAG(IS_CHROMEOS)
      return kOnFileTransferPref;
#endif
    case AnalysisConnector::ANALYSIS_CONNECTOR_UNSPECIFIED:
      NOTREACHED() << "Using unspecified analysis connector";
  }
}

const char* AnalysisConnectorScopePref(AnalysisConnector connector) {
  switch (connector) {
    case AnalysisConnector::BULK_DATA_ENTRY:
      return kOnBulkDataEntryScopePref;
    case AnalysisConnector::FILE_DOWNLOADED:
      return kOnFileDownloadedScopePref;
    case AnalysisConnector::FILE_ATTACHED:
      return kOnFileAttachedScopePref;
    case AnalysisConnector::PRINT:
      return kOnPrintScopePref;
    case AnalysisConnector::FILE_TRANSFER:
#if BUILDFLAG(IS_CHROMEOS)
      return kOnFileTransferScopePref;
#endif
    case AnalysisConnector::ANALYSIS_CONNECTOR_UNSPECIFIED:
      NOTREACHED() << "Using unspecified analysis connector";
  }
}

TriggeredRule::Action GetHighestPrecedenceAction(
    const ContentAnalysisResponse& response,
    std::string* tag) {
  auto action = TriggeredRule::ACTION_UNSPECIFIED;

  for (const auto& result : response.results()) {
    if (!result.has_status() ||
        result.status() != ContentAnalysisResponse::Result::SUCCESS) {
      continue;
    }

    for (const auto& rule : result.triggered_rules()) {
      auto higher_precedence_action =
          GetHighestPrecedenceAction(action, rule.action());
      if (higher_precedence_action != action && tag != nullptr) {
        *tag = result.tag();
      }
      action = higher_precedence_action;
    }
  }
  return action;
}

TriggeredRule::Action GetHighestPrecedenceAction(
    const TriggeredRule::Action& action_1,
    const TriggeredRule::Action& action_2) {
  // Don't use the enum's int values to determine precedence since that
  // may introduce bugs for new actions later.
  //
  // The current precedence is BLOCK > WARN > REPORT_ONLY > UNSPECIFIED
  if (action_1 == TriggeredRule::BLOCK || action_2 == TriggeredRule::BLOCK) {
    return TriggeredRule::BLOCK;
  }
  if (action_1 == TriggeredRule::FORCE_SAVE_TO_CLOUD ||
      action_2 == TriggeredRule::FORCE_SAVE_TO_CLOUD) {
    return TriggeredRule::FORCE_SAVE_TO_CLOUD;
  }
  if (action_1 == TriggeredRule::WARN || action_2 == TriggeredRule::WARN) {
    return TriggeredRule::WARN;
  }
  if (action_1 == TriggeredRule::REPORT_ONLY ||
      action_2 == TriggeredRule::REPORT_ONLY) {
    return TriggeredRule::REPORT_ONLY;
  }
  if (action_1 == TriggeredRule::ACTION_UNSPECIFIED ||
      action_2 == TriggeredRule::ACTION_UNSPECIFIED) {
    return TriggeredRule::ACTION_UNSPECIFIED;
  }
  NOTREACHED();
}

ContentAnalysisAcknowledgement::FinalAction GetHighestPrecedenceAction(
    const ContentAnalysisAcknowledgement::FinalAction& action_1,
    const ContentAnalysisAcknowledgement::FinalAction& action_2) {
  // Don't use the enum's int values to determine precedence since that
  // may introduce bugs for new actions later.
  //
  // The current precedence is BLOCK > WARN > REPORT_ONLY > ALLOW > UNSPECIFIED
  if (action_1 == ContentAnalysisAcknowledgement::BLOCK ||
      action_2 == ContentAnalysisAcknowledgement::BLOCK) {
    return ContentAnalysisAcknowledgement::BLOCK;
  }
  if (action_1 == ContentAnalysisAcknowledgement::WARN ||
      action_2 == ContentAnalysisAcknowledgement::WARN) {
    return ContentAnalysisAcknowledgement::WARN;
  }
  if (action_1 == ContentAnalysisAcknowledgement::REPORT_ONLY ||
      action_2 == ContentAnalysisAcknowledgement::REPORT_ONLY) {
    return ContentAnalysisAcknowledgement::REPORT_ONLY;
  }
  if (action_1 == ContentAnalysisAcknowledgement::ALLOW ||
      action_2 == ContentAnalysisAcknowledgement::ALLOW) {
    return ContentAnalysisAcknowledgement::ALLOW;
  }
  if (action_1 == ContentAnalysisAcknowledgement::ACTION_UNSPECIFIED ||
      action_2 == ContentAnalysisAcknowledgement::ACTION_UNSPECIFIED) {
    return ContentAnalysisAcknowledgement::ACTION_UNSPECIFIED;
  }
  NOTREACHED();
}

FileMetadata::FileMetadata(const std::string& filename,
                           const std::string& sha256,
                           const std::string& mime_type,
                           int64_t size,
                           const ContentAnalysisResponse& scan_response)
    : filename(filename),
      sha256(sha256),
      mime_type(mime_type),
      size(size),
      scan_response(scan_response) {}
FileMetadata::FileMetadata(FileMetadata&&) = default;
FileMetadata::FileMetadata(const FileMetadata&) = default;
FileMetadata& FileMetadata::operator=(const FileMetadata&) = default;
FileMetadata::~FileMetadata() = default;

const char ScanResult::kKey[] = "enterprise_connectors.scan_result_key";
ScanResult::ScanResult() = default;
ScanResult::ScanResult(FileMetadata metadata) {
  file_metadata.push_back(std::move(metadata));
}
ScanResult::~ScanResult() = default;

RequestHandlerResult::RequestHandlerResult() = default;
RequestHandlerResult::~RequestHandlerResult() = default;
RequestHandlerResult::RequestHandlerResult(RequestHandlerResult&&) = default;
RequestHandlerResult& RequestHandlerResult::operator=(RequestHandlerResult&&) =
    default;
RequestHandlerResult::RequestHandlerResult(const RequestHandlerResult&) =
    default;
RequestHandlerResult& RequestHandlerResult::operator=(
    const RequestHandlerResult&) = default;

ContentAnalysisAcknowledgement::FinalAction GetAckFinalAction(
    const ContentAnalysisResponse& response) {
  auto final_action = ContentAnalysisAcknowledgement::ALLOW;
  for (const auto& result : response.results()) {
    if (!result.has_status() ||
        result.status() != ContentAnalysisResponse::Result::SUCCESS) {
      continue;
    }

    for (const auto& rule : result.triggered_rules()) {
      final_action = GetHighestPrecedenceAction(
          final_action, RuleActionToAckAction(rule.action()));
    }
  }

  return final_action;
}

std::u16string GetCustomRuleString(
    const ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage&
        custom_rule_message) {
  std::u16string custom_message;
  for (const auto& custom_segment : custom_rule_message.message_segments()) {
    base::StrAppend(
        &custom_message,
        {base::UnescapeForHTML(base::UTF8ToUTF16(custom_segment.text()))});
  }
  return custom_message;
}

std::vector<std::pair<gfx::Range, GURL>> GetCustomRuleStyles(
    const ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage&
        custom_rule_message,
    size_t offset) {
  std::vector<std::pair<gfx::Range, GURL>> linked_ranges;
  for (const auto& custom_segment : custom_rule_message.message_segments()) {
    std::u16string unescaped_segment =
        base::UnescapeForHTML(base::UTF8ToUTF16(custom_segment.text()));
    if (custom_segment.has_link()) {
      GURL url(custom_segment.link());
      if (url.is_valid()) {
        linked_ranges.emplace_back(
            gfx::Range(offset, offset + unescaped_segment.length()), url);
      }
    }
    offset += unescaped_segment.length();
  }
  return linked_ranges;
}

ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage
CreateSampleCustomRuleMessage(const std::u16string& msg,
                              const std::string& url) {
  ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage
      custom_message;
  auto* custom_segment = custom_message.add_message_segments();
  custom_segment->set_text(base::UTF16ToUTF8(msg));
  custom_segment->set_link(url);
  return custom_message;
}

#if BUILDFLAG(USE_BLINK)
std::optional<ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage>
GetDownloadsCustomRuleMessage(const download::DownloadItem* download_item,
                              download::DownloadDangerType danger_type) {
  if (!download_item) {
    return std::nullopt;
  }

  // Custom rule message is currently only present for either warning or block
  // danger types.
  TriggeredRule::Action current_action;
  if (danger_type == download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING) {
    current_action = TriggeredRule::WARN;
  } else if (danger_type ==
             download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK) {
    current_action = TriggeredRule::BLOCK;
  } else {
    return std::nullopt;
  }

  enterprise_connectors::ScanResult* scan_result =
      static_cast<enterprise_connectors::ScanResult*>(
          download_item->GetUserData(enterprise_connectors::ScanResult::kKey));
  if (!scan_result) {
    return std::nullopt;
  }
  for (const auto& metadata : scan_result->file_metadata) {
    for (const auto& result : metadata.scan_response.results()) {
      for (const auto& rule : result.triggered_rules()) {
        if (rule.action() == current_action && rule.has_custom_rule_message()) {
          return rule.custom_rule_message();
        }
      }
    }
  }
  return std::nullopt;
}
#endif  // BUILDFLAG(USE_BLINK)

bool ContainsMalwareVerdict(const ContentAnalysisResponse& response) {
  return std::ranges::any_of(response.results(), [](const auto& result) {
    return result.tag() == kMalwareTag && !result.triggered_rules().empty();
  });
}

GURL GetRegionalizedEndpoint(base::span<const char* const> region_urls,
                             DataRegion data_region) {
  switch (data_region) {
    case DataRegion::NO_PREFERENCE:
      return GURL(region_urls[0]);
    case DataRegion::UNITED_STATES:
      return GURL(region_urls[1]);
    case DataRegion::EUROPE:
      return GURL(region_urls[2]);
  }
}

DataRegion ChromeDataRegionSettingToEnum(int chrome_data_region_setting) {
  switch (chrome_data_region_setting) {
    case 0:
      return DataRegion::NO_PREFERENCE;
    case 1:
      return DataRegion::UNITED_STATES;
    case 2:
      return DataRegion::EUROPE;
  }
  NOTREACHED();
}

EnterpriseReportingEventType GetUmaEnumFromEventName(
    std::string_view eventName) {
  auto it = kEventNameToUmaEnumMap.find(eventName);
  return it != kEventNameToUmaEnumMap.end()
             ? it->second
             : EnterpriseReportingEventType::kUnknownEvent;
}


EnterpriseReportingEventType GetUmaEnumFromEventCase(EventCase eventCase) {
  auto it = kEventCaseToUmaEnumMap.find(eventCase);
  return it != kEventCaseToUmaEnumMap.end()
             ? it->second
             : EnterpriseReportingEventType::kUnknownEvent;
}

std::string EventResultToString(EventResult result) {
  switch (result) {
    case EventResult::UNKNOWN:
      return "EVENT_RESULT_UNKNOWN";
    case EventResult::ALLOWED:
      return "EVENT_RESULT_ALLOWED";
    case EventResult::WARNED:
      return "EVENT_RESULT_WARNED";
    case EventResult::BLOCKED:
      return "EVENT_RESULT_BLOCKED";
    case EventResult::BYPASSED:
      return "EVENT_RESULT_BYPASSED";
    case EventResult::FORCED_SAVE_TO_CLOUD:
      return "EVENT_RESULT_FORCED_SAVE_TO_CLOUD";
  }
  NOTREACHED();
}

std::string GetProfileEmail(signin::IdentityManager* identity_manager) {
  // If the profile is not signed in, GetPrimaryAccountInfo() returns an
  // empty account info.
  return identity_manager
             ? identity_manager
                   ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                   .email
             : std::string();
}

std::string GetSuccessfulUploadDurationUmaMetricName(
    EnterpriseReportingEventType event_type) {
  auto* metric_name = base::FindOrNull(kUmaEnumToStringMap, event_type);
  return metric_name
             ? base::StrCat({*metric_name, "UploadSuccess.Duration"})
             : base::StrCat({kUnknownUmaMetricName, "UploadSuccess.Duration"});
}

std::string GetFailedUploadDurationUmaMetricName(
    EnterpriseReportingEventType event_type) {
  auto* metric_name = base::FindOrNull(kUmaEnumToStringMap, event_type);
  return metric_name
             ? base::StrCat({*metric_name, "UploadFailure.Duration"})
             : base::StrCat({kUnknownUmaMetricName, "UploadFailure.Duration"});
}

std::string DeepScanAccessPointToString(DeepScanAccessPoint access_point) {
  switch (access_point) {
    case DeepScanAccessPoint::DOWNLOAD:
      return "Download";
    case DeepScanAccessPoint::UPLOAD:
      return "Upload";
    case DeepScanAccessPoint::DRAG_AND_DROP:
      return "DragAndDrop";
    case DeepScanAccessPoint::PASTE:
      return "Paste";
    case DeepScanAccessPoint::PRINT:
      return "Print";
    case DeepScanAccessPoint::FILE_TRANSFER:
      return "FileTransfer";
  }
  NOTREACHED();
}

}  // namespace enterprise_connectors
