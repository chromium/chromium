// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/model_loader.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/numerics/clamped_math.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/bookmarks/browser/bookmark_codec.h"
#include "components/bookmarks/browser/bookmark_load_details.h"
#include "components/bookmarks/browser/titled_url_index.h"
#include "components/bookmarks/browser/url_index.h"
#include "components/bookmarks/common/bookmark_features.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/common/storage_file_encryption_type.h"
#include "components/bookmarks/common/url_load_stats.h"
#include "components/bookmarks/common/user_folder_load_stats.h"
#include "components/os_crypt/async/common/encryptor.h"

namespace bookmarks {

namespace {

const base::FilePath::CharType kBackupExtension[] = FILE_PATH_LITERAL("bak");

// Loads a JSON file determined by `file_path` and returns its content as a
// string or an error code if something fails.
// No kSuccess result is returned in case of success, it is implied by having a
// string returned.
// This function does not parse or decode the bookmarks data, so
// kJSONParsingFailed and kBookmarkCodecDecodingFailed aren't possible return
// values.
base::expected<std::string, metrics::BookmarksFileLoadResult> ReadFile(
    scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
        encryptor,
    StorageFileEncryptionType encryption_type,
    const base::FilePath& file_path,
    metrics::StorageFileForUma storage_file_for_uma) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  if (!base::PathExists(file_path)) {
    return base::unexpected(metrics::BookmarksFileLoadResult::kFileMissing);
  }
  std::string json_string;
  if (!base::ReadFileToString(file_path, &json_string)) {
    return base::unexpected(
        metrics::BookmarksFileLoadResult::kContentLoadingFailed);
  }

  if (encryption_type == StorageFileEncryptionType::kClearText) {
    metrics::RecordTimeToReadFile(storage_file_for_uma, encryption_type,
                                  base::TimeTicks::Now() - start_time);
    return json_string;
  }

  CHECK(encryptor);
  std::string decrypted_json_string;
  if (!encryptor->data.DecryptString(json_string, &decrypted_json_string)) {
    return base::unexpected(
        metrics::BookmarksFileLoadResult::kDecryptionFailed);
  }
  metrics::RecordTimeToReadFile(storage_file_for_uma, encryption_type,
                                base::TimeTicks::Now() - start_time);
  return decrypted_json_string;
}

// Deserializes the given JSON string. Returns it in the form of a dictionary,
// or the kJSONParsingFailed error code if something fails.
base::expected<base::DictValue, metrics::BookmarksFileLoadResult>
DeserializeStringToDict(std::string_view json_string) {
  // Titles may end up containing invalid utf and we shouldn't throw away
  // all bookmarks if some titles have invalid utf.
  JSONStringValueDeserializer deserializer(
      json_string, base::JSON_REPLACE_INVALID_CHARACTERS);
  std::unique_ptr<base::Value> root = deserializer.Deserialize(
      /*error_code=*/nullptr, /*error_message=*/nullptr);
  if (!root || !root->is_dict()) {
    return base::unexpected(
        metrics::BookmarksFileLoadResult::kJSONParsingFailed);
  }
  return std::move(*root).TakeDict();
}

void ReadBookmarksInSecondaryFileAndVerifyContentOnBackgroundSequence(
    std::string primary_json_string,
    scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
        encryptor,
    StorageFileEncryptionType secondary_encryption_type,
    const base::FilePath secondary_file_path,
    metrics::StorageFileForUma storage_file_for_uma,
    ModelLoader::SaveSingleFileCallback save_single_file_callback) {
  base::expected<std::string, metrics::BookmarksFileLoadResult>
      secondary_json_string =
          ReadFile(encryptor, secondary_encryption_type, secondary_file_path,
                   storage_file_for_uma);
  metrics::RecordBookmarksFileLoadResult(
      storage_file_for_uma, secondary_encryption_type,
      secondary_json_string.error_or(
          metrics::BookmarksFileLoadResult::kSuccess));
  bool file_matches = false;
  if (secondary_json_string.has_value()) {
    file_matches = primary_json_string == secondary_json_string.value();
    metrics::RecordEncryptedBookmarksFileMatchesResult(storage_file_for_uma,
                                                       file_matches);
  }
  if (!file_matches) {
    std::move(save_single_file_callback)
        .Run(secondary_encryption_type, std::move(primary_json_string));
  }
}

void MaybeScheduleReadBookmarksInSecondaryFileAndVerifyContent(
    std::string primary_json_string,
    const scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
        encryptor,
    StorageFileEncryptionType secondary_encryption_type,
    const base::FilePath& secondary_file_path,
    metrics::StorageFileForUma storage_file_for_uma,
    ModelLoader::SaveSingleFileCallback save_single_file_callback) {
  if (!ShouldVerifyBookmarksDataInSecondaryFileOnLoad()) {
    return;
  }
  CHECK(encryptor);
  CHECK(!secondary_file_path.empty());
  // Validate the encrypted data on a different task in order not to impact the
  // bookmarks load time.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ReadBookmarksInSecondaryFileAndVerifyContentOnBackgroundSequence,
          std::move(primary_json_string), encryptor, secondary_encryption_type,
          secondary_file_path, storage_file_for_uma,
          std::move(save_single_file_callback)));
}

