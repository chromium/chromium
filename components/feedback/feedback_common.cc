// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_common.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/feedback/feedback_constants.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_util.h"
#include "components/feedback/proto/common.pb.h"
#include "components/feedback/proto/dom.pb.h"
#include "components/feedback/proto/extension.pb.h"
#include "components/feedback/proto/math.pb.h"
#include "google_apis/gaia/gaia_auth_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

namespace {

// The below thresholds were chosen arbitrarily to conveniently show small data
// as part of the report itself without having to look into the system_logs.zip
// file.
constexpr size_t kFeedbackMaxLength = 1024;
constexpr size_t kFeedbackMaxLineCount = 10;

constexpr base::FilePath::CharType kLogsFilename[] =
    FILE_PATH_LITERAL("system_logs.txt");
constexpr char kLogsAttachmentName[] = "system_logs.zip";

constexpr char kZipExt[] = ".zip";

constexpr char kPngMimeType[] = "image/png";
constexpr char kArbitraryMimeType[] = "application/octet-stream";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Keep in sync with
// google3/java/com/google/wireless/android/tools/betterbug/protos/uploadfeedbackreport.proto.
constexpr char kIsCrossDeviceIssueKey[] = "is_cross_device_issue";
constexpr char kIsCrossDeviceIssueTrueValue[] = "true";
constexpr char kTargetDeviceIdKey[] = "target_device_id";
constexpr char kTargetDeviceIdTypeKey[] = "target_device_id_type";
constexpr char kInitiatingDeviceName[] = "initiating_device_name";
// Enum value for MAC_ADDRESS type.
constexpr char kTargetDeviceIdTypeMacAddressValue[] = "1";
constexpr char kInitiatingDeviceNameValue[] = "Chromebook";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

constexpr char kIsOffensiveOrUnsafeKey[] = "is_offensive_or_unsafe";

// Determine if the given feedback value is small enough to not need to
// be compressed.
bool BelowCompressionThreshold(const std::string& content) {
  if (content.length() > kFeedbackMaxLength)
    return false;
  const size_t line_count = base::ranges::count(content, '\n');
  if (line_count > kFeedbackMaxLineCount)
    return false;
  return true;
}

void AddFeedbackData(userfeedback::ExtensionSubmit* feedback_data,
                     const std::string& key,
                     const std::string& value) {
  // Don't bother with empty keys or values.
  if (key.empty() || value.empty())
    return;
  // Create log_value object and add it to the web_data object.
  userfeedback::ProductSpecificData log_value;
  log_value.set_key(key);
  log_value.set_value(value);
  userfeedback::WebData* web_data = feedback_data->mutable_web_data();
  *(web_data->add_product_specific_data()) = log_value;
}

// Adds data as an attachment to feedback_data if the data is non-empty.
void AddAttachment(userfeedback::ExtensionSubmit* feedback_data,
                   const char* name,
                   const std::string& data) {
  if (data.empty())
    return;

  userfeedback::ProductSpecificBinaryData* attachment =
      feedback_data->add_product_specific_binary_data();
  attachment->set_mime_type(kArbitraryMimeType);
  attachment->set_name(name);
  attachment->set_data(data);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// FeedbackCommon::AttachedFile::
////////////////////////////////////////////////////////////////////////////////

FeedbackCommon::AttachedFile::AttachedFile(const std::string& filename,
                                           std::string data)
    : name(filename), data(std::move(data)) {}

FeedbackCommon::AttachedFile::~AttachedFile() {}

////////////////////////////////////////////////////////////////////////////////
// FeedbackCommon::
////////////////////////////////////////////////////////////////////////////////

FeedbackCommon::FeedbackCommon() : product_id_(-1) {}

void FeedbackCommon::AddFile(const std::string& filename, std::string data) {
  base::AutoLock lock(attachments_lock_);
  attachments_.emplace_back(filename, std::move(data));
}

void FeedbackCommon::AddLog(std::string name, std::string value) {
  logs_[std::move(name)] = std::move(value);
}

void FeedbackCommon::AddLogs(SystemLogsMap logs) {
  // The empty logs_ case is just an optimization.
  if (logs_.empty())
    logs_ = std::move(logs);
  else
    logs.insert(logs.begin(), logs.end());
}

bool FeedbackCommon::RemoveLog(std::string name) {
  return logs_.erase(name) == 1;
}

void FeedbackCommon::PrepareReport(
    userfeedback::ExtensionSubmit* feedback_data) const {
  // Unused field, needs to be 0 though.
  feedback_data->set_type_id(0);

  // Set whether we're reporting from ChromeOS or Chrome on another platform.
  userfeedback::ChromeData chrome_data;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const userfeedback::ChromeData_ChromePlatform chrome_platform =
      userfeedback::ChromeData_ChromePlatform_CHROME_OS;
  const int default_product_id = feedback::kChromeOSProductId;
  userfeedback::ChromeOsData chrome_os_data;
  chrome_os_data.set_category(
      userfeedback::ChromeOsData_ChromeOsCategory_OTHER);
  *(chrome_data.mutable_chrome_os_data()) = chrome_os_data;
#else
  const userfeedback::ChromeData_ChromePlatform chrome_platform =
      userfeedback::ChromeData_ChromePlatform_CHROME_BROWSER;
  const int default_product_id = feedback::kChromeBrowserProductId;
  userfeedback::ChromeBrowserData chrome_browser_data;
  chrome_browser_data.set_category(
      userfeedback::ChromeBrowserData_ChromeBrowserCategory_OTHER);
  *(chrome_data.mutable_chrome_browser_data()) = chrome_browser_data;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  chrome_data.set_chrome_platform(chrome_platform);
  // TODO(b/301518187): Investigate if this line is needed in order for custom
  // product IDs to work. Remove `include_chrome_platform_` if it's not needed.
  if (include_chrome_platform_) {
    *(feedback_data->mutable_chrome_data()) = chrome_data;
  }

  feedback_data->set_product_id(HasProductId() ? product_id_
                                               : default_product_id);

  userfeedback::CommonData* common_data = feedback_data->mutable_common_data();
  // We're not using gaia ids, we're using the e-mail field instead.
  common_data->set_gaia_id(0);
  common_data->set_user_email(user_email());
  common_data->set_description(description());
  common_data->set_source_description_language(locale());

  userfeedback::WebData* web_data = feedback_data->mutable_web_data();
  if (!page_url().empty()) {
    web_data->set_url(page_url());
  }
  web_data->mutable_navigator()->set_user_agent(user_agent());

  AddFilesAndLogsToReport(feedback_data);

  if (image().size()) {
    userfeedback::PostedScreenshot screenshot;
    screenshot.set_mime_type(kPngMimeType);

    // Set that we 'have' dimensions of the screenshot. These dimensions are
    // ignored by the server but are a 'required' field in the protobuf.
    userfeedback::Dimensions dimensions;
    dimensions.set_width(0.0);
    dimensions.set_height(0.0);

    *(screenshot.mutable_dimensions()) = dimensions;
    screenshot.set_binary_content(image());

    *(feedback_data->mutable_screenshot()) = screenshot;
  }

  if (category_tag().size())
    feedback_data->set_bucket(category_tag());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::features::IsLinkCrossDeviceDogfoodFeedbackEnabled() &&
      gaia::IsGoogleInternalAccountEmail(user_email()) &&
      mac_address_.has_value()) {
    AddFeedbackData(feedback_data, kIsCrossDeviceIssueKey,
                    kIsCrossDeviceIssueTrueValue);
    AddFeedbackData(feedback_data, kTargetDeviceIdKey, mac_address_.value());
    AddFeedbackData(feedback_data, kTargetDeviceIdTypeKey,
                    kTargetDeviceIdTypeMacAddressValue);
    AddFeedbackData(feedback_data, kInitiatingDeviceName,
                    kInitiatingDeviceNameValue);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (is_offensive_or_unsafe_.has_value()) {
    AddFeedbackData(feedback_data, kIsOffensiveOrUnsafeKey,
                    is_offensive_or_unsafe_.value() ? "true" : "false");
  }
  if (!ai_metadata_.empty()) {
    // Add feedback data for each key/value pair.
    std::optional<base::Value::Dict> dict =
        base::JSONReader::ReadDict(ai_metadata_);
    CHECK(dict);
    for (auto pair : dict.value()) {
      AddFeedbackData(feedback_data, pair.first, pair.second.GetString());
    }
  }
}

void FeedbackCommon::RedactDescription(redaction::RedactionTool& redactor) {
  description_ = redactor.Redact(description_);
}

// static
bool FeedbackCommon::IncludeInSystemLogs(const std::string& key,
                                         bool is_google_email) {
  return is_google_email ||
         key != feedback::FeedbackReport::kAllCrashReportIdsKey;
}

// static
int FeedbackCommon::GetChromeBrowserProductId() {
  return feedback::kChromeBrowserProductId;
}

// static
int FeedbackCommon::GetMahiProductId() {
  return feedback::kMahiFeedbackProductId;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// static
int FeedbackCommon::GetChromeOSProductId() {
  return feedback::kChromeOSProductId;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

FeedbackCommon::~FeedbackCommon() = default;

void FeedbackCommon::CompressFile(const base::FilePath& filename,
                                  const std::string& zipname,
                                  std::string data_to_be_compressed) {
  std::optional<std::string> compressed_data =
      feedback_util::ZipString(filename, data_to_be_compressed);
  if (!compressed_data.has_value()) {
    return;
  }

  std::string attachment_file_name = zipname;
  if (attachment_file_name.empty()) {
    // We need to use the UTF8Unsafe methods here to accommodate Windows,
    // which uses wide strings to store file paths.
    attachment_file_name = filename.BaseName().AsUTF8Unsafe().append(kZipExt);
  }

  AddFile(attachment_file_name, std::move(compressed_data.value()));
}

void FeedbackCommon::CompressLogs() {
  // Convert the system logs into a string that we can compress and send with
  // the report.
  std::string logs = feedback_util::LogsToString(logs_);
  if (!logs.empty()) {
    CompressFile(base::FilePath(kLogsFilename), kLogsAttachmentName,
                 std::move(logs));
  }
}

void FeedbackCommon::AddFilesAndLogsToReport(
    userfeedback::ExtensionSubmit* feedback_data) const {
  for (size_t i = 0; i < attachments(); ++i) {
    const AttachedFile* file = attachment(i);
    AddAttachment(feedback_data, file->name.c_str(), file->data);
  }

  const bool is_google_email = gaia::IsGoogleInternalAccountEmail(user_email());
  for (const auto& iter : logs_) {
    if (BelowCompressionThreshold(iter.second)) {
      // We only send the list of all the crash report IDs if the user has a
      // @google.com email. We do this also in feedback_private_api, but not all
      // code paths go through that so we need to check again here.
      if (FeedbackCommon::IncludeInSystemLogs(iter.first, is_google_email)) {
        // Small enough logs should end up in the report data itself. However,
        // they're still added as part of the system_logs.zip file.
        AddFeedbackData(feedback_data, iter.first, iter.second);
      }
    }
  }
}
