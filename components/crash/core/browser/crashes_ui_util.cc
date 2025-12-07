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
#include "components/upload_list/upload_list.h"

namespace crash_reporter {

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

  NOTREACHED();
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
