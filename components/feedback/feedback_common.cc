// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_common.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_util.h"
#include "components/feedback/proto/common.pb.h"
#include "components/feedback/proto/dom.pb.h"
#include "components/feedback/proto/extension.pb.h"
#include "components/feedback/proto/math.pb.h"

namespace {

#if defined(OS_CHROMEOS)
constexpr int kChromeOSProductId = 208;
#else
constexpr int kChromeBrowserProductId = 237;
#endif

constexpr char kMultilineIndicatorString[] = "<multiline>\n";
constexpr char kMultilineStartString[] = "---------- START ----------\n";
constexpr char kMultilineEndString[] = "---------- END ----------\n\n";

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

// Determine if the given feedback value is small enough to not need to
// be compressed.
bool BelowCompressionThreshold(const std::string& content) {
  if (content.length() > kFeedbackMaxLength)
    return false;
  const size_t line_count = std::count(content.begin(), content.end(), '\n');
  if (line_count > kFeedbackMaxLineCount)
    return false;
  return true;
}

// Converts the system logs into a string that we can compress and send
// with the report.
std::string LogsToString(const FeedbackCommon::SystemLogsMap& sys_info) {
  std::string syslogs_string;
  for (const auto& iter : sys_info) {
    std::string key = iter.first;
    std::string value = iter.second;

    base::TrimString(key, "\n ", &key);
    base::TrimString(value, "\n ", &value);

    // We must avoid adding the crash IDs to the system_logs.txt file for
    // privacy reasons. They should just be part of the product specific data.
    if (key == feedback::FeedbackReport::kCrashReportIdsKey ||
        key == feedback::FeedbackReport::kAllCrashReportIdsKey)
      continue;

    if (value.find("\n") != std::string::npos) {
      syslogs_string.append(key + "=" + kMultilineIndicatorString +
                            kMultilineStartString + value + "\n" +
                            kMultilineEndString);
    } else {
      syslogs_string.append(key + "=" + value + "\n");
    }
  }
  return syslogs_string;
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

void FeedbackCommon::PrepareReport(
    userfeedback::ExtensionSubmit* feedback_data) const {
  // Unused field, needs to be 0 though.
  feedback_data->set_type_id(0);

  // Set whether we're reporting from ChromeOS or Chrome on another platform.
  userfeedback::ChromeData chrome_data;
#if defined(OS_CHROMEOS)
  const userfeedback::ChromeData_ChromePlatform chrome_platform =
      userfeedback::ChromeData_ChromePlatform_CHROME_OS;
  const int default_product_id = kChromeOSProductId;
  userfeedback::ChromeOsData chrome_os_data;
  chrome_os_data.set_category(
      userfeedback::ChromeOsData_ChromeOsCategory_OTHER);
  *(chrome_data.mutable_chrome_os_data()) = chrome_os_data;
#else
  const userfeedback::ChromeData_ChromePlatform chrome_platform =
      userfeedback::ChromeData_ChromePlatform_CHROME_BROWSER;
  const int default_product_id = kChromeBrowserProductId;
  userfeedback::ChromeBrowserData chrome_browser_data;
  chrome_browser_data.set_category(
      userfeedback::ChromeBrowserData_ChromeBrowserCategory_OTHER);
  *(chrome_data.mutable_chrome_browser_data()) = chrome_browser_data;
#endif  // defined(OS_CHROMEOS)
  chrome_data.set_chrome_platform(chrome_platform);
  *(feedback_data->mutable_chrome_data()) = chrome_data;

  feedback_data->set_product_id(HasProductId() ? product_id_
                                               : default_product_id);

  userfeedback::CommonData* common_data = feedback_data->mutable_common_data();
  // We're not using gaia ids, we're using the e-mail field instead.
  common_data->set_gaia_id(0);
  common_data->set_user_email(user_email());
  common_data->set_description(description());
  common_data->set_source_description_language(locale());

  userfeedback::WebData* web_data = feedback_data->mutable_web_data();
  web_data->set_url(page_url());
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
}

FeedbackCommon::~FeedbackCommon() {}

void FeedbackCommon::CompressFile(const base::FilePath& filename,
                                  const std::string& zipname,
                                  std::string data_to_be_compressed) {
  std::string compressed_data;
  if (feedback_util::ZipString(filename, std::move(data_to_be_compressed),
                               &compressed_data)) {
    std::string attachment_file_name = zipname;
    if (attachment_file_name.empty()) {
      // We need to use the UTF8Unsafe methods here to accommodate Windows,
      // which uses wide strings to store file paths.
      attachment_file_name = filename.BaseName().AsUTF8Unsafe().append(kZipExt);
    }

    AddFile(attachment_file_name, std::move(compressed_data));
  }
}

void FeedbackCommon::CompressLogs() {
  std::string logs = LogsToString(logs_);
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

  for (const auto& iter : logs_) {
    if (BelowCompressionThreshold(iter.second)) {
      // We only send the list of all the crash report IDs if the user has a
      // @google.com email. We do this also in feedback_private_api, but not all
      // code paths go through that so we need to check again here.
      if (iter.first == feedback::FeedbackReport::kAllCrashReportIdsKey &&
          !feedback_util::IsGoogleEmail(user_email())) {
        continue;
      }

      // Small enough logs should end up in the report data itself. However,
      // they're still added as part of the system_logs.zip file.
      AddFeedbackData(feedback_data, iter.first, iter.second);
    }
  }
}
