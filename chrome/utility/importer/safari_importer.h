// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_SAFARI_IMPORTER_H_
#define CHROME_UTILITY_IMPORTER_SAFARI_IMPORTER_H_

#include <stdint.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "chrome/utility/importer/importer.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/user_data_importer/common/importer_url_row.h"

namespace user_data_importer {
struct ImportedBookmarkEntry;
}  // namespace user_data_importer

// Importer for Safari on macOS.
class SafariImporter : public Importer {
 public:
  // |library_dir| is the full path to the ~/Library directory,
  // We pass it in as a parameter for testing purposes.
  explicit SafariImporter(const base::FilePath& library_dir);

  SafariImporter(const SafariImporter&) = delete;
  SafariImporter& operator=(const SafariImporter&) = delete;

  // Importer:
  void StartImport(const user_data_importer::SourceProfile& source_profile,
                   uint16_t items,
                   ImporterBridge* bridge) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SafariImporterTest, BookmarkImport);
  FRIEND_TEST_ALL_PREFIXES(SafariImporterTest,
                           BookmarkImportWithEmptyBookmarksMenu);
  FRIEND_TEST_ALL_PREFIXES(SafariImporterTest, HistoryImport);

  ~SafariImporter() override;

  void ImportBookmarks();

  // Parse Safari's stored bookmarks.
  void ParseBookmarks(
      const std::u16string& toolbar_name,
      std::vector<user_data_importer::ImportedBookmarkEntry>* bookmarks);

  base::FilePath library_dir_;
};

#endif  // CHROME_UTILITY_IMPORTER_SAFARI_IMPORTER_H_
