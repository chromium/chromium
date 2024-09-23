// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/health/health_module_files.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/reporting/util/reporting_errors.h"
#include "third_party/re2/src/re2/re2.h"

namespace reporting {

std::unique_ptr<HealthModuleFiles> HealthModuleFiles::Create(
    const base::FilePath& directory,
    std::string_view file_base_name,
    const uint32_t max_storage_space) {
  if (max_storage_space == 0) {
    return nullptr;
  }

  uint32_t max_file_header = 0;
  uint32_t storage_used = 0;
  std::map<uint32_t, base::FilePath> files{};
  base::FileEnumerator dir_enum(directory, /*recursive=*/false,
                                base::FileEnumerator::FILES,
                                FILE_PATH_LITERAL("*"));
  RE2 pattern(base::StrCat({".*", file_base_name, "([0-9]+).*"}));

  for (auto path = dir_enum.Next(); !path.empty(); path = dir_enum.Next()) {
    uint32_t header;
    if (!RE2::PartialMatch(path.MaybeAsASCII(), pattern, &header)) {
      continue;
    }

    max_file_header = std::max(max_file_header, header);
    files.emplace(header, path);

    auto size_result = FileSize(path);
    if (!size_result.has_value()) {
      continue;
    }
    uint32_t file_size = size_result.value();

    if (file_size > 0) {
      storage_used += file_size;
    } else if (file_size == 0) {
      base::DeleteFile(path);
      files.erase(header);
    }
  }

  // Cannot use make_unique - constructor is private.
  return base::WrapUnique(
      new HealthModuleFiles(directory, file_base_name, max_storage_space,
                            storage_used, max_file_header, std::move(files)));
}

HealthModuleFiles::HealthModuleFiles(
    const base::FilePath& directory,
    std::string_view file_base_name,
    uint32_t max_storage_space,
    uint32_t storage_used,
    uint32_t max_file_header,
    const std::map<uint32_t, base::FilePath>& files)
    : directory_(directory),
      file_base_name_(file_base_name),
      max_storage_space_(max_storage_space),
      storage_used_(storage_used),
      files_(files) {
  const auto free_result = FreeStorage(0);
  if (!free_result.ok()) {
    DVLOG(1) << "failed to initialize health files storage: "
             << free_result.error_message();
  }
}

HealthModuleFiles::~HealthModuleFiles() = default;

base::FilePath HealthModuleFiles::CreateNewFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++max_file_header_;
  base::FilePath file_path(directory_.AppendASCII(
      base::StrCat({file_base_name_, base::NumberToString(max_file_header_)})));
  files_.emplace(max_file_header_, file_path);
  return file_path;
}

void HealthModuleFiles::DeleteOldestFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (files_.empty()) {
    return;
  }

  const auto it = files_.begin();
  base::DeleteFile(it->second);
  files_.erase(it);
}

void HealthModuleFiles::PopulateHistory(ERPHealthData* data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& file : files_) {
    const auto read_result = MaybeReadFile(file.second, /*offset=*/0);
    if (!read_result.has_value()) {
      return;
    }

    const auto records = base::SplitString(
        read_result.value(), "\n", base::WhitespaceHandling::KEEP_WHITESPACE,
        base::SplitResult::SPLIT_WANT_NONEMPTY);
    for (const auto& record : records) {
      std::string bytes;
      base::HexStringToString(record, &bytes);
      data->add_history()->ParseFromString(bytes);
    }
  }
}

Status HealthModuleFiles::Write(std::string_view data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Status free_status = ReserveStorage(data.size());
  RETURN_IF_ERROR_STATUS(free_status);

  if (files_.empty()) {
    CreateNewFile();
  } else {
    ASSIGN_OR_RETURN(const uint32_t size_result,
                     FileSize(files_.rbegin()->second));
    if (size_result + data.size() > max_file_storage_) {
      CreateNewFile();
    }
  }

  // +1 for newline char.
  storage_used_ += data.size() + 1;
  return AppendLine(files_.rbegin()->second, data);
}

Status HealthModuleFiles::FreeStorage(uint32_t storage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (storage_used_ + storage <= max_storage_space_) {
    return Status::StatusOK();
  }
  uint32_t storage_to_remove = storage_used_ + storage - max_storage_space_;
  uint32_t storage_removed = 0;
  // Ensure max storage requirement is met.
  while (!files_.empty() && storage_to_remove > storage_removed) {
    const auto& file_path = files_.begin()->second;
    ASSIGN_OR_RETURN(uint32_t file_size, FileSize(file_path));

    if (file_size == 0 || file_size <= storage_to_remove - storage_removed) {
      DeleteOldestFile();
      storage_removed += file_size;
    } else {
      ASSIGN_OR_RETURN(uint32_t remove_result,
                       RemoveAndTruncateLine(
                           file_path, storage_to_remove - storage_removed - 1));
      storage_removed += remove_result;
    }
  }

  CHECK_GE(storage_used_, storage_removed);
  storage_used_ -= storage_removed;
  return Status::StatusOK();
}

Status HealthModuleFiles::ReserveStorage(uint32_t storage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  uint32_t actual_storage = storage + 1;  // account for newline character.
  if (actual_storage > max_storage_space_) {
    return Status(error::RESOURCE_EXHAUSTED,
                  "Requested storage space is larger than max allowed storage");
  }
  if (max_storage_space_ >= actual_storage + storage_used_) {
    return Status::StatusOK();
  }

  return FreeStorage(actual_storage);
}

// static
StatusOr<uint32_t> HealthModuleFiles::FileSize(
    const base::FilePath& file_path) {
  base::File::Info file_info;
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid() || !file.GetInfo(&file_info)) {
    base::UmaHistogramEnumeration(
        reporting::kUmaDataLossErrorReason,
        DataLossErrorReason::FAILED_TO_READ_HEALTH_DATA,
        DataLossErrorReason::MAX_VALUE);
    return base::unexpected(Status(
        error::DATA_LOSS, base::StrCat({"Failed to read health data file info ",
                                        file_path.MaybeAsASCII()})));
  }
  return file_info.size;
}
}  // namespace reporting
