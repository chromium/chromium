// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/utility/bookmark_util.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/reading_list/core/reading_list_model.h"

namespace user_data_importer {

namespace {

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

std::u16string EscapeAndJoinPath(const std::vector<std::u16string>& path) {
  if (path.empty()) {
    return u"";
  }
  std::vector<std::u16string> escaped_path;
  escaped_path.reserve(path.size());

  for (std::u16string component : path) {
    base::ReplaceChars(component, u"\\", u"\\\\", &component);
    base::ReplaceChars(component, u"/", u"\\/", &component);
    escaped_path.push_back(std::move(component));
  }
  return base::JoinString(escaped_path, u"/");
}

const BookmarkNode* CreateImportBookmarksFolder(
    BookmarkModel* bookmark_model,
    const std::u16string& folder_title) {
  CHECK(bookmark_model);
  CHECK(bookmark_model->loaded());

  const BookmarkNode* other_node = bookmark_model->account_other_node();
  if (!other_node) {
    other_node = bookmark_model->other_node();
  }

  CHECK(other_node);

  return bookmark_model->AddFolder(other_node, 0, folder_title);
}

}  // namespace

size_t ImportBookmarks(BookmarkModel* bookmark_model,
                       std::vector<ImportedBookmarkEntry> bookmarks,
                       const std::u16string& import_folder_title) {
  CHECK(bookmark_model);
  CHECK(bookmark_model->loaded());

  if (bookmarks.empty()) {
    return 0;
  }

  const BookmarkNode* import_folder =
      CreateImportBookmarksFolder(bookmark_model, import_folder_title);

  CHECK(import_folder);

  std::map<std::u16string, const BookmarkNode*> folder_cache;
  folder_cache[EscapeAndJoinPath({})] = import_folder;

  bookmark_model->BeginExtensiveChanges();

  size_t imported_count = 0u;

  for (const ImportedBookmarkEntry& bookmark_entry : bookmarks) {
    const BookmarkNode* parent = import_folder;
    std::vector<std::u16string> current_path;

    for (const auto& path_component : bookmark_entry.path) {
      current_path.push_back(path_component);
      const std::u16string current_path_key = EscapeAndJoinPath(current_path);

      auto cached_folder = folder_cache.find(current_path_key);

      // TODO(crbug.com/407587751): Replace with a CHECK.
      if (cached_folder != folder_cache.end()) {
        parent = cached_folder->second;
      } else {
        const BookmarkNode* new_folder = bookmark_model->AddFolder(
            parent, parent->children().size(), path_component);
        folder_cache[current_path_key] = new_folder;
        parent = new_folder;
      }
    }

    if (bookmark_entry.is_folder) {
      const BookmarkNode* new_folder = bookmark_model->AddFolder(
          parent, parent->children().size(), bookmark_entry.title);

      std::vector<std::u16string> new_folder_path = bookmark_entry.path;
      new_folder_path.push_back(bookmark_entry.title);
      folder_cache[EscapeAndJoinPath(new_folder_path)] = new_folder;
    } else {
      if (!bookmark_entry.url.is_valid()) {
        continue;
      }
      bookmark_model->AddURL(parent, parent->children().size(),
                             bookmark_entry.title, bookmark_entry.url, nullptr,
                             bookmark_entry.creation_time);
      ++imported_count;
    }
  }

  bookmark_model->EndExtensiveChanges();

  return imported_count;
}

size_t ImportReadingList(ReadingListModel* reading_list_model,
                         std::vector<ImportedBookmarkEntry> reading_list) {
  if (reading_list.empty() || !reading_list_model) {
    return 0;
  }

  size_t imported_count = 0u;

  auto scoped_reading_list_batch_update =
      reading_list_model->BeginBatchUpdates();

  for (const ImportedBookmarkEntry& reading_list_entry : reading_list) {
    if (!reading_list_entry.url.is_valid()) {
      continue;
    }
    reading_list_model->AddOrReplaceEntry(
        reading_list_entry.url, base::UTF16ToUTF8(reading_list_entry.title),
        reading_list::ADDED_VIA_IMPORT,
        /*estimated_read_time=*/std::nullopt, reading_list_entry.creation_time);
    ++imported_count;
  }

  return imported_count;
}

}  // namespace user_data_importer
