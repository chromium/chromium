// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/model_loader.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
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

// Adds node to the model's index, recursing through all children as well.
void AddBookmarksToIndex(BookmarkLoadDetails* details, BookmarkNode* node) {
  if (node->is_url()) {
    if (node->url().is_valid())
      details->index()->Add(node);
  } else {
    details->index()->AddPath(node);
    for (const auto& child : node->children())
      AddBookmarksToIndex(details, child.get());
  }
}

// Loads the bookmarks. This is intended to be called on the background thread.
// Updates state in |details| based on the load.
void LoadBookmarks(const base::FilePath& path,
                   BookmarkLoadDetails* details) {
  bool load_index = false;
  bool bookmark_file_exists = base::PathExists(path);
  if (bookmark_file_exists) {
    // Titles may end up containing invalid utf and we shouldn't throw away
    // all bookmarks if some titles have invalid utf.
    JSONFileValueDeserializer deserializer(
        path, base::JSON_REPLACE_INVALID_CHARACTERS);
    std::unique_ptr<base::Value> root_value =
        deserializer.Deserialize(nullptr, nullptr);

    if (!root_value) {
      // The bookmark file exists but was not deserialized.
    } else if (const auto* root_dict = root_value->GetIfDict()) {
      // Building the index can take a while, so we do it on the background
      // thread.
      int64_t max_node_id = 0;
      std::string sync_metadata_str;
      BookmarkCodec codec;
      codec.Decode(*root_dict, details->bb_node(), details->other_folder_node(),
                   details->mobile_folder_node(), &max_node_id,
                   &sync_metadata_str);
      details->set_sync_metadata_str(std::move(sync_metadata_str));
      details->set_max_id(std::max(max_node_id, details->max_id()));
      details->set_computed_checksum(codec.computed_checksum());
      details->set_stored_checksum(codec.stored_checksum());
      details->set_ids_reassigned(codec.ids_reassigned());
      details->set_uuids_reassigned(codec.uuids_reassigned());
      details->set_model_meta_info_map(codec.model_meta_info_map());
      details->set_model_unsynced_meta_info_map(
          codec.model_unsynced_meta_info_map());

      load_index = true;
    }
  }

  if (details->LoadManagedNode())
    load_index = true;

  // Load any extra root nodes now, after the IDs have been potentially
  // reassigned.
  if (load_index) {
    AddBookmarksToIndex(details, details->root_node());
  }

  details->CreateUrlIndex();

  UrlLoadStats stats = details->url_index()->ComputeStats();
  metrics::RecordUrlLoadStatsOnProfileLoad(stats);

  int64_t file_size_bytes;
  if (bookmark_file_exists && base::GetFileSize(path, &file_size_bytes)) {
    metrics::RecordFileSizeAtStartup(file_size_bytes);
    metrics::RecordAverageNodeSizeAtStartup(
        stats.total_url_bookmark_count == 0
            ? 0
            : file_size_bytes / stats.total_url_bookmark_count);
  }
}

}  // namespace

// static
scoped_refptr<ModelLoader> ModelLoader::Create(
    const base::FilePath& file_path,
    std::unique_ptr<BookmarkLoadDetails> details,
    LoadCallback callback) {
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
                     file_path, std::move(details)),
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
    const base::FilePath& file_path,
    std::unique_ptr<BookmarkLoadDetails> details) {
  LoadBookmarks(file_path, details.get());
  history_bookmark_model_ = details->url_index();
  loaded_signal_.Signal();
  return details;
}

}  // namespace bookmarks
