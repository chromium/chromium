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
#include "base/functional/callback_helpers.h"
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
#include "components/bookmarks/common/bookmark_features.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/common/storage_file_encryption_type.h"
#include "components/os_crypt/async/common/encryptor.h"

namespace bookmarks {

namespace {

// Extension used for backup files (copy of main file created during startup).
const base::FilePath::CharType kBackupExtension[] = FILE_PATH_LITERAL("bak");
constexpr char kBookmarkStorageHistogramSuffix[] = "BookmarkStorage";
constexpr char kBookmarkStorageEncryptedHistogramSuffix[] =
    "BookmarkStorageEncrypted";

std::string GetHistogramSuffix(StorageFileEncryptionType encryption_type) {
  switch (encryption_type) {
    case StorageFileEncryptionType::kClearText:
      return kBookmarkStorageHistogramSuffix;
    case StorageFileEncryptionType::kEncrypted:
      return kBookmarkStorageEncryptedHistogramSuffix;
  }
  NOTREACHED();
}

metrics::ImportantFileWriterType GetImportantFileWriterTypeForMetrics(
    StorageFileEncryptionType encryption_type) {
  switch (encryption_type) {
    case StorageFileEncryptionType::kClearText:
      return metrics::ImportantFileWriterType::kBookmarkStorage;
    case StorageFileEncryptionType::kEncrypted:
      return metrics::ImportantFileWriterType::kBookmarkStorageEncrypted;
  }
  NOTREACHED();
}

metrics::ImportantFileWriterType GetImmediateImportantFileWriterTypeForMetrics(
    StorageFileEncryptionType encryption_type) {
  switch (encryption_type) {
    case StorageFileEncryptionType::kClearText:
      return metrics::ImportantFileWriterType::kBookmarkStorageImmediate;
    case StorageFileEncryptionType::kEncrypted:
      return metrics::ImportantFileWriterType::
          kBookmarkStorageEncryptedImmediate;
  }
  NOTREACHED();
}

void BackupCallback(const base::FilePath& path,
                    const base::FilePath& secondary_file_path) {
  base::FilePath backup_path = path.ReplaceExtension(kBackupExtension);
  base::CopyFile(path, backup_path);
  if (ShouldWriteBookmarksToSecondaryFileOnDisk()) {
    base::FilePath secondary_backup_path =
        secondary_file_path.ReplaceExtension(kBackupExtension);
    base::CopyFile(secondary_file_path, secondary_backup_path);
  }
}

base::DictValue EncodeModelToDict(
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

void RecordSerializationResult(
    const base::TimeTicks& start_time,
    metrics::ImportantFileWriterType important_file_writer_type,
    metrics::BookmarksSerializationResult result) {
  if (result == metrics::BookmarksSerializationResult::kSuccess) {
    metrics::RecordTimeToSerialize(important_file_writer_type,
                                   base::TimeTicks::Now() - start_time);
  }
  metrics::RecordBookmarksSerializationResult(important_file_writer_type,
                                              result);
}

void SaveJsonContentToFile(
    std::string original_json_content,
    scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
        encryptor,
    StorageFileEncryptionType encryption_type,
    const base::FilePath file_path) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  CHECK(encryptor);
  std::string json_content = std::move(original_json_content);
  if (encryption_type == StorageFileEncryptionType::kEncrypted) {
    std::string encrypted_json_content;
    if (!encryptor->data.EncryptString(json_content, &encrypted_json_content)) {
      RecordSerializationResult(
          start_time,
          GetImmediateImportantFileWriterTypeForMetrics(encryption_type),
          metrics::BookmarksSerializationResult::kEncryptionFailed);
      return;
    }
    json_content = std::move(encrypted_json_content);
  }
  RecordSerializationResult(
      start_time,
      GetImmediateImportantFileWriterTypeForMetrics(encryption_type),
      metrics::BookmarksSerializationResult::kSuccess);
  base::ImportantFileWriter::WriteFileAtomically(
      file_path, std::move(json_content),
      base::StrCat({GetHistogramSuffix(encryption_type), "Immediate"}));
}

}  // namespace

// static
constexpr base::TimeDelta BookmarkStorage::kSaveDelay;

BookmarkStorage::BookmarkStorage(
    const BookmarkModel* model,
    PermanentNodeSelection permanent_node_selection,
    const scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
        encryptor,
    const base::FilePath& clear_text_file_path,
    const base::FilePath& encrypted_file_path)
    : model_(model),
      backend_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      permanent_node_selection_(permanent_node_selection),
      encryptor_(encryptor),
      primary_file_encryption_type_(
          ShouldUseEncryptedBookmarksAsPrimarySource()
              ? StorageFileEncryptionType::kEncrypted
              : StorageFileEncryptionType::kClearText),
      clear_text_file_path_(clear_text_file_path),
      encrypted_file_path_(encrypted_file_path),
      writer_(
          primary_file_encryption_type_ == StorageFileEncryptionType::kEncrypted
              ? encrypted_file_path
              : clear_text_file_path,
          backend_task_runner_,
          kSaveDelay,
          GetHistogramSuffix(primary_file_encryption_type_)),
      last_scheduled_save_(base::TimeTicks::Now()) {
  CHECK(!clear_text_file_path.empty());
  CHECK(!encrypted_file_path.empty());
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
        FROM_HERE, base::BindOnce(&BackupCallback, writer_.path(),
                                  GetSecondaryFilePath()));
  }

  writer_.ScheduleWriteWithBackgroundDataSerializer(this);

  const base::TimeDelta schedule_delta =
      base::TimeTicks::Now() - last_scheduled_save_;
  metrics::RecordTimeSinceLastScheduledSave(schedule_delta);
  last_scheduled_save_ = base::TimeTicks::Now();
  was_scheduled_save_ever_called_ = true;
}

