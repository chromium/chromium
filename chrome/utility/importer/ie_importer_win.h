// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_IE_IMPORTER_WIN_H_
#define CHROME_UTILITY_IMPORTER_IE_IMPORTER_WIN_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "chrome/utility/importer/importer.h"
#include "components/favicon_base/favicon_usage_data.h"

struct ImportedBookmarkEntry;

class IEImporter : public Importer {
 public:
  IEImporter();

  IEImporter(const IEImporter&) = delete;
  IEImporter& operator=(const IEImporter&) = delete;

  // Importer:
  void StartImport(const importer::SourceProfile& source_profile,
                   uint16_t items,
                   ImporterBridge* bridge) override;

 private:
  typedef std::vector<ImportedBookmarkEntry> BookmarkVector;

  // A struct that hosts the information of IE Favorite folder.
  struct FavoritesInfo {
    base::FilePath path;
    std::u16string links_folder;
  };

  // IE PStore subkey GUID: AutoComplete password & form data.
  static const GUID kPStoreAutocompleteGUID;

  // A fake GUID for unit test.
  static const GUID kUnittestGUID;

  FRIEND_TEST_ALL_PREFIXES(ImporterTest, IEImporter);

  ~IEImporter() override;

  void ImportFavorites();

  // Reads history information from COM interface.
  void ImportHistory();

  void ImportSearchEngines();

  // Import the homepage setting of IE. Note: IE supports multiple home pages,
  // whereas Chrome doesn't, so we import only the one defined under the
  // 'Start Page' registry key. We don't import if the homepage is set to the
  // machine default.
  void ImportHomepage();

  // Gets the information of Favorites folder. Returns true if successful.
  bool GetFavoritesInfo(FavoritesInfo* info);

  // This function will read the files in the Favorites folder, and store
  // the bookmark items in |bookmarks| and favicon information in |favicons|.
  void ParseFavoritesFolder(const FavoritesInfo& info,
                            BookmarkVector* bookmarks,
                            favicon_base::FaviconUsageDataList* favicons);

  // Set to true when importing favorites from old Edge on Windows 10.
  bool edge_import_mode_;

  // IE does not have source path. It's used in unit tests only for providing a
  // fake source and it's used if importing old Edge favorites on Windows 10.
  base::FilePath source_path_;
};

#endif  // CHROME_UTILITY_IMPORTER_IE_IMPORTER_WIN_H_