struct StorageFileReadConfig {
  // The type of file content that should be used as the primary source.
  StorageFileEncryptionType primary_source_encryption_type;
  // Whether the encrypted file was supposed to be used but was missing so need
  // to fall back to the clear text file.
  bool is_clear_text_fallback;
};

StorageFileReadConfig DetermineStorageFileReadConfig(
    const base::FilePath& clear_text_file_path,
    const base::FilePath& encrypted_file_path) {
  if (!ShouldUseEncryptedBookmarksAsPrimarySource()) {
    return {StorageFileEncryptionType::kClearText,
            /*is_clear_text_fallback=*/false};
  }

  // Fallback to clear text if the encrypted file is missing and clear text
  // file exists.
  if (!base::PathExists(encrypted_file_path) &&
      base::PathExists(clear_text_file_path)) {
    return {StorageFileEncryptionType::kClearText,
            /*is_clear_text_fallback=*/true};
  }

  return {StorageFileEncryptionType::kEncrypted,
          /*is_clear_text_fallback=*/false};
}

void OnFileLoaded(metrics::StorageFileForUma storage_file_for_uma,
                  StorageFileEncryptionType encryption_type,
                  StorageFileReadConfig storage_file_read_config,
                  metrics::BookmarksFileLoadResult result) {
  if (storage_file_read_config.is_clear_text_fallback) {
    metrics::RecordFallbackToClearTextFileOnLoadResult(storage_file_for_uma,
                                                       result);
  } else {
    metrics::RecordBookmarksFileLoadResult(storage_file_for_uma,
                                           encryption_type, result);
  }
}

void MaybeCleanUpFiles(StorageFileEncryptionType primary_source_encryption_type,
                       metrics::StorageFileForUma storage_file_for_uma,
                       const base::FilePath& clear_text_file_path) {
  if (!ShouldDeleteClearTextBookmarksFile() ||
      primary_source_encryption_type == StorageFileEncryptionType::kClearText) {
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](metrics::StorageFileForUma storage_file_for_uma,
             const base::FilePath& clear_text_file_path) {
            bool deleted =
                base::DeleteFile(clear_text_file_path) &&
                base::DeleteFile(
                    clear_text_file_path.ReplaceExtension(kBackupExtension));
            metrics::RecordClearTextFileDeletionResult(storage_file_for_uma,
                                                       deleted);
          },
          storage_file_for_uma, clear_text_file_path));
}

