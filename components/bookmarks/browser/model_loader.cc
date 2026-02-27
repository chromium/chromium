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
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/bookmarks/browser/bookmark_codec.h"
#include "components/bookmarks/browser/bookmark_load_details.h"
#include "components/bookmarks/browser/titled_url_index.h"
#include "components/bookmarks/browser/url_index.h"
#include "components/bookmarks/common/bookmark_features.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/common/url_load_stats.h"
#include "components/bookmarks/common/user_folder_load_stats.h"
#include "components/os_crypt/async/common/encryptor.h"

namespace bookmarks {

namespace {

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

  if (!encryptor) {
    metrics::RecordTimeToReadFile(storage_file_for_uma,
                                  metrics::EncryptionTypeForUma::kClearText,
                                  base::TimeTicks::Now() - start_time);
    return json_string;
  }

  std::string decrypted_json_string;
  if (!encryptor->data.DecryptString(json_string, &decrypted_json_string)) {
    return base::unexpected(
        metrics::BookmarksFileLoadResult::kDecryptionFailed);
  }
  metrics::RecordTimeToReadFile(storage_file_for_uma,
                                metrics::EncryptionTypeForUma::kEncrypted,
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

void ReadEncryptedDataAndVerifyHashOnBackgroundSequence(
    size_t json_string_hash,
    scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
        encryptor,
    const base::FilePath encrypted_file_path,
    metrics::StorageFileForUma storage_file_for_uma) {
  base::expected<std::string, metrics::BookmarksFileLoadResult>
      encrypted_json_string =
          ReadFile(encryptor, encrypted_file_path, storage_file_for_uma);
  metrics::RecordBookmarksFileLoadResult(
      storage_file_for_uma, metrics::EncryptionTypeForUma::kEncrypted,
      encrypted_json_string.error_or(
          metrics::BookmarksFileLoadResult::kSuccess));
  // TODO(crbug.com/435317726): If comparison isn't successful, we
  // should save the encrypted file to disk again.
  if (encrypted_json_string.has_value()) {
    const size_t encrypted_json_string_hash =
        base::FastHash(encrypted_json_string.value());
    metrics::RecordEncryptedBookmarksFileMatchesResult(
        storage_file_for_uma, encrypted_json_string_hash == json_string_hash);
  }
}

void MaybeScheduleReadEncryptedDataAndVerifyHash(
    std::string_view json_string,
    const scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
        encryptor,
    const base::FilePath& encrypted_file_path,
    metrics::StorageFileForUma storage_file_for_umage) {
  if (!ShouldVerifyEncryptedBookmarksDataOnLoad()) {
    return;
  }
  CHECK(encryptor);
  CHECK(!encrypted_file_path.empty());
  const size_t json_string_hash = base::FastHash(json_string);
  // Validate the encrypted data on a different task in order not to impact the
  // bookmarks load time.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ReadEncryptedDataAndVerifyHashOnBackgroundSequence,
                     json_string_hash, encryptor, encrypted_file_path,
                     storage_file_for_umage));
}

std::unique_ptr<BookmarkLoadDetails> LoadBookmarks(
    const scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
        encryptor,
    const base::FilePath& local_or_syncable_file_path,
    const base::FilePath& encrypted_local_or_syncable_file_path,
    const base::FilePath& account_file_path,
    const base::FilePath& encrypted_account_file_path) {
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

    std::unique_ptr<BookmarkPermanentNode> account_bb_node =
        BookmarkPermanentNode::CreateBookmarkBar(0, /*is_account_node=*/true);
    std::unique_ptr<BookmarkPermanentNode> account_other_folder_node =
        BookmarkPermanentNode::CreateOtherBookmarks(0,
                                                    /*is_account_node=*/true);
    std::unique_ptr<BookmarkPermanentNode> account_mobile_folder_node =
        BookmarkPermanentNode::CreateMobileBookmarks(0,
                                                     /*is_account_node=*/true);

    const base::expected<std::string, metrics::BookmarksFileLoadResult>
        json_string = ReadFile(/*encryptor=*/nullptr, account_file_path,
                               metrics::StorageFileForUma::kAccount);
    base::expected<base::DictValue, metrics::BookmarksFileLoadResult>
        root_dict = json_string.and_then(DeserializeStringToDict);
    BookmarkCodec codec;
    if (!root_dict.has_value()) {
      metrics::RecordBookmarksFileLoadResult(
          metrics::StorageFileForUma::kAccount,
          metrics::EncryptionTypeForUma::kClearText, root_dict.error());
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
      metrics::RecordBookmarksFileLoadResult(
          metrics::StorageFileForUma::kAccount,
          metrics::EncryptionTypeForUma::kClearText,
          metrics::BookmarksFileLoadResult::kSuccess);
      MaybeScheduleReadEncryptedDataAndVerifyHash(
          json_string.value(), encryptor, encrypted_account_file_path,
          metrics::StorageFileForUma::kAccount);
    } else {
      // In the failure case, it is still possible that sync metadata was
      // decoded, which includes legit scenarios like sync metadata indicating
      // that there were too many bookmarks in sync, server-side.
      details->set_account_sync_metadata_str(std::move(sync_metadata_str));
      metrics::RecordBookmarksFileLoadResult(
          metrics::StorageFileForUma::kAccount,
          metrics::EncryptionTypeForUma::kClearText,
          metrics::BookmarksFileLoadResult::kBookmarkCodecDecodingFailed);
    }
  }

