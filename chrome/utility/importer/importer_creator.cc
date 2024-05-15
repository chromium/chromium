// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/importer_creator.h"

#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/utility/importer/bookmarks_file_importer.h"
#include "chrome/utility/importer/firefox_importer.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/common/importer/edge_importer_utils_win.h"
#include "chrome/utility/importer/edge_importer_win.h"
#include "chrome/utility/importer/ie_importer_win.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#include "chrome/utility/importer/safari_importer.h"
#endif

namespace importer {

scoped_refptr<Importer> CreateImporterByType(ImporterType type) {
  switch (type) {
#if BUILDFLAG(IS_WIN)
    case TYPE_IE:
      return new IEImporter();
    case TYPE_EDGE:
      // If legacy mode we pass back an IE importer.
      if (IsEdgeFavoritesLegacyMode())
        return new IEImporter();
      return new EdgeImporter();
#endif
    case TYPE_BOOKMARKS_FILE:
      return new BookmarksFileImporter();
#if !BUILDFLAG(IS_CHROMEOS)
    case TYPE_FIREFOX:
      return new FirefoxImporter();
#endif
#if BUILDFLAG(IS_MAC)
    case TYPE_SAFARI:
      return new SafariImporter(base::apple::GetUserLibraryPath());
#endif
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

}  // namespace importer
