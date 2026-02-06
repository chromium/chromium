// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/flushed_map.h"

#include <algorithm>
#include <cstdint>
#include <optional>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "components/metrics/structured/lib/resource_info.h"
#include "components/metrics/structured/proto/event_storage.pb.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace metrics::structured {

namespace {

// Deletes all files at the given paths.
void DeleteFilesOnBackgroundThread(std::vector<base::FilePath> paths) {
  for (auto& file_path : paths) {
    base::DeleteFile(file_path);
  }
}

// Loads all keys from |dir| and returns them.
//
// It is assumed that all of the files in |dir| store serialized EventsProtos.
std::vector<FlushedKey> BuildKeysFromDirOnBackgroundThread(
    const base::FilePath& dir) {
  if (!base::CreateDirectory(dir)) {
    // Note: Only hit if the directory did not exist and could not be created.
    LOG(ERROR) << "Failed to create directory for flushed events: " << dir;
    return {};
  }

  // This loads all paths of |dir| into memory.
  base::FileEnumerator file_enumerator(dir, /*recursive=*/false,
                                       base::FileEnumerator::FileType::FILES);
  std::vector<FlushedKey> keys;

  // Iterate over all files in this directory. All files should be
  // FlushedEvents.
  file_enumerator.ForEach([&keys](const base::FilePath& path) {
    base::File::Info info;
    if (!base::GetFileInfo(path, &info)) {
      LOG(ERROR) << "Failed to get file info for " << path;
      return;
    }

    FlushedKey key;
    key.path = path;
    key.size = info.size;
    key.creation_time = info.creation_time;

    keys.push_back(key);
  });

  std::sort(keys.begin(), keys.end(),
            [](const FlushedKey& l, const FlushedKey& r) {
              return l.creation_time < r.creation_time;
            });
  return keys;
}

}  // namespace

FlushedMap::FlushedMap(const base::FilePath& flushed_dir,
                       uint64_t max_size_bytes)
    : flushed_dir_(flushed_dir),
      resource_info_(max_size_bytes),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&BuildKeysFromDirOnBackgroundThread, flushed_dir_),
      base::BindOnce(&FlushedMap::OnKeysLoaded, weak_factory_.GetWeakPtr()));
}

FlushedMap::~FlushedMap() = default;

void FlushedMap::Purge(
    base::OnceCallback<void(const std::vector<FlushedKey>&)> callback) {
  // This operation is deferred if the map has not been initialized.
  if (!is_initialized_) {
    deferred_operations_.push_back(base::BindOnce(
        &FlushedMap::Purge, weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
  std::vector<FlushedKey> keys_copy = keys_;
  DeleteKeys(keys_);
  if (callback) {
    std::move(callback).Run(keys_copy);
  }
}

void FlushedMap::Flush(EventBuffer<StructuredEventProto>& buffer,
                       FlushedCallback callback) {
  // This operation is not deferred if the map is not yet initialized. Instead,
  // the result of the flush is queued in OnFlushed() and processed after
  // initialization is complete.

  // Generate the new path to flush the buffer to.
  const base::FilePath path = GenerateFilePath();

  // Flush the buffer. This depends on the implementation of |buffer|.
  buffer.Flush(
      path, base::BindOnce(&FlushedMap::OnFlushed, weak_factory_.GetWeakPtr(),
                           std::move(callback)));
}

// static
std::optional<EventsProto> FlushedMap::ReadKey(const FlushedKey& key) {
  std::string content;
  if (!base::ReadFileToString(key.path, &content)) {
    LOG(ERROR) << "Failed to read flushed key: " << key.path;
    return std::nullopt;
  }

  EventsProto events;
  if (!events.MergeFromString(content)) {
    LOG(ERROR)
        << "Failed to load events stored at path: " << key.path
        << ". This probably means the content isn't an EventsProto proto.";
    events.Clear();
  }
  return events;
}

void FlushedMap::DeleteKey(const FlushedKey& key) {
  // DeleteKeys() handles deferred operations if the map has not been
  // initialized.
  DeleteKeys({key});
}

void FlushedMap::DeleteKeys(const std::vector<FlushedKey>& keys_to_delete) {
  // This operation is deferred if the map has not been initialized.
  if (!is_initialized_) {
    deferred_operations_.push_back(base::BindOnce(
        &FlushedMap::DeleteKeys, weak_factory_.GetWeakPtr(), keys_to_delete));
    return;
  }

  absl::flat_hash_set<base::FilePath> paths_to_delete_set;
  for (const auto& key : keys_to_delete) {
    paths_to_delete_set.insert(key.path);
  }

  std::vector<FlushedKey> updated_keys;
  updated_keys.reserve(keys_.size());
  std::vector<base::FilePath> paths_to_delete_on_disk;
  paths_to_delete_on_disk.reserve(keys_to_delete.size());

  for (const auto& key : keys_) {
    // Check if the current key should be deleted by attempting to erase it
    // from the set.
    if (paths_to_delete_set.erase(key.path)) {
      // It was present in the set. Add its path for disk deletion and
      // update resource usage.
      paths_to_delete_on_disk.push_back(key.path);
      resource_info_.used_size_bytes -= key.size;
    } else {
      // This key is not being deleted, so keep it.
      updated_keys.push_back(key);
    }
  }

  // Swap the old keys vector with the new one that has the deleted keys
  // removed.
  keys_.swap(updated_keys);

  // Any paths remaining in paths_to_delete_set were not found in keys_.
  for (const auto& path : paths_to_delete_set) {
    LOG(ERROR) << "Attempting to delete a key that doesn't exist: " << path;
  }

  if (paths_to_delete_on_disk.empty()) {
    return;
  }

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&DeleteFilesOnBackgroundThread,
                                        std::move(paths_to_delete_on_disk)));
}

base::FilePath FlushedMap::GenerateFilePath() const {
  return flushed_dir_.Append(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
}

void FlushedMap::OnKeysLoaded(std::vector<FlushedKey> keys) {
  for (const auto& key : keys) {
    resource_info_.Consume(key.size);
  }
  keys_ = keys;
  is_initialized_ = true;

  for (auto& op : deferred_operations_) {
    std::move(op).Run();
  }
  deferred_operations_.clear();
}

void FlushedMap::OnFlushed(FlushedCallback callback,
                           base::expected<FlushedKey, FlushError> key) {
  if (!is_initialized_) {
    deferred_operations_.push_back(
        base::BindOnce(&FlushedMap::OnFlushed, weak_factory_.GetWeakPtr(),
                       std::move(callback), std::move(key)));
    return;
  }
  if (!key.has_value()) {
    LOG(ERROR) << "Flush failed with error: " << static_cast<int>(key.error());
    std::move(callback).Run(key);
    return;
  }

  // Check if the key is already in |keys_|. This can happen if the key was
  // loaded from disk during initialization.
  auto it =
      std::find_if(keys_.begin(), keys_.end(),
                   [&key](const FlushedKey& k) { return k.path == key->path; });

  if (it != keys_.end()) {
    std::move(callback).Run(key);
    return;
  }

  // Add the key to |keys_| to keep track of it.
  keys_.push_back(*key);

  // Check if we have room to contain this data.
  const bool under_quota = resource_info_.HasRoom(key->size);

  // We have flushed at this point, the resources needs to be consumed.
  resource_info_.Consume(key->size);

  // Notify the call that this flush makes use exceed the allotted quota.
  // The key is not returned in this case because it is added to |keys_| above.
  if (under_quota) {
    std::move(callback).Run(key);
  } else {
    std::move(callback).Run(base::unexpected(kQuotaExceeded));
  }
}

}  // namespace metrics::structured
