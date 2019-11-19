// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_storage.h"

#include <stddef.h>
#include <algorithm>
#include <unordered_map>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_codec.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/titled_url_index.h"
#include "components/bookmarks/browser/url_index.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

using base::TimeTicks;

namespace bookmarks {

namespace {

// Extension used for backup files (copy of main file created during startup).
const base::FilePath::CharType kBackupExtension[] = FILE_PATH_LITERAL("bak");

// How often we save.
const int kSaveDelayMS = 2500;

void BackupCallback(const base::FilePath& path) {
  base::FilePath backup_path = path.ReplaceExtension(kBackupExtension);
  base::CopyFile(path, backup_path);
}

// Adds node to the model's index, recursing through all children as well.
void AddBookmarksToIndex(BookmarkLoadDetails* details,
                         BookmarkNode* node) {
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

// Computes the number of bookmarks with an empty title. This includes folders
// too except for the root.
int GetNumNodesWithEmptyTitle(const BookmarkNode* node) {
  DCHECK(node);

  int num_nodes_with_empty_title = 0;

  if (!node->is_root() && node->GetTitle().empty())
    ++num_nodes_with_empty_title;

  for (const auto& child : node->children())
    num_nodes_with_empty_title += GetNumNodesWithEmptyTitle(child.get());

  return num_nodes_with_empty_title;
}

}  // namespace

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
      TimeTicks start_time = TimeTicks::Now();
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
      details->set_model_sync_transaction_version(
          codec.model_sync_transaction_version());
      UMA_HISTOGRAM_TIMES("Bookmarks.DecodeTime",
                          TimeTicks::Now() - start_time);
      int64_t size = 0;
      if (base::GetFileSize(path, &size)) {
        int64_t size_kb = size / 1024;
        // For 0 bookmarks, file size is 700 bytes (less than 1KB)
        // Bookmarks file size is not expected to exceed 50000KB (50MB) for most
        // of the users.
        UMA_HISTOGRAM_CUSTOM_COUNTS("Bookmarks.FileSize", size_kb, 1, 50000,
                                    25);
      }

      load_index = true;
    }
  }

  if (details->LoadManagedNode())
    load_index = true;

  // Load any extra root nodes now, after the IDs have been potentially
  // reassigned.
  if (load_index) {
    TimeTicks start_time = TimeTicks::Now();
    AddBookmarksToIndex(details, details->root_node());
    UMA_HISTOGRAM_TIMES("Bookmarks.CreateBookmarkIndexTime",
                        TimeTicks::Now() - start_time);
  }

  details->CreateUrlIndex();

  UMA_HISTOGRAM_COUNTS_100000(
      "Bookmarks.Count.OnProfileLoad",
      base::saturated_cast<int>(details->url_index()->UrlCount()));

  if (emit_experimental_uma && details->root_node()) {
    TimeTicks start_time = TimeTicks::Now();

    int num_duplicate_urls = GetNumDuplicateUrls(details->root_node());
    if (num_duplicate_urls > 0) {
      base::UmaHistogramCounts10000(
          "Bookmarks.Count.OnProfileLoad.DuplicateUrl", num_duplicate_urls);
    }

    int num_nodes_with_empty_title =
        GetNumNodesWithEmptyTitle(details->root_node());
    if (num_nodes_with_empty_title > 0) {
      base::UmaHistogramCounts10000("Bookmarks.Count.OnProfileLoad.EmptyTitle",
                                    num_nodes_with_empty_title);
    }

    UMA_HISTOGRAM_TIMES("Bookmarks.DuplicateAndEmptyTitleDetectionTime",
                        TimeTicks::Now() - start_time);
  }
}

// BookmarkLoadDetails ---------------------------------------------------------

BookmarkLoadDetails::BookmarkLoadDetails(BookmarkClient* client)
    : load_managed_node_callback_(client->GetLoadManagedNodeCallback()),
      index_(std::make_unique<TitledUrlIndex>()),
      model_sync_transaction_version_(
          BookmarkNode::kInvalidSyncTransactionVersion) {
  // WARNING: do NOT add |client| as a member. Much of this code runs on another
  // thread, and |client_| is not thread safe, and/or may be destroyed before
  // this.
  root_node_ = std::make_unique<BookmarkNode>(
      /*id=*/0, BookmarkNode::RootNodeGuid(), GURL());
  root_node_ptr_ = root_node_.get();
  // WARNING: order is important here, various places assume the order is
  // constant (but can vary between embedders with the initial visibility
  // of permanent nodes).
  bb_node_ = CreatePermanentNode(client, BookmarkNode::BOOKMARK_BAR);
  other_folder_node_ = CreatePermanentNode(client, BookmarkNode::OTHER_NODE);
  mobile_folder_node_ = CreatePermanentNode(client, BookmarkNode::MOBILE);
}