std::unique_ptr<BookmarkLoadDetails> LoadBookmarks(
    const scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
        encryptor,
    const base::FilePath& local_or_syncable_file_path,
    const base::FilePath& encrypted_local_or_syncable_file_path,
    const base::FilePath& account_file_path,
    const base::FilePath& encrypted_account_file_path,
    ModelLoader::SaveSingleFileCallback
        save_local_or_syncable_single_file_callback,
    ModelLoader::SaveSingleFileCallback save_account_single_file_callback) {
  auto details = std::make_unique<BookmarkLoadDetails>();

  std::set<int64_t> ids_assigned_to_account_nodes;

  // Decode account bookmarks (if any). Doing this before decoding
  // local-or-syncable ones is interesting because, in case there are ID
  // collisions, it will lead to ID reassignments on the local-or-syncable part,
  // which is usually harmless. Doing the opposite would imply that account
  // bookmarks need to be redownloaded from the server (because ID reassignment
  // leads to invalidating sync metadata). This is particularly interesting on
  // iOS, in case the files were written by two independent BookmarkModel
  // instances (and hence the two files are prone to ID collisions) and later
  // loaded into a single BookmarkModel instance.
  if (!account_file_path.empty()) {
    std::string sync_metadata_str;
    int64_t max_node_id = 0;
    const StorageFileReadConfig account_storage_file_read_config =
        DetermineStorageFileReadConfig(account_file_path,
                                       encrypted_account_file_path);
    const StorageFileEncryptionType account_encryption_type =
        account_storage_file_read_config.primary_source_encryption_type;
    std::unique_ptr<BookmarkPermanentNode> account_bb_node =
        BookmarkPermanentNode::CreateBookmarkBar(0, /*is_account_node=*/true);
    std::unique_ptr<BookmarkPermanentNode> account_other_folder_node =
        BookmarkPermanentNode::CreateOtherBookmarks(0,
                                                    /*is_account_node=*/true);
    std::unique_ptr<BookmarkPermanentNode> account_mobile_folder_node =
        BookmarkPermanentNode::CreateMobileBookmarks(0,
                                                     /*is_account_node=*/true);

    const base::expected<std::string, metrics::BookmarksFileLoadResult>
        json_string = ReadFile(
            encryptor, account_encryption_type,
            account_encryption_type == StorageFileEncryptionType::kEncrypted
                ? encrypted_account_file_path
                : account_file_path,
            metrics::StorageFileForUma::kAccount);
    base::expected<base::DictValue, metrics::BookmarksFileLoadResult>
        root_dict = json_string.and_then(DeserializeStringToDict);
    BookmarkCodec codec;
    if (!root_dict.has_value()) {
      OnFileLoaded(metrics::StorageFileForUma::kAccount,
                   account_encryption_type, account_storage_file_read_config,
                   root_dict.error());
    } else if (codec.Decode(*root_dict, /*already_assigned_ids=*/{},
                            account_bb_node.get(),
                            account_other_folder_node.get(),
                            account_mobile_folder_node.get(), &max_node_id,
                            &sync_metadata_str)) {
      ids_assigned_to_account_nodes = codec.release_assigned_ids();

      // A successful decoding must have set proper IDs.
      CHECK_NE(0, account_bb_node->id());
      CHECK_NE(0, account_other_folder_node->id());
      CHECK_NE(0, account_mobile_folder_node->id());

      details->AddAccountPermanentNodes(std::move(account_bb_node),
                                        std::move(account_other_folder_node),
                                        std::move(account_mobile_folder_node));

      details->set_account_sync_metadata_str(std::move(sync_metadata_str));
      details->set_max_id(std::max(max_node_id, details->max_id()));
      details->set_ids_reassigned(details->ids_reassigned() ||
                                  codec.ids_reassigned());
      details->set_required_recovery(details->required_recovery() ||
                                     codec.required_recovery());

      // Record metrics that indicate whether or not IDs were reassigned for
      // account bookmarks.
      metrics::RecordIdsReassignedOnProfileLoad(
          metrics::StorageFileForUma::kAccount, codec.ids_reassigned());
      OnFileLoaded(metrics::StorageFileForUma::kAccount,
                   account_encryption_type, account_storage_file_read_config,
                   metrics::BookmarksFileLoadResult::kSuccess);
      if (account_storage_file_read_config.is_clear_text_fallback) {
        // We were supposed to use the encrypted file but it is missing.
        // Recreate it on a different task after model loading is complete.
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(save_account_single_file_callback),
                           StorageFileEncryptionType::kEncrypted,
                           std::move(json_string.value())));
      } else {
        const StorageFileEncryptionType secondary_account_encryption_type =
            account_encryption_type == StorageFileEncryptionType::kEncrypted
                ? StorageFileEncryptionType::kClearText
                : StorageFileEncryptionType::kEncrypted;
        MaybeScheduleReadBookmarksInSecondaryFileAndVerifyContent(
            std::move(json_string.value()), encryptor,
            secondary_account_encryption_type,
            secondary_account_encryption_type ==
                    StorageFileEncryptionType::kEncrypted
                ? encrypted_account_file_path
                : account_file_path,
            metrics::StorageFileForUma::kAccount,
            std::move(save_account_single_file_callback));
        MaybeCleanUpFiles(account_encryption_type,
                          metrics::StorageFileForUma::kAccount,
                          account_file_path);
      }
    } else {
      // In the failure case, it is still possible that sync metadata was
      // decoded, which includes legit scenarios like sync metadata indicating
      // that there were too many bookmarks in sync, server-side.
      details->set_account_sync_metadata_str(std::move(sync_metadata_str));
      OnFileLoaded(
          metrics::StorageFileForUma::kAccount, account_encryption_type,
          account_storage_file_read_config,
          metrics::BookmarksFileLoadResult::kBookmarkCodecDecodingFailed);
    }
  }

  // Decode local-or-syncable bookmarks.
  {
    const StorageFileReadConfig local_or_syncable_storage_file_read_config =
        DetermineStorageFileReadConfig(local_or_syncable_file_path,
                                       encrypted_local_or_syncable_file_path);
    const StorageFileEncryptionType local_or_syncable_encryption_type =
        local_or_syncable_storage_file_read_config
            .primary_source_encryption_type;
    std::string sync_metadata_str;
    int64_t max_node_id = 0;
    const base::expected<std::string, metrics::BookmarksFileLoadResult>
        json_string = ReadFile(encryptor, local_or_syncable_encryption_type,
                               local_or_syncable_encryption_type ==
                                       StorageFileEncryptionType::kEncrypted
                                   ? encrypted_local_or_syncable_file_path
                                   : local_or_syncable_file_path,
                               metrics::StorageFileForUma::kLocalOrSyncable);
    base::expected<base::DictValue, metrics::BookmarksFileLoadResult>
        root_dict = json_string.and_then(DeserializeStringToDict);
    BookmarkCodec codec;
    if (!root_dict.has_value()) {
      OnFileLoaded(metrics::StorageFileForUma::kLocalOrSyncable,
                   local_or_syncable_encryption_type,
                   local_or_syncable_storage_file_read_config,
                   root_dict.error());
    } else if (codec.Decode(*root_dict,
                            std::move(ids_assigned_to_account_nodes),
                            details->bb_node(), details->other_folder_node(),
                            details->mobile_folder_node(), &max_node_id,
                            &sync_metadata_str)) {
      details->set_local_or_syncable_sync_metadata_str(
          std::move(sync_metadata_str));
      details->set_max_id(std::max(max_node_id, details->max_id()));
      details->set_ids_reassigned(details->ids_reassigned() ||
                                  codec.ids_reassigned());
      details->set_required_recovery(details->required_recovery() ||
                                     codec.required_recovery());
      details->set_local_or_syncable_reassigned_ids_per_old_id(
          codec.release_reassigned_ids_per_old_id());

      // Record metrics that indicate whether or not IDs were reassigned for
      // local-or-syncable bookmarks.
      metrics::RecordIdsReassignedOnProfileLoad(
          metrics::StorageFileForUma::kLocalOrSyncable, codec.ids_reassigned());
      OnFileLoaded(metrics::StorageFileForUma::kLocalOrSyncable,
                   local_or_syncable_encryption_type,
                   local_or_syncable_storage_file_read_config,
                   metrics::BookmarksFileLoadResult::kSuccess);
      if (local_or_syncable_storage_file_read_config.is_clear_text_fallback) {
        // We were supposed to use the encrypted file but it is missing.
        // Recreate it on a different task after model loading is complete.
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                std::move(save_local_or_syncable_single_file_callback),
                StorageFileEncryptionType::kEncrypted,
                std::move(json_string.value())));
      } else {
        const StorageFileEncryptionType
            secondary_local_or_syncable_encryption_type =
                local_or_syncable_encryption_type ==
                        StorageFileEncryptionType::kEncrypted
                    ? StorageFileEncryptionType::kClearText
                    : StorageFileEncryptionType::kEncrypted;
        MaybeScheduleReadBookmarksInSecondaryFileAndVerifyContent(
            std::move(json_string.value()), encryptor,
            secondary_local_or_syncable_encryption_type,
            secondary_local_or_syncable_encryption_type ==
                    StorageFileEncryptionType::kEncrypted
                ? encrypted_local_or_syncable_file_path
                : local_or_syncable_file_path,
            metrics::StorageFileForUma::kLocalOrSyncable,
            std::move(save_local_or_syncable_single_file_callback));
        MaybeCleanUpFiles(local_or_syncable_encryption_type,
                          metrics::StorageFileForUma::kLocalOrSyncable,
                          local_or_syncable_file_path);
      }
    } else {
      OnFileLoaded(
          metrics::StorageFileForUma::kLocalOrSyncable,
          local_or_syncable_encryption_type,
          local_or_syncable_storage_file_read_config,
          metrics::BookmarksFileLoadResult::kBookmarkCodecDecodingFailed);
    }
  }

  return details;
}

