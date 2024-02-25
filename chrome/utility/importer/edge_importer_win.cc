// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/edge_importer_win.h"

#include <Shlobj.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/win/windows_version.h"
#include "chrome/common/importer/edge_importer_utils_win.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/utility/importer/edge_database_reader_win.h"
#include "chrome/utility/importer/favicon_reencode.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

// Toolbar favorites are placed under this special folder name.
const char16_t kFavoritesBarTitle[] = u"_Favorites_Bar_";
const wchar_t kSpartanDatabaseFile[] = L"spartan.edb";

struct EdgeFavoriteEntry {
  EdgeFavoriteEntry()
      : is_folder(false),
        order_number(0),
        item_id(GUID_NULL),
        parent_id(GUID_NULL) {}

  std::u16string title;
  GURL url;
  base::FilePath favicon_file;
  bool is_folder;
  int64_t order_number;
  base::Time date_updated;
  GUID item_id;
  GUID parent_id;

  std::vector<raw_ptr<const EdgeFavoriteEntry, VectorExperimental>> children;

  ImportedBookmarkEntry ToBookmarkEntry(
      bool in_toolbar,
      const std::vector<std::u16string>& path) const {
    ImportedBookmarkEntry entry;
    entry.in_toolbar = in_toolbar;
    entry.is_folder = is_folder;
    entry.url = url;
    entry.path = path;
    entry.title = title;
    entry.creation_time = date_updated;
    return entry;
  }
};

struct EdgeFavoriteEntryComparator {
  bool operator()(const EdgeFavoriteEntry* lhs,
                  const EdgeFavoriteEntry* rhs) const {
    return std::tie(lhs->order_number, lhs->title) <
           std::tie(rhs->order_number, rhs->title);
  }
};

// The name of the database file is spartan.edb, however it isn't clear how
// the intermediate path between the DataStore and the database is generated.
// Therefore we just do a simple recursive search until we find a matching name.
base::FilePath FindSpartanDatabase(const base::FilePath& profile_path) {
  base::FilePath data_path =
      profile_path.empty() ? importer::GetEdgeDataFilePath() : profile_path;
  if (data_path.empty())
    return base::FilePath();

  base::FileEnumerator enumerator(data_path.Append(L"DataStore\\Data"), true,
                                  base::FileEnumerator::FILES);
  base::FilePath path = enumerator.Next();
  while (!path.empty()) {
    if (base::EqualsCaseInsensitiveASCII(path.BaseName().value(),
                                         kSpartanDatabaseFile))
      return path;
    path = enumerator.Next();
  }
  return base::FilePath();
}

struct GuidComparator {
  bool operator()(const GUID& a, const GUID& b) const {
    return memcmp(&a, &b, sizeof(a)) < 0;
  }
};

bool ReadFaviconData(const base::FilePath& file,
                     std::vector<unsigned char>* data) {
  std::string image_data;
  if (!base::ReadFileToString(file, &image_data))
    return false;

  const unsigned char* ptr =
      reinterpret_cast<const unsigned char*>(image_data.c_str());
  return importer::ReencodeFavicon(ptr, image_data.size(), data);
}

void BuildBookmarkEntries(const EdgeFavoriteEntry& current_entry,
                          bool is_toolbar,
                          std::vector<ImportedBookmarkEntry>* bookmarks,
                          favicon_base::FaviconUsageDataList* favicons,
                          std::vector<std::u16string>* path) {
  for (const EdgeFavoriteEntry* entry : current_entry.children) {
    if (entry->is_folder) {
      // If the favorites bar then load all children as toolbar items.
      if (base::EqualsCaseInsensitiveASCII(entry->title, kFavoritesBarTitle)) {
        // Replace name with Links similar to IE.
        path->push_back(u"Links");
        BuildBookmarkEntries(*entry, true, bookmarks, favicons, path);
        path->pop_back();
      } else {
        path->push_back(entry->title);
        BuildBookmarkEntries(*entry, is_toolbar, bookmarks, favicons, path);
        path->pop_back();
      }
    } else {
      bookmarks->push_back(entry->ToBookmarkEntry(is_toolbar, *path));
      favicon_base::FaviconUsageData favicon;
      if (entry->url.is_valid() && !entry->favicon_file.empty() &&
          ReadFaviconData(entry->favicon_file, &favicon.png_data)) {
        // As the database doesn't provide us a favicon URL we'll fake one.
        GURL::Replacements path_replace;
        path_replace.SetPathStr("/favicon.ico");
        favicon.favicon_url =
            entry->url.GetWithEmptyPath().ReplaceComponents(path_replace);
        favicon.urls.insert(entry->url);
        favicons->push_back(favicon);
      }
    }
  }
}

}  // namespace

EdgeImporter::EdgeImporter() {}

void EdgeImporter::StartImport(const importer::SourceProfile& source_profile,
                               uint16_t items,
                               ImporterBridge* bridge) {
  bridge_ = bridge;
  bridge_->NotifyStarted();
  source_path_ = source_profile.source_path;

  if ((items & importer::FAVORITES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::FAVORITES);
    ImportFavorites();
    bridge_->NotifyItemEnded(importer::FAVORITES);
  }
  bridge_->NotifyEnded();
}

