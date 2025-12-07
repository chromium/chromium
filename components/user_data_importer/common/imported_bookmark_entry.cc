// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/common/imported_bookmark_entry.h"

namespace user_data_importer {

ImportedBookmarkEntry::ImportedBookmarkEntry()
    : in_toolbar(false), is_folder(false) {}

ImportedBookmarkEntry::ImportedBookmarkEntry(
    const ImportedBookmarkEntry& other) = default;

ImportedBookmarkEntry::~ImportedBookmarkEntry() = default;

bool ImportedBookmarkEntry::operator==(
    const ImportedBookmarkEntry& other) const = default;

}  // namespace user_data_importer
