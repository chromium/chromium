// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/importer/safari_importer_utils.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/common/importer/importer_data_types.h"

bool SafariImporterCanImport(const base::FilePath& library_dir,
                             uint16_t* services_supported) {
  DCHECK(services_supported);
  *services_supported = importer::NONE;

  // Only support the importing of bookmarks from Safari, if there is access to
  // the bookmarks storage file.

  // As for history import, this code used to support that, dependent on the
  // existence of the "History.plist" file. Long before macOS 10.10, Safari
  // switched to a database for the history file, and no one noticed that the
  // history importing broke. Given the file access restrictions in macOS 10.14
  // (https://crbug.com/850225) it's probably not worth fixing it, and so it was
  // removed.

  base::FilePath safari_dir = library_dir.Append("Safari");
  base::FilePath bookmarks_path = safari_dir.Append("Bookmarks.plist");

  if (base::PathIsReadable(bookmarks_path))
    *services_supported |= importer::FAVORITES;

  return *services_supported != importer::NONE;
}