EdgeImporter::~EdgeImporter() {}

void EdgeImporter::ImportFavorites() {
  std::vector<ImportedBookmarkEntry> bookmarks;
  favicon_base::FaviconUsageDataList favicons;
  ParseFavoritesDatabase(&bookmarks, &favicons);

  if (!bookmarks.empty() && !cancelled()) {
    const std::u16string& first_folder_name =
        l10n_util::GetStringUTF16(IDS_BOOKMARK_GROUP_FROM_EDGE);
    bridge_->AddBookmarks(bookmarks, first_folder_name);
  }
  if (!favicons.empty() && !cancelled())
    bridge_->SetFavicons(favicons);
}

// From Edge 13 (released with Windows 10 TH2), Favorites are stored in a JET
// database within the Edge local storage. The import uses the ESE library to
// open and read the data file. The data is stored in a Favorites table with
// the following schema.
// Column Name          Column Type
// ------------------------------------------
// DateUpdated          LongLong - FILETIME
// FaviconFile          LongText - Relative path
// HashedUrl            ULong
// IsDeleted            Bit
// IsFolder             Bit
// ItemId               Guid
// OrderNumber          LongLong
// ParentId             Guid
// RoamDisabled         Bit
// RowId                Long
// Title                LongText
// URL                  LongText
void EdgeImporter::ParseFavoritesDatabase(
    std::vector<ImportedBookmarkEntry>* bookmarks,
    favicon_base::FaviconUsageDataList* favicons) {
  base::FilePath database_path = FindSpartanDatabase(source_path_);
  if (database_path.empty())
    return;

  base::FilePath log_folder = database_path.DirName().Append(L"LogFiles");

  EdgeDatabaseReader database;

  // If the log file directory does not exist, don't set the log_folder
  // attribute, as the open database operation will fail in such cases.
  // The log folder will usually not be present when running the unit tests.
  if (base::PathExists(log_folder))
    database.set_log_folder(log_folder);
  if (!database.OpenDatabase(database_path)) {
    DVLOG(1) << "Error opening database " << database.GetErrorMessage();
    return;
  }

  std::unique_ptr<EdgeDatabaseTableEnumerator> enumerator =
      database.OpenTableEnumerator(L"Favorites");
  if (!enumerator) {
    DVLOG(1) << "Error opening database table " << database.GetErrorMessage();
    return;
  }

  if (!enumerator->Reset())
    return;

  std::map<GUID, EdgeFavoriteEntry, GuidComparator> database_entries;
  base::FilePath favicon_base =
      source_path_.empty() ? importer::GetEdgeDataFilePath() : source_path_;
  favicon_base = favicon_base.Append(L"DataStore");

  do {
    EdgeFavoriteEntry entry;
    bool is_deleted = false;
    if (!enumerator->RetrieveColumn(L"IsDeleted", &is_deleted))
      continue;
    if (is_deleted)
      continue;
    if (!enumerator->RetrieveColumn(L"IsFolder", &entry.is_folder))
      continue;
    std::u16string url;
    if (!enumerator->RetrieveColumn(L"URL", &url))
      continue;
    entry.url = GURL(url);
    if (!entry.is_folder && !entry.url.is_valid())
      continue;
    if (!enumerator->RetrieveColumn(L"Title", &entry.title))
      continue;
    std::u16string favicon_file;
    if (!enumerator->RetrieveColumn(L"FaviconFile", &favicon_file))
      continue;
    if (!favicon_file.empty()) {
      entry.favicon_file =
          favicon_base.Append(base::FilePath::FromUTF16Unsafe(favicon_file));
    }
    if (!enumerator->RetrieveColumn(L"ParentId", &entry.parent_id))
      continue;
    if (!enumerator->RetrieveColumn(L"ItemId", &entry.item_id))
      continue;
    if (!enumerator->RetrieveColumn(L"OrderNumber", &entry.order_number))
      continue;
    FILETIME data_updated;
    if (!enumerator->RetrieveColumn(L"DateUpdated", &data_updated))
      continue;
    entry.date_updated = base::Time::FromFileTime(data_updated);
    database_entries[entry.item_id] = entry;
  } while (enumerator->Next() && !cancelled());

  // Build simple tree.
  EdgeFavoriteEntry root_entry;
  for (auto& entry : database_entries) {
    auto found_parent = database_entries.find(entry.second.parent_id);
    if (found_parent == database_entries.end() ||
        !found_parent->second.is_folder) {
      root_entry.children.push_back(&entry.second);
    } else {
      found_parent->second.children.push_back(&entry.second);
    }
  }
  // With tree built sort the children of each node including the root.
  std::sort(root_entry.children.begin(), root_entry.children.end(),
            EdgeFavoriteEntryComparator());
  for (auto& entry : database_entries) {
    std::sort(entry.second.children.begin(), entry.second.children.end(),
              EdgeFavoriteEntryComparator());
  }
  std::vector<std::u16string> path;
  BuildBookmarkEntries(root_entry, false, bookmarks, favicons, &path);
}
