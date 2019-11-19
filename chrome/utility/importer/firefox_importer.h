// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_FIREFOX_IMPORTER_H_
#define CHROME_UTILITY_IMPORTER_FIREFOX_IMPORTER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/utility/importer/importer.h"
#include "components/favicon_base/favicon_usage_data.h"

class GURL;

namespace sql {
class Database;
}

// Importer for Mozilla Firefox 3 and later.
// Firefox stores its persistent information in a system called places.
// http://wiki.mozilla.org/Places
class FirefoxImporter : public Importer {
 public:
  FirefoxImporter();

  // Importer:
  void StartImport(const importer::SourceProfile& source_profile,
                   uint16_t items,
                   ImporterBridge* bridge) override;

 private:
  // Location of favicons in Firefox profile. It may vary depending on Firefox
  // version.
  enum class FaviconsLocation {
    // Favicons are stored in places.sqlite database (older Firefox versions).
    kPlacesDatabase,

    // Favicons are stored in favicons.sqlite (Firefox 55 and newer).
    kFaviconsDatabase,
  };

  using FaviconMap = std::map<int64_t, std::set<GURL>>;

  ~FirefoxImporter() override;

  FRIEND_TEST_ALL_PREFIXES(FirefoxImporterTest, ImportBookmarksV25);
  void ImportBookmarks();
  void ImportPasswords();
  void ImportHistory();
  void ImportSearchEngines();
  // Import the user's home page, unless it is set to default home page as
  // defined in browserconfig.properties.
  void ImportHomepage();
  void ImportAutofillFormData();
  void GetSearchEnginesXMLData(std::vector<std::string>* search_engine_data);
  void GetSearchEnginesXMLDataFromJSON(
      std::vector<std::string>* search_engine_data);

  // The struct stores the information about a bookmark item.
  struct BookmarkItem;
  using BookmarkList = std::vector<std::unique_ptr<BookmarkItem>>;

  // Gets the specific ID of bookmark node with given GUID from |db|.
  // Returns -1 if not found.
  int LoadNodeIDByGUID(sql::Database* db, const std::string& GUID);

  // Loads all livemark IDs from database |db|.
  void LoadLivemarkIDs(sql::Database* db, std::set<int>* livemark);

  // Gets the bookmark folder with given ID, and adds the entry in |list|
  // if successful.
  void GetTopBookmarkFolder(sql::Database* db,
                            int folder_id,
                            BookmarkList* list);

  // Loads all children of the given folder, and appends them to the |list|.
  void GetWholeBookmarkFolder(sql::Database* db,
                              BookmarkList* list,
                              size_t position,
                              FaviconsLocation favicons_location,
                              bool* empty_folder);

  // Loads the favicons given in the map from places.sqlite database, loads the
  // data, and converts it into FaviconUsageData structures.
  // This function supports older Firefox profiles (up to version 54).
  void LoadFavicons(sql::Database* db,
                    const FaviconMap& favicon_map,
                    favicon_base::FaviconUsageDataList* favicons);

  // Loads the favicons for |bookmarks| from favicons.sqlite database, loads the
  // data, and converts it into FaviconUsageData structures.
  // This function supports newer Firefox profiles (Firefox 55 and later).
  void LoadFavicons(const std::vector<ImportedBookmarkEntry>& bookmarks,
                    favicon_base::FaviconUsageDataList* favicons);

  // Copies |source_path_|/|base_file_name| to a temporary directory and returns
  // the copy's path. Using the copy is safer, ensures we don't modify Firefox's
  // profile. |base_file_name| must be ASCII. Returns empty path on I/O failure.
  base::FilePath GetCopiedSourcePath(base::StringPiece base_file_name);

  base::FilePath source_path_;
  base::FilePath app_path_;
  base::ScopedTempDir source_path_copy_;

#if defined(OS_POSIX)
  // Stored because we can only access it from the UI thread.
  std::string locale_;
#endif

  DISALLOW_COPY_AND_ASSIGN(FirefoxImporter);
};

#endif  // CHROME_UTILITY_IMPORTER_FIREFOX_IMPORTER_H_
