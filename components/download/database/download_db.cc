// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/database/download_db.h"

#include "base/functional/callback.h"
#include "components/download/database/download_db_entry.h"

namespace download {

DownloadDB::DownloadDB() = default;

DownloadDB::~DownloadDB() = default;

void DownloadDB::Initialize(DownloadDBCallback callback) {
  std::move(callback).Run(true);
}

void DownloadDB::AddOrReplace(const DownloadDBEntry& entry) {}

void DownloadDB::AddOrReplaceEntries(const std::vector<DownloadDBEntry>& entry,
                                     DownloadDBCallback callback) {
  std::move(callback).Run(true);
}

void DownloadDB::LoadEntries(LoadEntriesCallback callback) {
  std::move(callback).Run(true,
                          std::make_unique<std::vector<DownloadDBEntry>>());
}

void DownloadDB::Remove(const std::string& guid) {}

}  // namespace download
