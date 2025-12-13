// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/focus/focus_result_file_writer.h"

#include <string_view>

#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ui/startup/focus/focus_handler.h"

namespace focus {

namespace {

int FocusResultToExitCode(const FocusResult& result) {
  switch (result.status) {
    case FocusStatus::kFocused:
      return 0;
    case FocusStatus::kOpenedFallback:
      return 0;
    case FocusStatus::kNoMatch:
      return 1;
    case FocusStatus::kParseError:
      return 2;
  }
  return 1;
}

std::string_view FocusResultToString(const FocusResult& result) {
  switch (result.status) {
    case FocusStatus::kFocused:
      return "focused";
    case FocusStatus::kOpenedFallback:
      return "opened";
    case FocusStatus::kNoMatch:
      return "no_match";
    case FocusStatus::kParseError:
      return "parse_error";
  }
  return "unknown";
}

}  // namespace

std::string CreateFocusJsonString(const FocusResult& result) {
  base::Value::Dict json_dict;

  json_dict.Set("status", FocusResultToString(result));

  // Add error details for parse errors.
  if (result.status == FocusStatus::kParseError) {
    switch (result.error_type) {
      case FocusResult::Error::kEmptySelector:
        json_dict.Set("error", "Empty selector string");
        break;
      case FocusResult::Error::kInvalidFormat:
        json_dict.Set("error", "Invalid selector format");
        break;
      default:
        json_dict.Set("error", "Parse error");
        break;
    }
  }

  json_dict.Set("exit_code", FocusResultToExitCode(result));

  std::string json_string;
  base::JSONWriter::Write(json_dict, &json_string);
  return json_string;
}

void WriteResultToFile(std::string file_path, const FocusResult& result) {
  std::string json_string = CreateFocusJsonString(result);

  // Write to file on a background thread asynchronously.
  // This is called from the persistent browser process (not the CLI process
  // that exits), so async write is safe. The file will be written within
  // milliseconds, fast enough for any automation.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          [](const std::string& path, const std::string& content) {
            base::FilePath file_path = base::FilePath::FromUTF8Unsafe(path);
            base::WriteFile(file_path, content);
          },
          std::move(file_path), std::move(json_string)));
}

}  // namespace focus
