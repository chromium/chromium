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
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
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

base::Value::Dict EncodeModelToDict(
    const BookmarkModel* model,
    BookmarkStorage::PermanentNodeSelection permanent_node_selection) {
  BookmarkCodec codec;
  switch (permanent_node_selection) {
    case BookmarkStorage::kSelectLocalOrSyncableNodes:
      return codec.Encode(
          model->bookmark_bar_node(), model->other_node(), model->mobile_node(),
          model->client()->EncodeLocalOrSyncableBookmarkSyncMetadata());
    case BookmarkStorage::kSelectAccountNodes:
      // Either all permanent folders or none should exist.
      if (model->account_bookmark_bar_node()) {
        CHECK(model->account_other_node());
        CHECK(model->account_mobile_node());
      } else {
        // Encode the model even for the null-permanent-folder case in case
        // there is sync metadata to persist (e.g. the notion of a user having
        // too many bookmarks server-side).
        CHECK(!model->account_other_node());
        CHECK(!model->account_mobile_node());
      }
      return codec.Encode(model->account_bookmark_bar_node(),
                          model->account_other_node(),
                          model->account_mobile_node(),
                          model->client()->EncodeAccountBookmarkSyncMetadata());
  }

  NOTREACHED();
}

bool ShouldSaveBackupFile(
    BookmarkStorage::PermanentNodeSelection permanent_node_selection) {
  switch (permanent_node_selection) {
    case BookmarkStorage::kSelectLocalOrSyncableNodes:
      return true;
    case BookmarkStorage::kSelectAccountNodes:
      return false;
  }

  NOTREACHED();
}

}  // namespace

// static
constexpr base::TimeDelta BookmarkStorage::kSaveDelay;

BookmarkStorage::BookmarkStorage(
    const BookmarkModel* model,
    PermanentNodeSelection permanent_node_selection,
    const base::FilePath& file_path)
    : model_(model),
      backend_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      permanent_node_selection_(permanent_node_selection),
      writer_(file_path, backend_task_runner_, kSaveDelay, "BookmarkStorage"),
      last_scheduled_save_(base::TimeTicks::Now()) {
  CHECK(!file_path.empty());
}

BookmarkStorage::~BookmarkStorage() {
  SaveNowIfScheduled();
}

void BookmarkStorage::ScheduleSave() {
  // If this is the first scheduled save, create a backup before overwriting the
  // JSON file.
  if (!backup_triggered_ && ShouldSaveBackupFile(permanent_node_selection_)) {
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

base::ImportantFileWriter::BackgroundDataProducerCallback
BookmarkStorage::GetSerializedDataProducerForBackgroundSequence() {
  base::Value::Dict value =
      EncodeModelToDict(model_, permanent_node_selection_);

  return base::BindOnce(
      [](base::Value::Dict value) -> std::optional<std::string> {
        // This runs on the background sequence.
        std::string output;
        if (!base::JSONWriter::WriteWithOptions(
                value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &output)) {
          return std::nullopt;
        }
        return output;
      },
      std::move(value));
}

bool BookmarkStorage::HasScheduledSaveForTesting() const {
  return writer_.HasPendingWrite();
}

void BookmarkStorage::SaveNowIfScheduledForTesting() {
  SaveNowIfScheduled();
}

void BookmarkStorage::SaveNowIfScheduled() {
  if (writer_.HasPendingWrite()) {
    writer_.DoScheduledWrite();
  }
}

}  // namespace bookmarks
