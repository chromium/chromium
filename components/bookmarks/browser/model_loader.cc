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

// Loads the bookmarks from a file determined by `file_path`. This is intended
// to be called on the background thread. Returns the loaded
// BookmarkLoadDetails.
std::unique_ptr<BookmarkLoadDetails> LoadBookmarksFromFile(
    const base::FilePath& file_path) {
  auto details = std::make_unique<BookmarkLoadDetails>();

  std::optional<base::Value::Dict> root_dict = LoadFileToDict(file_path);
  if (!root_dict.has_value()) {
    return details;
  }

  int64_t max_node_id = 0;
  std::string sync_metadata_str;
  BookmarkCodec codec;
  codec.Decode(*root_dict, details->bb_node(), details->other_folder_node(),
               details->mobile_folder_node(), &max_node_id, &sync_metadata_str);
  details->set_local_or_syncable_sync_metadata_str(
      std::move(sync_metadata_str));
  details->set_max_id(std::max(max_node_id, details->max_id()));
  details->set_ids_reassigned(codec.ids_reassigned());
  details->set_required_recovery(codec.required_recovery());

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

void RecordLoadMetrics(const BookmarkLoadDetails& details,
                       const base::FilePath& file_path) {
  UrlLoadStats stats = details.url_index()->ComputeStats();
  metrics::RecordUrlLoadStatsOnProfileLoad(stats);

  int64_t file_size_bytes;
  if (base::GetFileSize(file_path, &file_size_bytes)) {
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
    LoadManagedNodeCallback load_managed_node_callback,
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
                     file_path, std::move(load_managed_node_callback)),
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
    LoadManagedNodeCallback load_managed_node_callback) {
  std::unique_ptr<BookmarkLoadDetails> details =
      LoadBookmarksFromFile(file_path);
  CHECK(details);

  LoadManagedNode(std::move(load_managed_node_callback), *details);

  // Building the indices can take a while so it's done on the background
  // thread.
  details->CreateIndices();

  RecordLoadMetrics(*details, file_path);

  history_bookmark_model_ = details->url_index();
  loaded_signal_.Signal();
  return details;
}

// static
scoped_refptr<ModelLoader> ModelLoader::CreateForTest(
    LoadManagedNodeCallback load_managed_node_callback,
    BookmarkLoadDetails* details) {
  CHECK(details);
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