void LoadManagedNode(LoadManagedNodeCallback load_managed_node_callback,
                     BookmarkLoadDetails& details) {
  if (!load_managed_node_callback) {
    return;
  }

  int64_t max_node_id = details.max_id();
  std::unique_ptr<BookmarkPermanentNode> managed_node =
      std::move(load_managed_node_callback).Run(&max_node_id);
  if (managed_node) {
    details.AddManagedNode(std::move(managed_node));
    details.set_max_id(std::max(max_node_id, details.max_id()));
  }
}

uint64_t GetFileSizeOrZero(const base::FilePath& file_path) {
  return base::GetFileSize(file_path).value_or(0);
}

void RecordLoadMetrics(
    BookmarkLoadDetails* details,
    const base::FilePath& local_or_syncable_file_path,
    const base::FilePath& encrypted_local_or_syncable_file_path,
    const base::FilePath& account_file_path,
    const base::FilePath& encrypted_account_file_path) {
  UrlLoadStats url_stats = details->url_index()->ComputeStats();
  metrics::RecordUrlLoadStatsOnProfileLoad(url_stats);

  UserFolderLoadStats user_folder_stats = details->ComputeUserFolderStats();
  metrics::RecordUserFolderLoadStatsOnProfileLoad(user_folder_stats);

  // Only record clear text file size metrics when clear text file is used.
  if (!ShouldUseEncryptedBookmarksAsPrimarySource() ||
      ShouldVerifyBookmarksDataInSecondaryFileOnLoad()) {
    const uint64_t local_or_syncable_file_size =
        GetFileSizeOrZero(local_or_syncable_file_path);
    const uint64_t account_file_size =
        account_file_path.empty() ? 0U : GetFileSizeOrZero(account_file_path);

    if (local_or_syncable_file_size != 0) {
      metrics::RecordFileSizeAtStartup(StorageFileEncryptionType::kClearText,
                                       local_or_syncable_file_size);
    }

    if (account_file_size != 0) {
      metrics::RecordFileSizeAtStartup(StorageFileEncryptionType::kClearText,
                                       account_file_size);
    }

    metrics::RecordAverageNodeSizeAtStartupIfNonZero(
        StorageFileEncryptionType::kClearText,
        url_stats.total_url_bookmark_count,
        local_or_syncable_file_size + account_file_size);
  }

  // Only record encrypted file size metrics when encrypted file is used.
  if (ShouldUseEncryptedBookmarksAsPrimarySource() ||
      ShouldVerifyBookmarksDataInSecondaryFileOnLoad()) {
    const uint64_t encrypted_local_or_syncable_file_size =
        GetFileSizeOrZero(encrypted_local_or_syncable_file_path);
    if (encrypted_local_or_syncable_file_size != 0) {
      metrics::RecordFileSizeAtStartup(StorageFileEncryptionType::kEncrypted,
                                       encrypted_local_or_syncable_file_size);
    }

    const uint64_t encrypted_account_file_size =
        GetFileSizeOrZero(encrypted_account_file_path);
    if (encrypted_account_file_size != 0) {
      metrics::RecordFileSizeAtStartup(StorageFileEncryptionType::kEncrypted,
                                       encrypted_account_file_size);
    }

    metrics::RecordAverageNodeSizeAtStartupIfNonZero(
        StorageFileEncryptionType::kEncrypted,
        url_stats.total_url_bookmark_count,
        encrypted_local_or_syncable_file_size + encrypted_account_file_size);
  }
}

}  // namespace

