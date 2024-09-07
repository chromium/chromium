// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/flushed_map.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <unordered_set>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "components/metrics/structured/lib/resource_info.h"
#include "components/metrics/structured/proto/event_storage.pb.h"

namespace metrics::structured {

FlushedMap::FlushedMap(const base::FilePath& flushed_dir,
                       uint64_t max_size_bytes)
    : flushed_dir_(flushed_dir),
      resource_info_(max_size_bytes),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  if (!base::CreateDirectory(flushed_dir_)) {
    LOG(ERROR) << "Failed to created directory for flushed events: "
               << flushed_dir_;
    return;
  }

  LoadKeysFromDir(flushed_dir_);
}

FlushedMap::~FlushedMap() = default;

void FlushedMap::Purge() {
  for (const FlushedKey& key : keys_) {
    if (!base::DeleteFile(base::FilePath(key.path))) {
      LOG(ERROR) << "Failed to delete key: " << key.path;
    }
  }
  keys_.clear();
}

void FlushedMap::Flush(EventBuffer<StructuredEventProto>& buffer,
                       FlushedCallback callback) {
  // Generate the new path to flush the buffer to.
  const base::FilePath path = GenerateFilePath();

  // Flush the buffer. This depends on the implementation of |buffer|.
  buffer.Flush(path,
               base::BindPostTask(task_runner_,
                                  base::BindOnce(&FlushedMap::OnFlushed,
                                                 weak_factory_.GetWeakPtr(),
                                                 std::move(callback))));
}

std::optional<EventsProto> FlushedMap::ReadKey(const FlushedKey& key) const {
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
  // If not on |task_runner_| then post to that task.
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&FlushedMap::DeleteKey,
                                          weak_factory_.GetWeakPtr(), key));
    return;
  }

  // Lambda is safe because it is used only within this scope.
  auto elem = std::find_if(
      keys_.begin(), keys_.end(),
      [&key](const FlushedKey& elem) { return elem.path == key.path; });

  if (elem == keys_.end()) {
    LOG(ERROR) << "Attempting to delete a key that doesn't exist: " << key.path;
    return;
  }

  keys_.erase(elem);
  resource_info_.used_size_bytes -= key.size;
  base::DeleteFile(key.path);
}

void FlushedMap::DeleteKeys(const std::vector<FlushedKey>& keys) {
  // If not on |task_runner_| then post to that task.
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&FlushedMap::DeleteKeys,
                                          weak_factory_.GetWeakPtr(), keys));
    return;
  }

  for (const FlushedKey& key : keys) {
    DeleteKey(key);
  }
}

base::FilePath FlushedMap::GenerateFilePath() const {
  return flushed_dir_.Append(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
}

void FlushedMap::LoadKeysFromDir(const base::FilePath& dir) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&FlushedMap::BuildKeysFromDir,
                                        weak_factory_.GetWeakPtr(), dir));
}

void FlushedMap::BuildKeysFromDir(const base::FilePath& dir) {
  // This loads all paths of |dir| into memory.
  base::FileEnumerator file_enumerator(dir, /*recursive=*/false,
                                       base::FileEnumerator::FileType::FILES);

  // Iterate over all files in this directory. All files should be
  // FlushedEvents.
  file_enumerator.ForEach([this](const base::FilePath& path) {
    base::File::Info info;
    if (!base::GetFileInfo(path, &info)) {
      LOG(ERROR) << "Failed to get file info for " << path;
      return;
    }

    FlushedKey key;
    key.path = path;
    key.size = info.size;
    key.creation_time = info.creation_time;

    // Update the amount of space consumed. We should always be below quota. If
    // not, then the next flush will resolve it.
    resource_info_.Consume(key.size);

    keys_.push_back(key);
  });

  std::sort(keys_.begin(), keys_.end(),
            [](const FlushedKey& l, const FlushedKey& r) {
              return l.creation_time < r.creation_time;
            });
}

void FlushedMap::OnFlushed(FlushedCallback callback,
                           base::expected<FlushedKey, FlushError> key) {
  if (!key.has_value()) {
    LOG(ERROR) << "Flush failed with error: " << static_cast<int>(key.error());
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
