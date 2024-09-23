// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/dump_info.h"

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>

#include <string_view>

#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"

namespace chromecast {

namespace {

const int kNumRequiredParams = 4;

const char kNameKey[] = "name";
const char kDumpTimeKey[] = "dump_time";
const char kDumpKey[] = "dump";
const char kUptimeKey[] = "uptime";
const char kLogfileKey[] = "logfile";
const char kAttachmentsKey[] = "attachments";
const char kSuffixKey[] = "suffix";
const char kPrevAppNameKey[] = "prev_app_name";
const char kCurAppNameKey[] = "cur_app_name";
const char kLastAppNameKey[] = "last_app_name";
const char kReleaseVersionKey[] = "release_version";
const char kBuildNumberKey[] = "build_number";
const char kReasonKey[] = "reason";
const char kStadiaSessionIdKey[] = "stadia_session_id";
const char kCrashProductNameKey[] = "crash_product_name";
const char kExecNameKey[] = "exec_name";
const char kSignatureKey[] = "signature";
const char kExtraInfoKey[] = "extra_info";

// CastLite specific data
const char kComments[] = "comments";
const char kJsEngine[] = "js_engine";
const char kJsBuildLabel[] = "js_build_label";
const char kJsExceptionCategory[] = "js_exception_category";
const char kJsExceptionDetails[] = "js_exception_details";
const char kJsExceptionSignature[] = "js_exception_signature";

// Convenience wrapper around Value::Dict::FindString(), for easier use in if
// statements. If `key` is a string in `dict`, writes it to `out` and returns
// true. Leaves `out` alone and returns false otherwise.
bool FindString(const base::Value::Dict& dict,
                std::string_view key,
                std::string& out) {
  const std::string* value = dict.FindString(key);
  if (!value)
    return false;
  out = *value;
  return true;
}

}  // namespace

DumpInfo::DumpInfo(const base::Value* entry) : valid_(ParseEntry(entry)) {}

DumpInfo::DumpInfo(const std::string& crashed_process_dump,
                   const std::string& crashed_process_logfile,
                   const base::Time& dump_time,
                   const MinidumpParams& params,
                   const std::vector<std::string>* attachments)
    : crashed_process_dump_(crashed_process_dump),
      logfile_(crashed_process_logfile),
      dump_time_(dump_time),
      params_(params),
      valid_(true) {
  if (attachments) {
    attachments_ = *attachments;
  }
}

DumpInfo::~DumpInfo() {}

base::Value DumpInfo::GetAsValue() const {
  base::Value::Dict result;

  result.Set(kDumpTimeKey, base::UnlocalizedTimeFormatWithPattern(
                               dump_time_, "yyyy-MM-dd HH:mm:ss"));

  result.Set(kDumpKey, crashed_process_dump_);
  std::string uptime = std::to_string(params_.process_uptime);
  result.Set(kUptimeKey, uptime);
  result.Set(kLogfileKey, logfile_);

  base::Value::List attachments_list;
  for (const auto& attachment : attachments_) {
    attachments_list.Append(attachment);
  }
  result.Set(kAttachmentsKey, std::move(attachments_list));
  result.Set(kSuffixKey, params_.suffix);
  result.Set(kPrevAppNameKey, params_.previous_app_name);
  result.Set(kCurAppNameKey, params_.current_app_name);
  result.Set(kLastAppNameKey, params_.last_app_name);
  result.Set(kReleaseVersionKey, params_.cast_release_version);
  result.Set(kBuildNumberKey, params_.cast_build_number);
  result.Set(kReasonKey, params_.reason);
  result.Set(kStadiaSessionIdKey, params_.stadia_session_id);
  result.Set(kExecNameKey, params_.exec_name);
  result.Set(kSignatureKey, params_.signature);
  result.Set(kExtraInfoKey, params_.extra_info);
  result.Set(kCrashProductNameKey, params_.crash_product_name);
  result.Set(kComments, params_.comments);
  result.Set(kJsEngine, params_.js_engine);
  result.Set(kJsBuildLabel, params_.js_build_label);
  result.Set(kJsExceptionCategory, params_.js_exception_category);
  result.Set(kJsExceptionDetails, params_.js_exception_details);
  result.Set(kJsExceptionSignature, params_.js_exception_signature);

  return base::Value(std::move(result));
}

bool DumpInfo::ParseEntry(const base::Value* entry) {
  valid_ = false;

  if (!entry)
    return false;

  const base::Value::Dict* dict = entry->GetIfDict();
  if (!dict)
    return false;

  // Extract required fields.
  std::string dump_time;
  if (!FindString(*dict, kDumpTimeKey, dump_time))
    return false;
  if (!SetDumpTimeFromString(dump_time))
    return false;

  if (!FindString(*dict, kDumpKey, crashed_process_dump_))
    return false;

  std::string uptime;
  if (!FindString(*dict, kUptimeKey, uptime))
    return false;
  errno = 0;
  params_.process_uptime = strtoull(uptime.c_str(), nullptr, 0);
  if (errno != 0)
    return false;

  if (!FindString(*dict, kLogfileKey, logfile_))
    return false;
  size_t num_params = kNumRequiredParams;

  // Extract all other optional fields.
  const base::Value::List* attachments_list = dict->FindList(kAttachmentsKey);
  if (attachments_list) {
    ++num_params;
    for (const auto& attachment : *attachments_list) {
      attachments_.push_back(attachment.GetString());
    }
  }

  std::string unused_process_name;
  if (FindString(*dict, kNameKey, unused_process_name))
    ++num_params;
  if (FindString(*dict, kSuffixKey, params_.suffix))
    ++num_params;
  if (FindString(*dict, kPrevAppNameKey, params_.previous_app_name))
    ++num_params;
  if (FindString(*dict, kCurAppNameKey, params_.current_app_name))
    ++num_params;
  if (FindString(*dict, kLastAppNameKey, params_.last_app_name))
    ++num_params;
  if (FindString(*dict, kReleaseVersionKey, params_.cast_release_version))
    ++num_params;
  if (FindString(*dict, kBuildNumberKey, params_.cast_build_number))
    ++num_params;
  if (FindString(*dict, kReasonKey, params_.reason))
    ++num_params;
  if (FindString(*dict, kStadiaSessionIdKey, params_.stadia_session_id))
    ++num_params;
  if (FindString(*dict, kExecNameKey, params_.exec_name))
    ++num_params;
  if (FindString(*dict, kSignatureKey, params_.signature))
    ++num_params;
  if (FindString(*dict, kExtraInfoKey, params_.extra_info))
    ++num_params;
  if (FindString(*dict, kCrashProductNameKey, params_.crash_product_name))
    ++num_params;
  if (FindString(*dict, kComments, params_.comments)) {
    ++num_params;
  }
  if (FindString(*dict, kJsEngine, params_.js_engine)) {
    ++num_params;
  }
  if (FindString(*dict, kJsBuildLabel, params_.js_build_label)) {
    ++num_params;
  }
  if (FindString(*dict, kJsExceptionCategory, params_.js_exception_category)) {
    ++num_params;
  }
  if (FindString(*dict, kJsExceptionDetails, params_.js_exception_details)) {
    ++num_params;
  }
  if (FindString(*dict, kJsExceptionSignature,
                 params_.js_exception_signature)) {
    ++num_params;
  }

  // Disallow extraneous params
  if (dict->size() != num_params)
    return false;

  valid_ = true;
  return true;
}

bool DumpInfo::SetDumpTimeFromString(const std::string& timestr) {
  if (base::Time::FromString(timestr.c_str(), &dump_time_)) {
    return true;
  }

  LOG(INFO) << "Failed to convert dump time invalid";
  return false;
}

}  // namespace chromecast
