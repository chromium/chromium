// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/model_loader.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_codec.h"
#include "components/bookmarks/browser/bookmark_load_details.h"
#include "components/bookmarks/browser/titled_url_index.h"
#include "components/bookmarks/browser/url_index.h"

namespace bookmarks {

namespace {

// TODO(mastiz): Remove this kill switch asap since the UMA metrics entail
// negligible risks for stability or performance overhead.
const base::Feature kEmitExperimentalBookmarkLoadUma{
    "EmitExperimentalBookmarkLoadUma", base::FEATURE_ENABLED_BY_DEFAULT};

// Adds node to the model's index, recursing through all children as well.
void AddBookmarksToIndex(BookmarkLoadDetails* details, BookmarkNode* node) {
  if (node->is_url()) {
    if (node->url().is_valid())
      details->index()->Add(node);
  } else {
    for (const auto& child : node->children())
      AddBookmarksToIndex(details, child.get());
  }
}

// Helper function to recursively traverse the bookmark tree and count the
// number of bookmarks (excluding folders) per URL (more precisely, per URL
// hash).
void PopulateNumNodesPerUrlHash(
    const BookmarkNode* node,
    std::unordered_map<size_t, int>* num_nodes_per_url_hash) {
  DCHECK(num_nodes_per_url_hash);
  DCHECK(node);

  if (!node->is_folder())
    (*num_nodes_per_url_hash)[std::hash<std::string>()(node->url().spec())]++;

  for (const auto& child : node->children())
    PopulateNumNodesPerUrlHash(child.get(), num_nodes_per_url_hash);
}

// Computes the number of bookmarks (excluding folders) with a URL that is used
// by at least one other bookmark.
int GetNumDuplicateUrls(const BookmarkNode* root) {
  DCHECK(root);

  // The key is hash of the URL, instead of the full URL, to keep memory usage
  // low. The value indicates the node count.
  std::unordered_map<size_t, int> num_nodes_per_url_hash;
  PopulateNumNodesPerUrlHash(root, &num_nodes_per_url_hash);

  int num_duplicate_urls = 0;
  for (const auto& url_hash_and_count : num_nodes_per_url_hash) {
    if (url_hash_and_count.second > 1)
      num_duplicate_urls += url_hash_and_count.second;
  }
  return num_duplicate_urls;
}

// Loads the bookmarks. This is intended to be called on the background thread.
// Updates state in |details| based on the load. |emit_experimental_uma|
// determines whether a few newly introduced and experimental UMA metrics should
// be logged.
void LoadBookmarks(const base::FilePath& path,
                   bool emit_experimental_uma,
                   BookmarkLoadDetails* details) {
  bool load_index = false;
  bool bookmark_file_exists = base::PathExists(path);
  if (bookmark_file_exists) {
    // Titles may end up containing invalid utf and we shouldn't throw away
    // all bookmarks if some titles have invalid utf.
    JSONFileValueDeserializer deserializer(
        path, base::JSON_REPLACE_INVALID_CHARACTERS);
    std::unique_ptr<base::Value> root =
        deserializer.Deserialize(nullptr, nullptr);

    if (root) {
      // Building the index can take a while, so we do it on the background
      // thread.
      int64_t max_node_id = 0;
      std::string sync_metadata_str;
      BookmarkCodec codec;
      codec.Decode(*root, details->bb_node(), details->other_folder_node(),
                   details->mobile_folder_node(), &max_node_id,
                   &sync_metadata_str);
      details->set_sync_metadata_str(std::move(sync_metadata_str));
      details->set_max_id(std::max(max_node_id, details->max_id()));
      details->set_computed_checksum(codec.computed_checksum());
      details->set_stored_checksum(codec.stored_checksum());
      details->set_ids_reassigned(codec.ids_reassigned());
      details->set_guids_reassigned(codec.guids_reassigned());
      details->set_model_meta_info_map(codec.model_meta_info_map());

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

  base::UmaHistogramCounts100000(
      "Bookmarks.Count.OnProfileLoad",
      base::saturated_cast<int>(details->url_index()->UrlCount()));

  if (emit_experimental_uma && details->root_node()) {
    const int num_duplicate_urls = GetNumDuplicateUrls(details->root_node());
    if (num_duplicate_urls > 0) {
      base::UmaHistogramCounts10000(
          "Bookmarks.Count.OnProfileLoad.DuplicateUrl", num_duplicate_urls);
    }
  }
}

}  // namespace

// static
scoped_refptr<ModelLoader> ModelLoader::Create(
    const base::FilePath& profile_path,
    std::unique_ptr<BookmarkLoadDetails> details,
    LoadCallback callback) {
  // Note: base::MakeRefCounted is not available here, as ModelLoader's
  // constructor is private.
  auto model_loader = base::WrapRefCounted(new ModelLoader());
  model_loader->backend_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  // We plumb the value for kEmitExperimentalBookmarkLoadUma as retrieved on
  // the UI thread to avoid issues with TSAN bots (in case there are tests that
  // override feature toggles -not necessarily this one- while bookmark loading
  // is ongoing, which is problematic due to how feature overriding for tests is
  // implemented).
  model_loader->backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ModelLoader::DoLoadOnBackgroundThread, model_loader, profile_path,
          base::FeatureList::IsEnabled(kEmitExperimentalBookmarkLoadUma),
          std::move(details)),
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
    const base::FilePath& profile_path,
    bool emit_experimental_uma,
    std::unique_ptr<BookmarkLoadDetails> details) {
  LoadBookmarks(profile_path, emit_experimental_uma, details.get());
  history_bookmark_model_ = details->url_index();
  loaded_signal_.Signal();
  return details;
}

}  // namespace bookmarks
