// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/storage_util.h"

#include <string>
#include <tuple>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "components/reporting/util/file.h"
#include "components/reporting/util/status.h"

namespace reporting {

// static
StorageDirectory::Set StorageDirectory::FindQueueDirectories(
    const base::FilePath& storage_directory,
    const StorageOptions::QueuesOptionsList& options_list) {
  Set queue_params;
  base::FileEnumerator dir_enum(storage_directory,
                                /*recursive=*/false,
                                base::FileEnumerator::DIRECTORIES);
  for (auto full_name = dir_enum.Next(); !full_name.empty();
       full_name = dir_enum.Next()) {
    if (const auto priority_result =
            ParsePriorityFromQueueDirectory(full_name, options_list);
        priority_result.has_value() && full_name.Extension().empty()) {
      // This is a legacy queue directory named just by priority with no
      // generation guid as an extension: foo/bar/Security,
      // foo/bar/FastBatch, etc.
      queue_params.emplace(priority_result.value());
      LOG(WARNING) << "Found legacy queue directory: " << full_name;
    } else {
      LOG(WARNING) << "Could not parse queue parameters from filename "
                   << full_name.MaybeAsASCII()
                   << " error = " << priority_result.error();
    }
  }
  return queue_params;
}

// static
StatusOr<Priority> StorageDirectory::ParsePriorityFromQueueDirectory(
    const base::FilePath& full_path,
    const StorageOptions::QueuesOptionsList& options_list) {
  for (const auto& priority_queue_options_pair : options_list) {
    if (priority_queue_options_pair.second.directory() ==
        full_path.RemoveExtension()) {
      return priority_queue_options_pair.first;
    }
  }
  return base::unexpected(Status(
      error::NOT_FOUND, base::StrCat({"Found no priority for queue directory ",
                                      full_path.MaybeAsASCII()})));
}

// static
bool StorageDirectory::IsMetaDataFile(const base::FilePath& filepath) {
  const auto found = filepath.BaseName().value().find(kMetadataFileNamePrefix);
  return found != std::string::npos;
}
}  // namespace reporting
