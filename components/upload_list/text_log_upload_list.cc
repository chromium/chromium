// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/upload_list/text_log_upload_list.h"

#include <algorithm>
#include <optional>
#include <sstream>
#include <utility>

#include "base/containers/adapters.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"

namespace {

constexpr size_t kUploadTimeIndex = 0;
constexpr size_t kCaptureTimeIndex = 3;

constexpr char kJsonLogKeyUploadId[] = "upload_id";
constexpr char kJsonLogKeyUploadTime[] = "upload_time";
constexpr char kJsonLogKeyLocalId[] = "local_id";
constexpr char kJsonLogKeyCaptureTime[] = "capture_time";
constexpr char kJsonLogKeyState[] = "state";
constexpr char kJsonLogKeySource[] = "source";
constexpr char kJsonLogKeyPathHash[] = "path_hash";

std::vector<std::string> SplitIntoComponents(const std::string& line) {
  return base::SplitString(line, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_ALL);
}

bool CheckCsvUploadListOutOfRange(const std::string& line,
                                  const base::Time& begin,
                                  const base::Time& end) {
  std::vector<std::string> components = SplitIntoComponents(line);
  double seconds_since_epoch;
  if (components.size() > kUploadTimeIndex &&
      !components[kUploadTimeIndex].empty() &&
      base::StringToDouble(components[kUploadTimeIndex],
                           &seconds_since_epoch)) {
    base::Time upload_time =
        base::Time::FromSecondsSinceUnixEpoch(seconds_since_epoch);
    if (begin <= upload_time && upload_time <= end)
      return false;
  }

  if (components.size() > kCaptureTimeIndex &&
      !components[kCaptureTimeIndex].empty() &&
      base::StringToDouble(components[kCaptureTimeIndex],
                           &seconds_since_epoch)) {
    base::Time capture_time =
        base::Time::FromSecondsSinceUnixEpoch(seconds_since_epoch);
    if (begin <= capture_time && capture_time <= end)
      return false;
  }

  return true;
}

bool CheckFieldOutOfRange(const std::string* time_string,
                          const base::Time& begin,
                          const base::Time& end) {
  if (time_string) {
    double upload_time_double = 0.0;
    if (base::StringToDouble(*time_string, &upload_time_double)) {
      base::Time upload_time =
          base::Time::FromSecondsSinceUnixEpoch(upload_time_double);
      if (begin <= upload_time && upload_time <= end)
        return false;
    }
  }
  return true;
}

bool CheckJsonUploadListOutOfRange(const base::Value::Dict& dict,
                                   const base::Time& begin,
                                   const base::Time& end) {
  const std::string* upload_time_string =
      dict.FindString(kJsonLogKeyUploadTime);

  const std::string* capture_time_string =
      dict.FindString(kJsonLogKeyCaptureTime);

  return CheckFieldOutOfRange(upload_time_string, begin, end) &&
         CheckFieldOutOfRange(capture_time_string, begin, end);
}
}  // namespace

