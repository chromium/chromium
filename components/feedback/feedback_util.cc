// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_util.h"

#include <string>

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

bool ZipString(const base::FilePath& filename,
               const std::string& data,
               std::string* compressed_logs) {
  base::ScopedTempDir temp_dir;
  base::FilePath zip_file;

  // Create a temporary directory, put the logs into a file in it. Create
  // another temporary file to receive the zip file in.
  if (!temp_dir.CreateUniqueTempDir())
    return false;
  if (!base::WriteFile(temp_dir.GetPath().Append(filename), data)) {
    return false;
  }

  bool succeed = base::CreateTemporaryFile(&zip_file) &&
                 zip::Zip(temp_dir.GetPath(), zip_file, false) &&
                 base::ReadFileToString(zip_file, compressed_logs);

  base::DeleteFile(zip_file);

  return succeed;
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
  absl::optional<base::Value> properties = base::JSONReader::Read(
      autofill_metadata, base::JSON_ALLOW_TRAILING_COMMAS);

  if (!properties || !properties->is_dict()) {
    LOG(ERROR) << "base::JSONReader::Read failed to translate to JSON";
    return;
  }

  base::Value::Dict& autofill_data = properties->GetDict();
  if (base::Value::List* form_structures =
          autofill_data.FindList("formStructures")) {
    for (base::Value& item : *form_structures) {
      item.RemoveKey("sourceUrl");
      item.RemoveKey("mainFrameUrl");
    }
  }
  base::JSONWriter::Write(properties.value(), &autofill_metadata);
  return;
}

// Note: This function is excluded from win build because its unit tests do
// not pass on OS_WIN.
// This function is only called on ChromeOS and Lacros build.
// See https://crbug.com/1119560.
#if !BUILDFLAG(IS_WIN)
bool ReadEndOfFile(const base::FilePath& path,
                   size_t max_size,
                   std::string* contents) {
  if (!contents) {
    LOG(ERROR) << "contents buffer is null.";
    return false;
  }

  if (path.ReferencesParent()) {
    LOG(ERROR) << "ReadEndOfFile can't be called on file paths with parent "
                  "references.";
    return false;
  }

  base::ScopedFILE fp(base::OpenFile(path, "r"));
  if (!fp) {
    PLOG(ERROR) << "Failed to open file " << path.value();
    return false;
  }

  std::unique_ptr<char[]> chunk(new char[max_size]);
  std::unique_ptr<char[]> last_chunk(new char[max_size]);
  chunk[0] = '\0';
  last_chunk[0] = '\0';

  size_t total_bytes_read = 0;
  size_t bytes_read = 0;

  // Since most logs are not seekable, read until the end keeping tracking of
  // last two chunks.
  while ((bytes_read = fread(chunk.get(), 1, max_size, fp.get())) == max_size) {
    total_bytes_read += bytes_read;
    last_chunk.swap(chunk);
    chunk[0] = '\0';
  }
  total_bytes_read += bytes_read;
  if (total_bytes_read < max_size) {
    // File is smaller than max_size
    contents->assign(chunk.get(), bytes_read);
  } else if (bytes_read == 0) {
    // File is exactly max_size or a multiple of max_size
    contents->assign(last_chunk.get(), max_size);
  } else {
    // Number of bytes to keep from last_chunk
    size_t bytes_from_last = max_size - bytes_read;

    // Shift left last_chunk by size of chunk and fit it in the back of
    // last_chunk.
    memmove(last_chunk.get(), last_chunk.get() + bytes_read, bytes_from_last);
    memcpy(last_chunk.get() + bytes_from_last, chunk.get(), bytes_read);

    contents->assign(last_chunk.get(), max_size);
  }

  return true;
}
#endif  // !BUILDFLAG(IS_WIN)

}  // namespace feedback_util
