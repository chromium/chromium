// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_storage.h"

#include <stddef.h>
#include <algorithm>
#include <unordered_map>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/guid.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/bookmarks/browser/bookmark_codec.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"

namespace bookmarks {

namespace {

// Extension used for backup files (copy of main file created during startup).
const base::FilePath::CharType kBackupExtension[] = FILE_PATH_LITERAL("bak");

void BackupCallback(const base::FilePath& path) {
  base::FilePath backup_path = path.ReplaceExtension(kBackupExtension);
  base::CopyFile(path, backup_path);
}

}  // namespace

// static
constexpr base::TimeDelta BookmarkStorage::kSaveDelay;

BookmarkStorage::BookmarkStorage(BookmarkModel* model,
                                 const base::FilePath& file_path)
    : model_(model),
      backend_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      writer_(file_path, backend_task_runner_, kSaveDelay, "BookmarkStorage"),
      last_scheduled_save_(base::TimeTicks::Now()) {}

BookmarkStorage::~BookmarkStorage() {
  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();
}

void BookmarkStorage::ScheduleSave() {
  // If this is the first scheduled save, create a backup before overwriting the
  // JSON file.
  if (!backup_triggered_) {
    backup_triggered_ = true;
    backend_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&BackupCallback, writer_.path()));
  }

  writer_.ScheduleWriteWithBackgroundDataSerializer(this);

  const base::TimeDelta schedule_delta =
      base::TimeTicks::Now() - last_scheduled_save_;
  metrics::RecordTimeSinceLastScheduledSave(schedule_delta);
  last_scheduled_save_ = base::TimeTicks::Now();
}

void BookmarkStorage::BookmarkModelDeleted() {
  // We need to save now as otherwise by the time SerializeData() is invoked
  // the model is gone.
  if (writer_.HasPendingWrite()) {
    writer_.DoScheduledWrite();
    DCHECK(!writer_.HasPendingWrite());
  }

  model_ = nullptr;
}

base::ImportantFileWriter::BackgroundDataProducerCallback
BookmarkStorage::GetSerializedDataProducerForBackgroundSequence() {
  BookmarkCodec codec;
  base::Value value(
      codec.Encode(model_, model_->client()->EncodeBookmarkSyncMetadata()));

  return base::BindOnce(
      [](base::Value value, std::string* output) {
        // This runs on the background sequence.
        JSONStringValueSerializer serializer(output);
        serializer.set_pretty_print(true);
        return serializer.Serialize(value);
      },
      std::move(value));
}

bool BookmarkStorage::HasScheduledSaveForTesting() const {
  return writer_.HasPendingWrite();
}

}  // namespace bookmarks
