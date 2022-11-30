// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_EDGE_IMPORTER_WIN_H_
#define CHROME_UTILITY_IMPORTER_EDGE_IMPORTER_WIN_H_

#include <stdint.h>

#include <vector>

#include "base/files/file_path.h"
#include "chrome/utility/importer/importer.h"
#include "components/favicon_base/favicon_usage_data.h"

struct ImportedBookmarkEntry;

class EdgeImporter : public Importer {
 public:
  EdgeImporter();

  EdgeImporter(const EdgeImporter&) = delete;
  EdgeImporter& operator=(const EdgeImporter&) = delete;

  // Importer:
  void StartImport(const importer::SourceProfile& source_profile,
                   uint16_t items,
                   ImporterBridge* bridge) override;

 private:
  ~EdgeImporter() override;

  void ImportFavorites();
  // This function will read the favorites from the spartan database storing
  // the bookmark items in |bookmarks| and favicon information in |favicons|.
  void ParseFavoritesDatabase(std::vector<ImportedBookmarkEntry>* bookmarks,
                              favicon_base::FaviconUsageDataList* favicons);

  // Edge does not have source path. It's used in unit tests only for providing
  // a fake source for the spartan database location.
  base::FilePath source_path_;
};

#endif  // CHROME_UTILITY_IMPORTER_EDGE_IMPORTER_WIN_H_
