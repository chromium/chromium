// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/feedback/feedback_util.h"

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/feedback/feedback_report.h"
#include "third_party/zlib/google/zip.h"

namespace {

constexpr char kMultilineIndicatorString[] = "<multiline>\n";
constexpr char kMultilineStartString[] = "---------- START ----------\n";
constexpr char kMultilineEndString[] = "---------- END ----------\n\n";

}  // namespace

namespace feedback_util {

std::optional<std::string> ZipString(const base::FilePath& filename,
                                     std::string_view data) {
  base::ScopedTempDir temp_dir;
  base::FilePath zip_file;

  // Create a temporary directory, put the logs into a file in it. Create
  // another temporary file to receive the zip file in.
  if (!temp_dir.CreateUniqueTempDir()) {
    return std::nullopt;
  }
  if (!base::WriteFile(temp_dir.GetPath().Append(filename), data)) {
    return std::nullopt;
  }
  if (!base::CreateTemporaryFile(&zip_file)) {
    return std::nullopt;
  }

  std::string compressed_logs;
  bool succeed = zip::Zip(temp_dir.GetPath(), zip_file, false) &&
                 base::ReadFileToString(zip_file, &compressed_logs);
  base::DeleteFile(zip_file);
  if (!succeed) {
    return std::nullopt;
  }
  return compressed_logs;
}

std::string LogsToString(const FeedbackCommon::SystemLogsMap& sys_info) {
  std::string syslogs_string;
  for (const auto& iter : sys_info) {
    std::string key = iter.first;
    base::TrimString(key, "\n ", &key);

    if (key == feedback::FeedbackReport::kCrashReportIdsKey ||
        key == feedback::FeedbackReport::kAllCrashReportIdsKey) {
      // Avoid adding the crash IDs to the system_logs.txt file for privacy
      // reasons. They should just be part of the product specific data.
      continue;
    }

    if (key == feedback::FeedbackReport::kFeedbackUserCtlConsentKey) {
      // Avoid adding user consent to the system_logs.txt file. It just needs to
      // be in the product specific data.
      continue;
    }

    std::string value = iter.second;
    base::TrimString(value, "\n ", &value);
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

void RemoveUrlsFromAutofillData(std::string& autofill_metadata) {
  std::optional<base::Value::Dict> autofill_data = base::JSONReader::ReadDict(
      autofill_metadata, base::JSON_ALLOW_TRAILING_COMMAS);

  if (!autofill_data) {
    LOG(ERROR) << "base::JSONReader::Read failed to translate to JSON";
    return;
  }

  if (base::Value::List* form_structures =
          autofill_data->FindList("formStructures")) {
    for (base::Value& item : *form_structures) {
      auto& dict = item.GetDict();
      dict.Remove("sourceUrl");
      dict.Remove("mainFrameUrl");
    }
  }
  base::JSONWriter::Write(*autofill_data, &autofill_metadata);
  return;
}

// Note: This function is excluded from win build because its unit tests do
// not pass on OS_WIN.
// This function is only called on ChromeOS.
// See https://crbug.com/1119560.
#if !BUILDFLAG(IS_WIN)
std::optional<std::string> ReadEndOfFile(const base::FilePath& path,
                                         size_t max_size) {
  if (path.ReferencesParent()) {
    LOG(ERROR) << "ReadEndOfFile can't be called on file paths with parent "
                  "references.";
    return std::nullopt;
  }

  base::ScopedFILE fp(base::OpenFile(path, "r"));
  if (!fp) {
    PLOG(ERROR) << "Failed to open file " << path.value();
    return std::nullopt;
  }

  std::vector<char> chunk(max_size);
  std::vector<char> last_chunk(max_size);

  size_t total_bytes_read = 0;
  size_t bytes_read = 0;

  // Since most logs are not seekable, read until the end keeping tracking of
  // last two chunks.
  while ((bytes_read = fread(chunk.data(), 1, max_size, fp.get())) ==
         max_size) {
    total_bytes_read += bytes_read;
    last_chunk.swap(chunk);
    chunk[0] = '\0';
  }
  total_bytes_read += bytes_read;
  std::string contents;
  if (total_bytes_read < max_size) {
    // File is smaller than max_size
    contents.assign(chunk.data(), bytes_read);
  } else if (bytes_read == 0) {
    // File is exactly max_size or a multiple of max_size
    contents.assign(last_chunk.data(), max_size);
  } else {
    // Number of bytes to keep from last_chunk
    size_t bytes_from_last = max_size - bytes_read;

    // Shift left last_chunk by size of chunk and fit it in the back of
    // last_chunk.
    memmove(last_chunk.data(), last_chunk.data() + bytes_read, bytes_from_last);
    memcpy(last_chunk.data() + bytes_from_last, chunk.data(), bytes_read);

    contents.assign(last_chunk.data(), max_size);
  }

  return contents;
}
#endif  // !BUILDFLAG(IS_WIN)

}  // namespace feedback_util
