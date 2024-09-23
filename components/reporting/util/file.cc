// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/file.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/expected.h"
#include "components/reporting/util/reporting_errors.h"

namespace reporting {

bool DeleteFileWarnIfFailed(const base::FilePath& path) {
  const auto delete_result = base::DeleteFile(path);
  if (!delete_result) {
    LOG(WARNING) << "Failed to delete " << path.MaybeAsASCII();
  }
  return delete_result;
}

bool DeleteFilesWarnIfFailed(
    base::FileEnumerator& dir_enum,
    base::RepeatingCallback<bool(const base::FilePath&)> pred) {
  std::vector<base::FilePath> files_to_delete;
  for (auto full_name = dir_enum.Next(); !full_name.empty();
       full_name = dir_enum.Next()) {
    if (pred.Run(full_name)) {
      files_to_delete.push_back(std::move(full_name));
    }
  }

  // Starting from deeper paths so that directories are always emptied first if
  // the files there are to be deleted. This can be done by deleting the file
  // with the longest full paths first.
  std::sort(files_to_delete.begin(), files_to_delete.end(),
            [](const base::FilePath& fp0, const base::FilePath& fp1) {
              // Use size of the file path string is sufficient. Semantically it
              // is better to use the number of components in a file path
              // (GetComponents().size()), but this is more efficient.
              return fp0.value().size() > fp1.value().size();
            });
  bool success = true;
  for (const auto& file_to_delete : files_to_delete) {
    if (!DeleteFileWarnIfFailed(file_to_delete)) {
      success = false;
    }
  }
  return success;
}

bool DeleteFilesWarnIfFailed(
    base::FileEnumerator&& dir_enum,
    base::RepeatingCallback<bool(const base::FilePath&)> pred) {
  return DeleteFilesWarnIfFailed(dir_enum, pred);
}

StatusOr<std::string> MaybeReadFile(const base::FilePath& file_path,
                                    int64_t offset) {
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                  DataLossErrorReason::FAILED_TO_OPEN_FILE,
                                  DataLossErrorReason::MAX_VALUE);
    return base::unexpected(Status(
        error::NOT_FOUND, base::StrCat({"Could not open health data file ",
                                        file_path.MaybeAsASCII()})));
  }

  base::File::Info file_info;
  if (!file.GetInfo(&file_info) || file_info.size - offset < 0) {
    base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                  DataLossErrorReason::FAILED_TO_READ_FILE_INFO,
                                  DataLossErrorReason::MAX_VALUE);
    return base::unexpected(
        Status(error::DATA_LOSS, base::StrCat({"Failed to read data file info ",
                                               file_path.MaybeAsASCII()})));
  }

  std::string result;
  result.resize(file_info.size - offset);
  const int read_result =
      UNSAFE_TODO(file.Read(offset, result.data(), file_info.size - offset));
  if (read_result != file_info.size - offset) {
    base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                  DataLossErrorReason::FAILED_TO_READ_FILE,
                                  DataLossErrorReason::MAX_VALUE);
    return base::unexpected(Status(
        error::DATA_LOSS,
        base::StrCat({"Failed to read data file ", file_path.MaybeAsASCII()})));
  }

  return result;
}

Status AppendLine(const base::FilePath& file_path,
                  const std::string_view& data) {
  base::File file(file_path,
                  base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
  if (!file.IsValid()) {
    base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                  DataLossErrorReason::FAILED_TO_OPEN_FILE,
                                  DataLossErrorReason::MAX_VALUE);
    return Status(error::NOT_FOUND,
                  base::StrCat({"Could not open health data file ",
                                file_path.MaybeAsASCII()}));
  }

  const std::string line = base::StrCat({data, "\n"});
  const int write_count = UNSAFE_TODO(file.Write(0, line.data(), line.size()));
  if (write_count < 0 || static_cast<size_t>(write_count) < line.size()) {
    base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                  DataLossErrorReason::FAILED_TO_WRITE_FILE,
                                  DataLossErrorReason::MAX_VALUE);
    return Status(error::DATA_LOSS,
                  base::StrCat({"Failed to write health data file ",
                                file_path.MaybeAsASCII(), " write count=",
                                base::NumberToString(write_count)}));
  }
  return Status::StatusOK();
}

StatusOr<uint32_t> RemoveAndTruncateLine(const base::FilePath& file_path,
                                         uint32_t pos) {
  StatusOr<std::string> status_or = MaybeReadFile(file_path, pos);
  if (!status_or.has_value()) {
    return base::unexpected(std::move(status_or).error());
  }
  std::string content = status_or.value();
  uint32_t offset = 0;
  // Search for next new line after pos.
  while (offset < content.length()) {
    if (content.at(offset++) == '\n') {
      break;
    }
  }

  // Check if the last line was removed.
  if (offset >= content.length()) {
    content = "";
  } else {
    content = content.substr(offset);
  }

  Status status = MaybeWriteFile(file_path, content);
  if (!status.ok()) {
    return base::unexpected(std::move(status));
  }
  return pos + offset;
}

Status MaybeWriteFile(const base::FilePath& file_path,
                      const std::string_view& data) {
  base::File file(file_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    return Status(error::NOT_FOUND, base::StrCat({"Could not open data file ",
                                                  file_path.MaybeAsASCII()}));
  }

  const int write_count = UNSAFE_TODO(file.Write(0, data.data(), data.size()));
  if (write_count < 0 || static_cast<size_t>(write_count) < data.size()) {
    base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                  DataLossErrorReason::FAILED_TO_WRITE_FILE,
                                  DataLossErrorReason::MAX_VALUE);
    return Status(
        error::DATA_LOSS,
        base::StrCat({"Failed to write data file ", file_path.MaybeAsASCII(),
                      " write count=", base::NumberToString(write_count)}));
  }

  return Status::StatusOK();
}
}  // namespace reporting
