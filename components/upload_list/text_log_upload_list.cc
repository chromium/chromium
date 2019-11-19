// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/upload_list/text_log_upload_list.h"

#include <algorithm>
#include <sstream>

#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace {

constexpr size_t kUploadTimeIndex = 0;
constexpr size_t kCaptureTimeIndex = 3;

std::vector<std::string> SplitIntoLines(const std::string& file_contents) {
  return base::SplitString(file_contents, base::kWhitespaceASCII,
                           base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

std::vector<std::string> SplitIntoComponents(const std::string& line) {
  return base::SplitString(line, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_ALL);
}

}  // namespace

TextLogUploadList::TextLogUploadList(const base::FilePath& upload_log_path)
    : upload_log_path_(upload_log_path) {}

TextLogUploadList::~TextLogUploadList() = default;

std::vector<UploadList::UploadInfo> TextLogUploadList::LoadUploadList() {
  std::vector<UploadInfo> uploads;

  if (base::PathExists(upload_log_path_)) {
    std::string contents;
    base::ReadFileToString(upload_log_path_, &contents);
    ParseLogEntries(SplitIntoLines(contents), &uploads);
  }

  return uploads;
}

void TextLogUploadList::ClearUploadList(const base::Time& begin,
                                        const base::Time& end) {
  if (!base::PathExists(upload_log_path_))
    return;

  std::string contents;
  base::ReadFileToString(upload_log_path_, &contents);
  std::vector<std::string> log_entries = SplitIntoLines(contents);

  std::ostringstream new_contents_stream;
  for (const std::string& line : log_entries) {
    std::vector<std::string> components = SplitIntoComponents(line);
    double seconds_since_epoch;
    if (components.size() > kUploadTimeIndex &&
        !components[kUploadTimeIndex].empty() &&
        base::StringToDouble(components[kUploadTimeIndex],
                             &seconds_since_epoch)) {
      base::Time upload_time = base::Time::FromDoubleT(seconds_since_epoch);
      if (begin <= upload_time && upload_time <= end)
        continue;
    }

    if (components.size() > kCaptureTimeIndex &&
        !components[kCaptureTimeIndex].empty() &&
        base::StringToDouble(components[kCaptureTimeIndex],
                             &seconds_since_epoch)) {
      base::Time capture_time = base::Time::FromDoubleT(seconds_since_epoch);
      if (begin <= capture_time && capture_time <= end)
        continue;
    }

    new_contents_stream << line << std::endl;
  }

  std::string new_contents = new_contents_stream.str();
  if (new_contents.size() == 0) {
    base::DeleteFile(upload_log_path_, /*recursive*/ false);
  } else {
    base::WriteFile(upload_log_path_, new_contents.c_str(),
                    new_contents.size());
  }
}

void TextLogUploadList::ParseLogEntries(
    const std::vector<std::string>& log_entries,
    std::vector<UploadInfo>* uploads) {
  std::vector<std::string>::const_reverse_iterator i;
  for (i = log_entries.rbegin(); i != log_entries.rend(); ++i) {
    const std::string& line = *i;
    std::vector<std::string> components = SplitIntoComponents(line);
    // Skip any blank (or corrupted) lines.
    if (components.size() < 2 || components.size() > 5)
      continue;
    base::Time upload_time;
    double seconds_since_epoch;
    if (!components[kUploadTimeIndex].empty()) {
      if (!base::StringToDouble(components[kUploadTimeIndex],
                                &seconds_since_epoch))
        continue;
      upload_time = base::Time::FromDoubleT(seconds_since_epoch);
    }
    UploadInfo info(components[1], upload_time);

    // Add local ID if present.
    if (components.size() > 2)
      info.local_id = components[2];

    // Add capture time if present.
    if (components.size() > kCaptureTimeIndex &&
        !components[kCaptureTimeIndex].empty() &&
        base::StringToDouble(components[kCaptureTimeIndex],
                             &seconds_since_epoch)) {
      info.capture_time = base::Time::FromDoubleT(seconds_since_epoch);
    }

    int state;
    if (components.size() > 4 && !components[4].empty() &&
        base::StringToInt(components[4], &state)) {
      info.state = static_cast<UploadInfo::State>(state);
    }

    uploads->push_back(info);
  }
}