// static
scoped_refptr<ModelLoader> ModelLoader::Create(
    scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
        encryptor,
    const base::FilePath& local_or_syncable_file_path,
    const base::FilePath& encrypted_local_or_syncable_file_path,
    const base::FilePath& account_file_path,
    const base::FilePath& encrypted_account_file_path,
    LoadManagedNodeCallback load_managed_node_callback,
    SaveSingleFileCallback save_local_or_syncable_single_file_callback,
    SaveSingleFileCallback save_account_single_file_callback,
    LoadCallback callback) {
  CHECK(!local_or_syncable_file_path.empty());
  // Note: base::MakeRefCounted is not available here, as ModelLoader's
  // constructor is private.
  auto model_loader = base::WrapRefCounted(new ModelLoader());
  model_loader->backend_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  auto save_local_or_syncable_single_file_callback_on_main_sequence =
      base::BindPostTaskToCurrentDefault(
          std::move(save_local_or_syncable_single_file_callback));
  auto save_account_single_file_callback_on_main_sequence =
      base::BindPostTaskToCurrentDefault(
          std::move(save_account_single_file_callback));

  model_loader->backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ModelLoader::DoLoadOnBackgroundThread, model_loader, encryptor,
          local_or_syncable_file_path, encrypted_local_or_syncable_file_path,
          account_file_path, encrypted_account_file_path,
          std::move(
              save_local_or_syncable_single_file_callback_on_main_sequence),
          std::move(save_account_single_file_callback_on_main_sequence),
          std::move(load_managed_node_callback)),
      std::move(callback));
  return model_loader;
}