  // Decode local-or-syncable bookmarks.
  {
    std::string sync_metadata_str;
    int64_t max_node_id = 0;
    const base::expected<std::string, metrics::BookmarksFileLoadResult>
        json_string =
            ReadFile(/*encryptor=*/nullptr, local_or_syncable_file_path,
                     metrics::StorageFileForUma::kLocalOrSyncable);
    base::expected<base::DictValue, metrics::BookmarksFileLoadResult>
        root_dict = json_string.and_then(DeserializeStringToDict);
    BookmarkCodec codec;
    if (!root_dict.has_value()) {
      metrics::RecordBookmarksFileLoadResult(
          metrics::StorageFileForUma::kLocalOrSyncable,
          metrics::EncryptionTypeForUma::kClearText, root_dict.error());
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
      metrics::RecordBookmarksFileLoadResult(
          metrics::StorageFileForUma::kLocalOrSyncable,
          metrics::EncryptionTypeForUma::kClearText,
          metrics::BookmarksFileLoadResult::kSuccess);
      MaybeScheduleReadEncryptedDataAndVerifyHash(
          json_string.value(), encryptor, encrypted_local_or_syncable_file_path,
          metrics::StorageFileForUma::kLocalOrSyncable);
    } else {
      metrics::RecordBookmarksFileLoadResult(
          metrics::StorageFileForUma::kLocalOrSyncable,
          metrics::EncryptionTypeForUma::kClearText,
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

  const uint64_t local_or_syncable_file_size =
      GetFileSizeOrZero(local_or_syncable_file_path);
  const uint64_t account_file_size =
      account_file_path.empty() ? 0U : GetFileSizeOrZero(account_file_path);

  if (local_or_syncable_file_size != 0) {
    metrics::RecordFileSizeAtStartup(metrics::EncryptionTypeForUma::kClearText,
                                     local_or_syncable_file_size);
  }

  if (account_file_size != 0) {
    metrics::RecordFileSizeAtStartup(metrics::EncryptionTypeForUma::kClearText,
                                     account_file_size);
  }

  if (ShouldVerifyEncryptedBookmarksDataOnLoad()) {
    const uint64_t encrypted_local_or_syncable_file_size =
        GetFileSizeOrZero(encrypted_local_or_syncable_file_path);
    if (encrypted_local_or_syncable_file_size != 0) {
      metrics::RecordFileSizeAtStartup(
          metrics::EncryptionTypeForUma::kEncrypted,
          encrypted_local_or_syncable_file_size);
    }

    const uint64_t encrypted_account_file_size =
        GetFileSizeOrZero(encrypted_account_file_path);
    if (encrypted_account_file_size != 0) {
      metrics::RecordFileSizeAtStartup(
          metrics::EncryptionTypeForUma::kEncrypted,
          encrypted_account_file_size);
    }
  }

  const uint64_t sum_file_size =
      local_or_syncable_file_size + account_file_size;
  if (sum_file_size > 0) {
    metrics::RecordAverageNodeSizeAtStartup(
        url_stats.total_url_bookmark_count == 0
            ? 0
            : sum_file_size / url_stats.total_url_bookmark_count);
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
    LoadCallback callback) {
  CHECK(!local_or_syncable_file_path.empty());
  // Note: base::MakeRefCounted is not available here, as ModelLoader's
  // constructor is private.
  auto model_loader = base::WrapRefCounted(new ModelLoader());
  model_loader->backend_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  model_loader->backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ModelLoader::DoLoadOnBackgroundThread, model_loader,
                     encryptor, local_or_syncable_file_path,
                     encrypted_local_or_syncable_file_path, account_file_path,
                     encrypted_account_file_path,
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
    LoadManagedNodeCallback load_managed_node_callback) {
  std::unique_ptr<BookmarkLoadDetails> details =
      LoadBookmarks(encryptor, local_or_syncable_file_path,
                    encrypted_local_or_syncable_file_path, account_file_path,
                    encrypted_account_file_path);
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
