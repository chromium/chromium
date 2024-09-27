// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_COMMON_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_COMMON_H_

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/files/file_path.h"
#include "base/supports_user_data.h"
#include "build/blink_buildflags.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

#if BUILDFLAG(USE_BLINK)
#include "components/download/public/common/download_danger_type.h"

namespace download {
class DownloadItem;
}  // namespace download
#endif  // BUILDFLAG(USE_BLINK)

namespace enterprise_connectors {

// Alias to reduce verbosity when using TriggeredRule::Actions.
using TriggeredRule = ContentAnalysisResponse::Result::TriggeredRule;

// Pair to specify the source and destination.
using SourceDestinationStringPair = std::pair<std::string, std::string>;

// Keys used to read a connector's policy values.
inline constexpr char kKeyServiceProvider[] = "service_provider";
inline constexpr char kKeyLinuxVerification[] = "verification.linux";
inline constexpr char kKeyMacVerification[] = "verification.mac";
inline constexpr char kKeyWindowsVerification[] = "verification.windows";
inline constexpr char kKeyEnable[] = "enable";
inline constexpr char kKeyDisable[] = "disable";
inline constexpr char kKeyUrlList[] = "url_list";
inline constexpr char kKeySourceDestinationList[] = "source_destination_list";
inline constexpr char kKeyTags[] = "tags";
inline constexpr char kKeyBlockUntilVerdict[] = "block_until_verdict";
inline constexpr char kKeyBlockPasswordProtected[] = "block_password_protected";
inline constexpr char kKeyBlockLargeFiles[] = "block_large_files";
inline constexpr char kKeyMinimumDataSize[] = "minimum_data_size";
inline constexpr char kKeyEnabledEventNames[] = "enabled_event_names";
inline constexpr char kKeyCustomMessages[] = "custom_messages";
inline constexpr char kKeyRequireJustificationTags[] =
    "require_justification_tags";
inline constexpr char kKeyCustomMessagesTag[] = "tag";
inline constexpr char kKeyCustomMessagesMessage[] = "message";
inline constexpr char kKeyCustomMessagesLearnMoreUrl[] = "learn_more_url";
inline constexpr char kKeyMimeTypes[] = "mime_types";
inline constexpr char kKeyEnterpriseId[] = "enterprise_id";
inline constexpr char kKeyDefaultAction[] = "default_action";
inline constexpr char kKeyDomain[] = "domain";
inline constexpr char kKeyEnabledOptInEvents[] = "enabled_opt_in_events";
inline constexpr char kKeyOptInEventName[] = "name";
inline constexpr char kKeyOptInEventUrlPatterns[] = "url_patterns";

// Available tags.
inline constexpr char kDlpTag[] = "dlp";
inline constexpr char kMalwareTag[] = "malware";

// A MIME type string that matches all MIME types.
inline constexpr char kWildcardMimeType[] = "*";

// The reporting connector subdirectory in User_Data_Directory
inline constexpr base::FilePath::CharType RC_BASE_DIR[] =
    FILE_PATH_LITERAL("Enterprise/ReportingConnector/");

enum class ReportingConnector {
  SECURITY_EVENT,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep this enum in sync with
// EnterpriseReportingEventType in enums.xml.
enum class EnterpriseReportingEventType {
  kUnknownEvent = 0,
  kPasswordReuseEvent = 1,
  kPasswordChangedEvent = 2,
  kDangerousDownloadEvent = 3,
  kInterstitialEvent = 4,
  kSensitiveDataEvent = 5,
  kUnscannedFileEvent = 6,
  kLoginEvent = 7,
  kPasswordBreachEvent = 8,
  kUrlFilteringInterstitialEvent = 9,
  kExtensionInstallEvent = 10,
  kBrowserCrashEvent = 11,
  kExtensionTelemetryEvent = 12,
  kMaxValue = kExtensionTelemetryEvent,
};

// Mapping from event name to UMA enum for logging histogram.
inline constexpr auto kEventNameToUmaEnumMap =
    base::MakeFixedFlatMap<std::string_view, EnterpriseReportingEventType>({
        {kKeyPasswordReuseEvent,
         EnterpriseReportingEventType::kPasswordReuseEvent},
        {kKeyPasswordChangedEvent,
         EnterpriseReportingEventType::kPasswordChangedEvent},
        {kKeyDangerousDownloadEvent,
         EnterpriseReportingEventType::kDangerousDownloadEvent},
        {kKeyInterstitialEvent,
         EnterpriseReportingEventType::kInterstitialEvent},
        {kKeySensitiveDataEvent,
         EnterpriseReportingEventType::kSensitiveDataEvent},
        {kKeyUnscannedFileEvent,
         EnterpriseReportingEventType::kUnscannedFileEvent},
        {kKeyLoginEvent, EnterpriseReportingEventType::kLoginEvent},
        {kKeyPasswordBreachEvent,
         EnterpriseReportingEventType::kPasswordBreachEvent},
        {kKeyUrlFilteringInterstitialEvent,
         EnterpriseReportingEventType::kUrlFilteringInterstitialEvent},
        {kExtensionInstallEvent,
         EnterpriseReportingEventType::kExtensionInstallEvent},
        {kBrowserCrashEvent, EnterpriseReportingEventType::kBrowserCrashEvent},
        {kExtensionTelemetryEvent,
         EnterpriseReportingEventType::kExtensionTelemetryEvent},
    });

// Struct holding the necessary data to tweak the behavior of the reporting
// Connector.
struct ReportingSettings {
  ReportingSettings();
  ReportingSettings(const std::string& dm_token, bool per_profile);
  ReportingSettings(ReportingSettings&&);
  ReportingSettings(const ReportingSettings&);
  ReportingSettings& operator=(ReportingSettings&&);
  ~ReportingSettings();

