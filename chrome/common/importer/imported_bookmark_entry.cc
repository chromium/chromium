// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/importer/imported_bookmark_entry.h"

ImportedBookmarkEntry::ImportedBookmarkEntry()
    : in_toolbar(false),
      is_folder(false) {}

ImportedBookmarkEntry::ImportedBookmarkEntry(
    const ImportedBookmarkEntry& other) = default;

ImportedBookmarkEntry::~ImportedBookmarkEntry() {}

bool ImportedBookmarkEntry::operator==(
    const ImportedBookmarkEntry& other) const {
  return (in_toolbar == other.in_toolbar &&
          is_folder == other.is_folder &&
          url == other.url &&
          path == other.path &&
          title == other.title &&
          creation_time == other.creation_time);
}
