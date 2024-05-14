// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/browser/crashes_ui_util.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/i18n/time_formatting.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/upload_list/upload_list.h"

namespace crash_reporter {

const CrashesUILocalizedString kCrashesUILocalizedStrings[] = {
    {"bugLinkText", IDS_CRASH_BUG_LINK_LABEL},
    {"crashCaptureTimeFormat", IDS_CRASH_CAPTURE_TIME_FORMAT},
    {"crashCountFormat", IDS_CRASH_CRASH_COUNT_BANNER_FORMAT},
    {"crashStatus", IDS_CRASH_REPORT_STATUS},
    {"crashStatusNotUploaded", IDS_CRASH_REPORT_STATUS_NOT_UPLOADED},
    {"crashStatusPending", IDS_CRASH_REPORT_STATUS_PENDING},
    {"crashStatusPendingUserRequested",
     IDS_CRASH_REPORT_STATUS_PENDING_USER_REQUESTED},
    {"crashStatusUploaded", IDS_CRASH_REPORT_STATUS_UPLOADED},
    {"crashesTitle", IDS_CRASH_TITLE},
    {"disabledHeader", IDS_CRASH_DISABLED_HEADER},
    {"disabledMessage", IDS_CRASH_DISABLED_MESSAGE},
    {"fileSize", IDS_CRASH_REPORT_FILE_SIZE},
    {"localId", IDS_CRASH_REPORT_LOCAL_ID},
    {"noCrashesMessage", IDS_CRASH_NO_CRASHES_MESSAGE},
    {"showDeveloperDetails", IDS_CRASH_SHOW_DEVELOPER_DETAILS},
    {"uploadCrashesLinkText", IDS_CRASH_UPLOAD_MESSAGE},
    {"uploadId", IDS_CRASH_REPORT_UPLOADED_ID},
    {"uploadNowLinkText", IDS_CRASH_UPLOAD_NOW_LINK_TEXT},
    {"uploadTime", IDS_CRASH_REPORT_UPLOADED_TIME},
};

const size_t kCrashesUILocalizedStringsCount =
    std::size(kCrashesUILocalizedStrings);

const char kCrashesUICrashesJS[] = "crashes.js";
const char kCrashesUICrashesCSS[] = "crashes.css";
const char kCrashesUISadTabSVG[] = "sadtab.svg";
const char kCrashesUIRequestCrashList[] = "requestCrashList";
const char kCrashesUIRequestCrashUpload[] = "requestCrashUpload";
const char kCrashesUIShortProductName[] = "shortProductName";
const char kCrashesUIUpdateCrashList[] = "update-crash-list";
const char kCrashesUIRequestSingleCrashUpload[] = "requestSingleCrashUpload";

std::string UploadInfoStateAsString(UploadList::UploadInfo::State state) {
  switch (state) {
    case UploadList::UploadInfo::State::NotUploaded:
      return "not_uploaded";
    case UploadList::UploadInfo::State::Pending:
      return "pending";
    case UploadList::UploadInfo::State::Pending_UserRequested:
      return "pending_user_requested";
    case UploadList::UploadInfo::State::Uploaded:
      return "uploaded";
  }

  NOTREACHED_IN_MIGRATION();
  return "";
}

void UploadListToValue(UploadList* upload_list, base::Value::List* out_value) {
  const std::vector<const UploadList::UploadInfo*> crashes =
      upload_list->GetUploads(50);

  for (const auto* info : crashes) {
    base::Value::Dict crash;
    crash.Set("id", info->upload_id);
    if (info->state == UploadList::UploadInfo::State::Uploaded) {
      crash.Set("upload_time",
                base::UTF16ToUTF8(
                    base::TimeFormatFriendlyDateAndTime(info->upload_time)));
    }
    if (!info->capture_time.is_null()) {
      crash.Set("capture_time",
                base::UTF16ToUTF8(
                    base::TimeFormatFriendlyDateAndTime(info->capture_time)));
    }
    crash.Set("local_id", info->local_id);
    crash.Set("state", UploadInfoStateAsString(info->state));
    if (info->file_size.has_value()) {
      crash.Set("file_size", static_cast<double>(*info->file_size));
    }
    out_value->Append(std::move(crash));
  }
}

}  // namespace crash_reporter
