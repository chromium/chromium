// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_DB_ENTRY_H_
#define COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_DB_ENTRY_H_

#include <string>

#include "components/download/database/download_info.h"
#include "components/download/database/download_namespace.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace download {

// Representing one entry in the DownloadDB.
struct DownloadDBEntry {
 public:
  DownloadDBEntry();
  DownloadDBEntry(const DownloadDBEntry& other);
  ~DownloadDBEntry();

  bool operator==(const DownloadDBEntry& other) const;
  bool operator!=(const DownloadDBEntry& other) const;

  // Gets a unique ID for this entry.
  std::string GetGuid() const;

  // Information about a regular download.
  absl::optional<DownloadInfo> download_info;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_DB_ENTRY_H_
