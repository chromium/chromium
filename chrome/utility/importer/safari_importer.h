// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_SAFARI_IMPORTER_H_
#define CHROME_UTILITY_IMPORTER_SAFARI_IMPORTER_H_

#include <stdint.h>

#include <map>
#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "chrome/common/importer/importer_url_row.h"
#include "chrome/utility/importer/importer.h"
#include "components/favicon_base/favicon_usage_data.h"

#if __OBJC__
@class NSDictionary;
@class NSString;
#else
class NSDictionary;
class NSString;
#endif

class GURL;
struct ImportedBookmarkEntry;

namespace sql {
class Database;
}

// Importer for Safari on OS X.
class SafariImporter : public Importer {
 public:
  // |library_dir| is the full path to the ~/Library directory,
  // We pass it in as a parameter for testing purposes.
  explicit SafariImporter(const base::FilePath& library_dir);

  SafariImporter(const SafariImporter&) = delete;
  SafariImporter& operator=(const SafariImporter&) = delete;

  // Importer:
  void StartImport(const importer::SourceProfile& source_profile,
                   uint16_t items,
                   ImporterBridge* bridge) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SafariImporterTest, BookmarkImport);
  FRIEND_TEST_ALL_PREFIXES(SafariImporterTest,
                           BookmarkImportWithEmptyBookmarksMenu);
  FRIEND_TEST_ALL_PREFIXES(SafariImporterTest, FaviconImport);
  FRIEND_TEST_ALL_PREFIXES(SafariImporterTest, HistoryImport);

  ~SafariImporter() override;

  // Multiple URLs can share the same favicon; this is a map
  // of URLs -> IconIDs that we load as a temporary step before
  // actually loading the icons.
  using FaviconMap = std::map<int64_t, std::set<GURL>>;

  void ImportBookmarks();

  // Parse Safari's stored bookmarks.
  void ParseBookmarks(const std::u16string& toolbar_name,
                      std::vector<ImportedBookmarkEntry>* bookmarks);

  // Function to recursively read Bookmarks out of Safari plist.
  // |bookmark_folder| The dictionary containing a folder to parse.
  // |parent_path_elements| Path elements up to this point.
  // |is_in_toolbar| Is this folder in the toolbar.
  // |out_bookmarks| BookMark element array to write into.
  void RecursiveReadBookmarksFolder(
      NSDictionary* bookmark_folder,
      const std::vector<std::u16string>& parent_path_elements,
      bool is_in_toolbar,
      const std::u16string& toolbar_name,
      std::vector<ImportedBookmarkEntry>* out_bookmarks);

  // Opens the favicon database file.
  bool OpenDatabase(sql::Database* db);

  // Loads the urls associated with the favicons into favicon_map;
  void ImportFaviconURLs(sql::Database* db, FaviconMap* favicon_map);

  // Loads and reencodes the individual favicons.
  void LoadFaviconData(sql::Database* db,
                       const FaviconMap& favicon_map,
                       favicon_base::FaviconUsageDataList* favicons);

  base::FilePath library_dir_;
};

#endif  // CHROME_UTILITY_IMPORTER_SAFARI_IMPORTER_H_