// static
std::vector<std::string> TextLogUploadList::SplitIntoLines(
    const std::string& file_contents) {
  return base::SplitString(file_contents, base::kWhitespaceASCII,
                           base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

TextLogUploadList::TextLogUploadList(const base::FilePath& upload_log_path)
    : upload_log_path_(upload_log_path) {}

TextLogUploadList::~TextLogUploadList() = default;

std::vector<std::unique_ptr<UploadList::UploadInfo>>
TextLogUploadList::LoadUploadList() {
  std::vector<std::unique_ptr<UploadInfo>> uploads;

  if (base::PathExists(upload_log_path_)) {
    std::string contents;
    base::ReadFileToString(upload_log_path_, &contents);
    ParseLogEntries(SplitIntoLines(contents), &uploads);
  }

  return uploads;
}

void TextLogUploadList::ClearUploadList(const base::Time& begin,
                                        const base::Time& end) {
  if (!base::PathExists(upload_log_path_)) {
    return;
  }

  std::string contents;
  base::ReadFileToString(upload_log_path_, &contents);
  std::vector<std::string> log_entries = SplitIntoLines(contents);

  std::ostringstream new_contents_stream;
  for (const std::string& line : log_entries) {
    std::optional<base::Value> json = base::JSONReader::Read(line);
    bool should_copy = false;

    if (json.has_value()) {
      should_copy = json->is_dict() && CheckJsonUploadListOutOfRange(
                                           json.value().GetDict(), begin, end);
    } else {
      should_copy = CheckCsvUploadListOutOfRange(line, begin, end);
    }

    if (should_copy) {
      new_contents_stream << line << std::endl;
    }
  }

  std::string new_contents = new_contents_stream.str();
  if (new_contents.size() == 0) {
    base::DeleteFile(upload_log_path_);
  } else {
    base::WriteFile(upload_log_path_, new_contents);
  }
}

std::unique_ptr<UploadList::UploadInfo> TextLogUploadList::TryParseCsvLogEntry(
    const std::string& log_line) {
  std::vector<std::string> components = SplitIntoComponents(log_line);
  // Skip any blank (or corrupted) lines.
  if (components.size() < 2 || components.size() > 5)
    return nullptr;
  base::Time upload_time;
  double seconds_since_epoch;
  if (!components[kUploadTimeIndex].empty()) {
    if (!base::StringToDouble(components[kUploadTimeIndex],
                              &seconds_since_epoch))
      return nullptr;
    upload_time = base::Time::FromSecondsSinceUnixEpoch(seconds_since_epoch);
  }
  auto info = std::make_unique<TextLogUploadList::UploadInfo>(components[1],
                                                              upload_time);

  // Add local ID if present.
  if (components.size() > 2)
    info->local_id = components[2];

  // Add capture time if present.
  if (components.size() > kCaptureTimeIndex &&
      !components[kCaptureTimeIndex].empty() &&
      base::StringToDouble(components[kCaptureTimeIndex],
                           &seconds_since_epoch)) {
    info->capture_time =
        base::Time::FromSecondsSinceUnixEpoch(seconds_since_epoch);
  }

  int state;
  if (components.size() > 4 && !components[4].empty() &&
      base::StringToInt(components[4], &state)) {
    info->state = static_cast<UploadList::UploadInfo::State>(state);
  }

  return info;
}

std::unique_ptr<UploadList::UploadInfo> TextLogUploadList::TryParseJsonLogEntry(
    const base::Value::Dict& dict) {
  // Parse upload_id.
  const base::Value* upload_id_value = dict.Find(kJsonLogKeyUploadId);
  if (upload_id_value && !upload_id_value->is_string())
    return nullptr;

  // Parse upload_time.
  const std::string* upload_time_string =
      dict.FindString(kJsonLogKeyUploadTime);
  double upload_time_double = 0.0;
  if (upload_time_string &&
      !base::StringToDouble(*upload_time_string, &upload_time_double))
    return nullptr;

  auto info = std::make_unique<TextLogUploadList::UploadInfo>(
      upload_id_value ? upload_id_value->GetString() : std::string(),
      base::Time::FromSecondsSinceUnixEpoch(upload_time_double));

  // Parse local_id.
  const std::string* local_id = dict.FindString(kJsonLogKeyLocalId);
  if (local_id)
    info->local_id = *local_id;

  // Parse capture_time.
  const std::string* capture_time_string =
      dict.FindString(kJsonLogKeyCaptureTime);
  double capture_time_double = 0.0;
  if (capture_time_string &&
      base::StringToDouble(*capture_time_string, &capture_time_double))
    info->capture_time =
        base::Time::FromSecondsSinceUnixEpoch(capture_time_double);

  // Parse state.
  std::optional<int> state = dict.FindInt(kJsonLogKeyState);
  if (state.has_value())
    info->state = static_cast<UploadList::UploadInfo::State>(state.value());

  // Parse source.
  if (const std::string* source = dict.FindString(kJsonLogKeySource); source) {
    info->source = *source;
  }

  // Parse path hash.
  if (const std::string* path_hash = dict.FindString(kJsonLogKeyPathHash);
      path_hash) {
    info->path_hash = *path_hash;
  }

  return info;
}

void TextLogUploadList::ParseLogEntries(
    const std::vector<std::string>& log_entries,
    std::vector<std::unique_ptr<UploadList::UploadInfo>>* uploads) {
  for (const std::string& line : base::Reversed(log_entries)) {
    std::unique_ptr<UploadList::UploadInfo> info;
    std::optional<base::Value> json = base::JSONReader::Read(line);

    if (json.has_value() && json->is_dict())
      info = TryParseJsonLogEntry(json.value().GetDict());
    else
      info = TryParseCsvLogEntry(line);

    if (info)
      uploads->push_back(std::move(info));
  }
}

void TextLogUploadList::RequestSingleUpload(const std::string& local_id) {
  // Do nothing.
}
