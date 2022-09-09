// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_BOOKMARKS_FILE_IMPORTER_H_
#define CHROME_UTILITY_IMPORTER_BOOKMARKS_FILE_IMPORTER_H_

#include <stdint.h>

#include "chrome/utility/importer/importer.h"

// Importer for bookmarks files.
class BookmarksFileImporter : public Importer {
 public:
  BookmarksFileImporter();

  BookmarksFileImporter(const BookmarksFileImporter&) = delete;
  BookmarksFileImporter& operator=(const BookmarksFileImporter&) = delete;

  void StartImport(const importer::SourceProfile& source_profile,
                   uint16_t items,
                   ImporterBridge* bridge) override;

 private:
  ~BookmarksFileImporter() override;
};

#endif  // CHROME_UTILITY_IMPORTER_BOOKMARKS_FILE_IMPORTER_H_