base::ImportantFileWriter::BackgroundDataProducerCallback
BookmarkStorage::GetSerializedDataProducerForBackgroundSequence() {
  base::DictValue value = EncodeModelToDict(model_, permanent_node_selection_);
  return base::BindOnce(
      [](base::DictValue value,
         scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
             encryptor,
         StorageFileEncryptionType primary_file_encryption_type,
         const base::FilePath secondary_file_path)
          -> std::optional<std::string> {
        const base::TimeTicks start_time = base::TimeTicks::Now();
        std::string json_content;
        if (!base::JSONWriter::WriteWithOptions(
                value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_content)) {
          RecordSerializationResult(
              start_time,
              GetImportantFileWriterTypeForMetrics(
                  primary_file_encryption_type),
              metrics::BookmarksSerializationResult::kJSONParsingFailed);
          return std::nullopt;
        }

        switch (primary_file_encryption_type) {
          case StorageFileEncryptionType::kEncrypted: {
            CHECK(encryptor);
            std::string encrypted_json_content;
            const bool encryption_succeeded = encryptor->data.EncryptString(
                json_content, &encrypted_json_content);
            if (ShouldWriteBookmarksToSecondaryFileOnDisk()) {
              // Also write the unencrypted data to disk. Make sure this second
              // write is performed after the first one is completed.
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(
                      base::IgnoreResult(
                          &base::ImportantFileWriter::WriteFileAtomically),
                      secondary_file_path, std::move(json_content),
                      kBookmarkStorageHistogramSuffix));
            }
            if (!encryption_succeeded) {
              RecordSerializationResult(
                  start_time,
                  metrics::ImportantFileWriterType::kBookmarkStorageEncrypted,
                  metrics::BookmarksSerializationResult::kEncryptionFailed);
              return std::nullopt;
            }
            RecordSerializationResult(
                start_time,
                metrics::ImportantFileWriterType::kBookmarkStorageEncrypted,
                metrics::BookmarksSerializationResult::kSuccess);
            return encrypted_json_content;
          }
          case StorageFileEncryptionType::kClearText: {
            metrics::BookmarksSerializationResult result =
                metrics::BookmarksSerializationResult::kSuccess;
            if (ShouldWriteBookmarksToSecondaryFileOnDisk()) {
              CHECK(encryptor);
              std::string encrypted_json_content;
              if (encryptor->data.EncryptString(json_content,
                                                &encrypted_json_content)) {
                // Also write the encrypted data to disk. Make sure this second
                // write is performed after the first one is completed.
                base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                    FROM_HERE,
                    base::BindOnce(
                        base::IgnoreResult(
                            &base::ImportantFileWriter::WriteFileAtomically),
                        secondary_file_path, std::move(encrypted_json_content),
                        kBookmarkStorageEncryptedHistogramSuffix));
              } else {
                result =
                    metrics::BookmarksSerializationResult::kEncryptionFailed;
              }
            }
            RecordSerializationResult(
                start_time, metrics::ImportantFileWriterType::kBookmarkStorage,
                result);
            return json_content;
          }
        }
        NOTREACHED();
      },
      std::move(value), encryptor_, primary_file_encryption_type_,
      GetSecondaryFilePath());
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

void BookmarkStorage::SaveSingleFileIfNoPreviousSave(
    StorageFileEncryptionType encryption_type,
    std::string json_content) {
  CHECK(encryptor_);
  if (was_scheduled_save_ever_called_) {
    // The storage has already been scheduled to save. Do nothing since the
    // json_content might be outdated.
    return;
  }

  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SaveJsonContentToFile, std::move(json_content),
                     encryptor_, encryption_type,
                     encryption_type == StorageFileEncryptionType::kEncrypted
                         ? encrypted_file_path_
                         : clear_text_file_path_));
}

base::FilePath BookmarkStorage::GetSecondaryFilePath() const {
  switch (primary_file_encryption_type_) {
    case StorageFileEncryptionType::kClearText:
      return encrypted_file_path_;
    case StorageFileEncryptionType::kEncrypted:
      return clear_text_file_path_;
  }
  NOTREACHED();
}

}  // namespace bookmarks