  std::set<std::string> enabled_event_names;
  std::map<std::string, std::vector<std::string>> enabled_opt_in_events;
  std::string dm_token;

  // Indicates if the report should be made for the profile, or the browser if
  // false.
  bool per_profile = false;
};

// Returns the pref path corresponding to an analysis connector.
const char* AnalysisConnectorPref(AnalysisConnector connector);
const char* AnalysisConnectorScopePref(AnalysisConnector connector);

// Returns the highest precedence action in the given parameters. Writes the tag
// field of the result containing the highest precedence action into |tag|.
TriggeredRule::Action GetHighestPrecedenceAction(
    const ContentAnalysisResponse& response,
    std::string* tag);
TriggeredRule::Action GetHighestPrecedenceAction(
    const TriggeredRule::Action& action_1,
    const TriggeredRule::Action& action_2);
ContentAnalysisAcknowledgement::FinalAction GetHighestPrecedenceAction(
    const ContentAnalysisAcknowledgement::FinalAction& action_1,
    const ContentAnalysisAcknowledgement::FinalAction& action_2);

// Struct used to persist metadata about a file in base::SupportsUserData
// through ScanResult.
struct FileMetadata {
  FileMetadata(
      const std::string& filename,
      const std::string& sha256,
      const std::string& mime_type,
      int64_t size,
      const ContentAnalysisResponse& scan_response = ContentAnalysisResponse());
  FileMetadata(FileMetadata&&);
  FileMetadata(const FileMetadata&);
  FileMetadata& operator=(const FileMetadata&);
  ~FileMetadata();

  std::string filename;
  std::string sha256;
  std::string mime_type;
  int64_t size;
  ContentAnalysisResponse scan_response;
};

// User data class to persist scanning results for multiple files corresponding
// to a single base::SupportsUserData object.
struct ScanResult : public base::SupportsUserData::Data {
  ScanResult();
  explicit ScanResult(FileMetadata metadata);
  ~ScanResult() override;
  static const char kKey[];

  std::vector<FileMetadata> file_metadata;
  std::optional<std::u16string> user_justification;
};

// Enum to identify which message to show once scanning is complete. Ordered
// by precedence for when multiple files have conflicting results.
enum class FinalContentAnalysisResult {
  // Show that an issue was found and that the upload is blocked.
  FAILURE = 0,

  // Show that the scan failed and that the upload is blocked.
  FAIL_CLOSED = 1,

  // Show that files were not uploaded since they were too large.
  LARGE_FILES = 2,

  // Show that files were not uploaded since they were encrypted.
  ENCRYPTED_FILES = 3,

  // Show that DLP checks failed, but that the user can proceed if they want.
  WARNING = 4,

  // Show that no issue was found and that the user may proceed.
  SUCCESS = 5,
};

// Result for a single request of the RequestHandler classes.
struct RequestHandlerResult {
  RequestHandlerResult();
  ~RequestHandlerResult();
  RequestHandlerResult(RequestHandlerResult&&);
  RequestHandlerResult& operator=(RequestHandlerResult&&);
  RequestHandlerResult(const RequestHandlerResult&);
  RequestHandlerResult& operator=(const RequestHandlerResult&);

  bool complies;
  FinalContentAnalysisResult final_result;
  std::string tag;
  std::string request_token;
  ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage
      custom_rule_message;
};

// Calculates the ContentAnalysisAcknowledgement::FinalAction for an action
// based on the response it got from scanning.
ContentAnalysisAcknowledgement::FinalAction GetAckFinalAction(
    const ContentAnalysisResponse& response);

// Extracts the message string from the custom rule message field in the content
// analysis response.
std::u16string GetCustomRuleString(
    const ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage&
        custom_rule_message);

// Extracts the ranges and their corresponding links from the custom rule
// message field in the content analysis response. Used to style the custom rule
// message in the content analysis dialog. `offset` corresponds to its start
// index as we are inserting it in another message.
std::vector<std::pair<gfx::Range, GURL>> GetCustomRuleStyles(
    const ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage&
        custom_rule_message,
    size_t offset);

// Simple custom rule message for tests, with one message segment containing the
// text and associated url.
ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage
CreateSampleCustomRuleMessage(const std::u16string& msg,
                              const std::string& url);

#if BUILDFLAG(USE_BLINK)
// Extracts the custom rule message from `download_item`. The rule for that
// message needs to have an action (WARN, BLOCK) corresponding to `danger_type`.
std::optional<ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage>
GetDownloadsCustomRuleMessage(const download::DownloadItem* download_item,
                              download::DownloadDangerType danger_type);
#endif  // BUILDFLAG(USE_BLINK)

// Checks if |response| contains a negative malware verdict.
bool ContainsMalwareVerdict(const ContentAnalysisResponse& response);

enum EnterpriseRealTimeUrlCheckMode {
  REAL_TIME_CHECK_DISABLED = 0,
  REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED = 1,
};

// Helper enum to get the corresponding regional url in service provider config
// for data region setting policy.
// LINT.IfChange(DataRegion)
enum class DataRegion { NO_PREFERENCE = 0, UNITED_STATES = 1, EUROPE = 2 };
// LINT.ThenChange(//components/enterprise/connectors/core/service_provider_config.cc:DlpRegionEndpoints)
GURL GetRegionalizedEndpoint(base::span<const char* const> region_urls,
                             DataRegion data_region);
DataRegion ChromeDataRegionSettingToEnum(int chrome_data_region_setting);

EnterpriseReportingEventType GetUmaEnumFromEventName(
    std::string_view eventName);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_COMMON_H_
