// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/database/download_db_entry.h"

namespace download {

DownloadDBEntry::DownloadDBEntry() = default;

DownloadDBEntry::DownloadDBEntry(const DownloadDBEntry& other) = default;

DownloadDBEntry::~DownloadDBEntry() = default;

bool DownloadDBEntry::operator==(const DownloadDBEntry& other) const {
  return download_info == other.download_info;
}

std::string DownloadDBEntry::GetGuid() const {
  if (!download_info)
    return std::string();
  return download_info->guid;
}

bool DownloadDBEntry::operator!=(const DownloadDBEntry& other) const {
  return !(*this == other);
}

}  // namespace download
