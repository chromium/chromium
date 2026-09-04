// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/hash_prefix_map.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/scoped_blocking_call.h"

namespace safe_browsing {

namespace {

std::string GenerateExtension(PrefixSize size) {
  return base::StrCat(
      {base::NumberToString(size), "_",
       base::NumberToString(
           base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds())});
}

}  // namespace

HashPrefixMap::HashPrefixMap(
    const base::FilePath& store_path,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    size_t buffer_size)
    : HashPrefixContainer(store_path, std::move(task_runner), buffer_size) {}

HashPrefixMap::~HashPrefixMap() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

void HashPrefixMap::Clear() {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // Clear the map on the db task runner, since the memory mapped files should
    // be destroyed on the same thread they were created. base::Unretained is
    // safe since the map is destroyed on the db task runner.
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&HashPrefixMap::ClearOnTaskRunner,
                                          base::Unretained(this)));
  } else {
    map_.clear();
  }
}

void HashPrefixMap::ClearOnTaskRunner() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  map_.clear();
}

HashPrefixMapView HashPrefixMap::view() const {
  HashPrefixMapView view;
  view.reserve(map_.size());
  for (const auto& kv : map_) {
    if (!kv.second.IsReadable()) {
      continue;
    }

    view.emplace(kv.first, kv.second.GetView());
  }
  return view;
}

void HashPrefixMap::Append(PrefixSize size, HashPrefixesView prefix) {
  if (prefix.empty()) {
    return;
  }

  GetFileInfo(size)
      .GetOrCreateWriter(buffer_size_, GenerateExtension(size), "V4Store")
      ->Write(prefix);
}

ApplyUpdateResult HashPrefixMap::ReadFromDisk(
    const SBStoreFileFormat& file_format) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const V4StoreFileFormat& v4_file_format = file_format.v4_file_format();
  DCHECK(v4_file_format.list_update_response().additions().empty());
  for (const auto& hash_file : v4_file_format.hash_files()) {
    PrefixSize prefix_size = hash_file.prefix_size();
    if (hash_file.file_size() % prefix_size != 0) {
      return ADDITIONS_SIZE_UNEXPECTED_FAILURE;
    }

    auto& file_info = GetFileInfo(prefix_size);
    if (!file_info.Initialize(hash_file.extension(), hash_file.file_size(),
                              /*initialize_after_write=*/false, "V4Store")) {
      return MMAP_FAILURE;
    }
  }
  return APPLY_UPDATE_SUCCESS;
}

namespace {

class HashPrefixMapWriteSession : public HashPrefixContainer::WriteSession {
 public:
  HashPrefixMapWriteSession() = default;
  HashPrefixMapWriteSession(const HashPrefixMapWriteSession&) = delete;
  HashPrefixMapWriteSession& operator=(const HashPrefixMapWriteSession&) =
      delete;
  ~HashPrefixMapWriteSession() override = default;
};

}  // namespace

std::unique_ptr<HashPrefixContainer::WriteSession> HashPrefixMap::WriteToDisk(
    SBStoreFileFormat& file_format) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  V4StoreFileFormat* v4_file_format = file_format.v4_file_format_mutable();
  for (auto& [size, file_info] : map_) {
    HashFile hash_file;
    std::optional<FinalizedFileInfo> finalized_info =
        file_info.Finalize("V4Store");
    if (!finalized_info.has_value()) {
      return nullptr;
    }

    if (finalized_info->file_size == 0) {
      continue;
    }

    if (!file_info.Initialize(finalized_info->extension,
                              finalized_info->file_size,
                              /*initialize_after_write=*/true, "V4Store")) {
      return nullptr;
    }

    hash_file.set_prefix_size(size);
    hash_file.set_file_size(finalized_info->file_size);
    hash_file.set_extension(std::move(finalized_info->extension));

    v4_file_format->add_hash_files()->Swap(&hash_file);
  }
  return std::make_unique<HashPrefixMapWriteSession>();
}

ApplyUpdateResult HashPrefixMap::IsValid() const {
  for (const auto& kv : map_) {
    if (!kv.second.IsReadable()) {
      return MMAP_FAILURE;
    }
  }
  return APPLY_UPDATE_SUCCESS;
}

HashPrefixStr HashPrefixMap::GetMatchingHashPrefix(std::string_view full_hash) {
  for (const auto& kv : map_) {
    HashPrefixStr matching_prefix = kv.second.Matches(full_hash);
    if (!matching_prefix.empty()) {
      return matching_prefix;
    }
  }
  return HashPrefixStr();
}

void HashPrefixMap::GetPrefixInfo(
    google::protobuf::RepeatedPtrField<
        DatabaseManagerInfo::DatabaseInfo::StoreInfo::PrefixSet>* prefix_sets) {
  for (const auto& size_and_info : map_) {
    const PrefixSize& size = size_and_info.first;
    const FileInfo& info = size_and_info.second;

    DatabaseManagerInfo::DatabaseInfo::StoreInfo::PrefixSet* prefix_set =
        prefix_sets->Add();
    prefix_set->set_size(size);
    prefix_set->set_count(info.GetView().size() / size);
  }
}

std::vector<base::FilePath> HashPrefixMap::GetPaths() const {
  std::vector<base::FilePath> paths;
  for (const auto& [size, file_info] : map_) {
    if (file_info.IsReadable()) {
      paths.push_back(file_info.GetPath());
    }
  }
  return paths;
}

const std::string& HashPrefixMap::GetExtensionForTesting(PrefixSize size) {
  return GetFileInfo(size).GetExtensionForTesting();  // IN-TEST
}

void HashPrefixMap::ClearAndWaitForTesting() {
  Clear();
  base::RunLoop run_loop;
  task_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

HashPrefixMap::FileInfo& HashPrefixMap::GetFileInfo(PrefixSize size) {
  auto [it, inserted] = map_.try_emplace(size, store_path_, size);
  return it->second;
}

}  // namespace safe_browsing