void ModelLoader::BlockTillLoaded() {
  loaded_signal_.Wait();
}

ModelLoader::ModelLoader()
    : loaded_signal_(base::WaitableEvent::ResetPolicy::MANUAL,
                     base::WaitableEvent::InitialState::NOT_SIGNALED) {}

ModelLoader::~ModelLoader() = default;

std::unique_ptr<BookmarkLoadDetails> ModelLoader::DoLoadOnBackgroundThread(
    const scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
        encryptor,
    const base::FilePath& local_or_syncable_file_path,
    const base::FilePath& encrypted_local_or_syncable_file_path,
    const base::FilePath& account_file_path,
    const base::FilePath& encrypted_account_file_path,
    SaveSingleFileCallback save_local_or_syncable_single_file_callback,
    SaveSingleFileCallback save_account_single_file_callback,
    LoadManagedNodeCallback load_managed_node_callback) {
  std::unique_ptr<BookmarkLoadDetails> details =
      LoadBookmarks(encryptor, local_or_syncable_file_path,
                    encrypted_local_or_syncable_file_path, account_file_path,
                    encrypted_account_file_path,
                    std::move(save_local_or_syncable_single_file_callback),
                    std::move(save_account_single_file_callback));
  CHECK(details);

  details->PopulateNodeIdsForLocalOrSyncablePermanentNodes();

  LoadManagedNode(std::move(load_managed_node_callback), *details);

  // Building the indices can take a while so it's done on the background
  // thread.
  details->CreateIndices();

  RecordLoadMetrics(details.get(), local_or_syncable_file_path,
                    encrypted_local_or_syncable_file_path, account_file_path,
                    encrypted_account_file_path);

  history_bookmark_model_ = details->url_index();
  loaded_signal_.Signal();
  return details;
}

// static
scoped_refptr<ModelLoader> ModelLoader::CreateForTest(
    LoadManagedNodeCallback load_managed_node_callback,
    BookmarkLoadDetails* details) {
  CHECK(details);
  details->PopulateNodeIdsForLocalOrSyncablePermanentNodes();
  LoadManagedNode(std::move(load_managed_node_callback), *details);
  details->CreateIndices();

  // Note: base::MakeRefCounted is not available here, as ModelLoader's
  // constructor is private.
  auto model_loader = base::WrapRefCounted(new ModelLoader());
  model_loader->history_bookmark_model_ = details->url_index();
  model_loader->loaded_signal_.Signal();
  return model_loader;
}

}  // namespace bookmarks