BookmarkLoadDetails::~BookmarkLoadDetails() {
}

bool BookmarkLoadDetails::LoadManagedNode() {
  if (!load_managed_node_callback_)
    return false;

  std::unique_ptr<BookmarkPermanentNode> managed_node =
      std::move(load_managed_node_callback_).Run(&max_id_);
  if (!managed_node)
    return false;
  bool has_children = !managed_node->children().empty();
  root_node_->Add(std::move(managed_node));
  return has_children;
}

void BookmarkLoadDetails::CreateUrlIndex() {
  url_index_ = base::MakeRefCounted<UrlIndex>(std::move(root_node_));
}

BookmarkPermanentNode* BookmarkLoadDetails::CreatePermanentNode(
    BookmarkClient* client,
    BookmarkNode::Type type) {
  DCHECK(type == BookmarkNode::BOOKMARK_BAR ||
         type == BookmarkNode::OTHER_NODE || type == BookmarkNode::MOBILE);
  std::unique_ptr<BookmarkPermanentNode> node =
      std::make_unique<BookmarkPermanentNode>(max_id_++, type);
  node->set_visible(client->IsPermanentNodeVisible(node.get()));

  int title_id;
  switch (type) {
    case BookmarkNode::BOOKMARK_BAR:
      title_id = IDS_BOOKMARK_BAR_FOLDER_NAME;
      break;
    case BookmarkNode::OTHER_NODE:
      title_id = IDS_BOOKMARK_BAR_OTHER_FOLDER_NAME;
      break;
    case BookmarkNode::MOBILE:
      title_id = IDS_BOOKMARK_BAR_MOBILE_FOLDER_NAME;
      break;
    default:
      NOTREACHED();
      title_id = IDS_BOOKMARK_BAR_FOLDER_NAME;
      break;
  }
  node->SetTitle(l10n_util::GetStringUTF16(title_id));
  BookmarkPermanentNode* permanent_node = node.get();
  root_node_->Add(std::move(node));
  return permanent_node;
}

// BookmarkStorage -------------------------------------------------------------

BookmarkStorage::BookmarkStorage(
    BookmarkModel* model,
    const base::FilePath& profile_path,
    base::SequencedTaskRunner* sequenced_task_runner)
    : model_(model),
      writer_(profile_path.Append(kBookmarksFileName),
              sequenced_task_runner,
              base::TimeDelta::FromMilliseconds(kSaveDelayMS),
              "BookmarkStorage"),
      sequenced_task_runner_(sequenced_task_runner) {}

BookmarkStorage::~BookmarkStorage() {
  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();
}

void BookmarkStorage::ScheduleSave() {
  switch (backup_state_) {
    case BACKUP_NONE:
      backup_state_ = BACKUP_DISPATCHED;
      sequenced_task_runner_->PostTaskAndReply(
          FROM_HERE, base::BindOnce(&BackupCallback, writer_.path()),
          base::BindOnce(&BookmarkStorage::OnBackupFinished,
                         weak_factory_.GetWeakPtr()));
      return;
    case BACKUP_DISPATCHED:
      // Currently doing a backup which will call this function when done.
      return;
    case BACKUP_ATTEMPTED:
      writer_.ScheduleWrite(this);
      return;
  }
  NOTREACHED();
}

void BookmarkStorage::OnBackupFinished() {
  backup_state_ = BACKUP_ATTEMPTED;
  ScheduleSave();
}

void BookmarkStorage::BookmarkModelDeleted() {
  // We need to save now as otherwise by the time SaveNow is invoked
  // the model is gone.
  if (writer_.HasPendingWrite())
    SaveNow();
  model_ = nullptr;
}

bool BookmarkStorage::SerializeData(std::string* output) {
  BookmarkCodec codec;
  std::unique_ptr<base::Value> value(
      codec.Encode(model_, model_->client()->EncodeBookmarkSyncMetadata()));
  JSONStringValueSerializer serializer(output);
  serializer.set_pretty_print(true);
  return serializer.Serialize(*(value.get()));
}

bool BookmarkStorage::SaveNow() {
  if (!model_ || !model_->loaded()) {
    // We should only get here if we have a valid model and it's finished
    // loading.
    NOTREACHED();
    return false;
  }

  std::unique_ptr<std::string> data(new std::string);
  if (!SerializeData(data.get()))
    return false;
  writer_.WriteNow(std::move(data));
  return true;
}

}  // namespace bookmarks
