// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/model_loader.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/numerics/clamped_math.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/bookmarks/browser/bookmark_codec.h"
#include "components/bookmarks/browser/bookmark_load_details.h"
#include "components/bookmarks/browser/titled_url_index.h"
#include "components/bookmarks/browser/url_index.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/common/url_load_stats.h"

namespace bookmarks {

namespace {

// Loads and deserializes a JSON file determined by `file_path` and returns it
// in the form of a dictionary, or nullopt if something fails.
std::optional<base::Value::Dict> LoadFileToDict(
    const base::FilePath& file_path) {
  // Titles may end up containing invalid utf and we shouldn't throw away
  // all bookmarks if some titles have invalid utf.
  JSONFileValueDeserializer deserializer(file_path,
                                         base::JSON_REPLACE_INVALID_CHARACTERS);
  std::unique_ptr<base::Value> root = deserializer.Deserialize(
      /*error_code=*/nullptr, /*error_message=*/nullptr);
  if (!root || !root->is_dict()) {
    // The bookmark file exists but was not deserialized properly.
    return std::nullopt;
  }

  return std::make_optional(std::move(*root).TakeDict());
}

std::unique_ptr<BookmarkLoadDetails> LoadBookmarks(
    const base::FilePath& local_or_syncable_file_path,
    const base::FilePath& account_file_path) {
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
        BookmarkPermanentNode::CreateBookmarkBar(0);
    std::unique_ptr<BookmarkPermanentNode> account_other_folder_node =
        BookmarkPermanentNode::CreateOtherBookmarks(0);
    std::unique_ptr<BookmarkPermanentNode> account_mobile_folder_node =
        BookmarkPermanentNode::CreateMobileBookmarks(0);

    std::optional<base::Value::Dict> root_dict =
        LoadFileToDict(account_file_path);
    BookmarkCodec codec;
    if (root_dict.has_value() &&
        codec.Decode(*root_dict, /*already_assigned_ids=*/{},
                     account_bb_node.get(), account_other_folder_node.get(),
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
    } else {
      // In the failure case, it is still possible that sync metadata was
      // decoded, which includes legit scenarios like sync metadata indicating
      // that there were too many bookmarks in sync, server-side.
      details->set_account_sync_metadata_str(std::move(sync_metadata_str));
    }
  }

  // Decode local-or-syncable bookmarks.
  {
    std::string sync_metadata_str;
    int64_t max_node_id = 0;
    std::optional<base::Value::Dict> root_dict =
        LoadFileToDict(local_or_syncable_file_path);
    BookmarkCodec codec;
    if (root_dict.has_value() &&
        codec.Decode(*root_dict, std::move(ids_assigned_to_account_nodes),
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
  int64_t file_size_bytes = 0;
  if (base::GetFileSize(file_path, &file_size_bytes)) {
    return file_size_bytes;
  }
  return 0;
}

void RecordLoadMetrics(const BookmarkLoadDetails& details,
                       const base::FilePath& local_or_syncable_file_path,
                       const base::FilePath& account_file_path) {
  UrlLoadStats stats = details.url_index()->ComputeStats();
  metrics::RecordUrlLoadStatsOnProfileLoad(stats);

  const uint64_t local_or_syncable_file_size =
      GetFileSizeOrZero(local_or_syncable_file_path);
  const uint64_t account_file_size =
      account_file_path.empty() ? 0U : GetFileSizeOrZero(account_file_path);

  if (local_or_syncable_file_size != 0) {
    metrics::RecordFileSizeAtStartup(local_or_syncable_file_size);
  }

  if (account_file_size != 0) {
    metrics::RecordFileSizeAtStartup(account_file_size);
  }

  const uint64_t sum_file_size =
      local_or_syncable_file_size + account_file_size;
  if (sum_file_size > 0) {
    metrics::RecordAverageNodeSizeAtStartup(
        stats.total_url_bookmark_count == 0
            ? 0
            : sum_file_size / stats.total_url_bookmark_count);
  }
}

}  // namespace

// static
scoped_refptr<ModelLoader> ModelLoader::Create(
    const base::FilePath& local_or_syncable_file_path,
    const base::FilePath& account_file_path,
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
                     local_or_syncable_file_path, account_file_path,
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
    const base::FilePath& local_or_syncable_file_path,
    const base::FilePath& account_file_path,
    LoadManagedNodeCallback load_managed_node_callback) {
  std::unique_ptr<BookmarkLoadDetails> details =
      LoadBookmarks(local_or_syncable_file_path, account_file_path);
  CHECK(details);

  details->PopulateNodeIdsForLocalOrSyncablePermanentNodes();

  LoadManagedNode(std::move(load_managed_node_callback), *details);

  // Building the indices can take a while so it's done on the background
  // thread.
  details->CreateIndices();

  RecordLoadMetrics(*details, local_or_syncable_file_path, account_file_path);

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
