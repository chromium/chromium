// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_vfs/metrics_util.h"

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "components/sqlite_vfs/client.h"
#include "components/sqlite_vfs/file_type.h"

namespace sqlite_vfs {

namespace {

std::string_view FileTypeToTag(FileType file_type) {
  switch (file_type) {
    case FileType::kMainDb:
      return "DbFile.";
    case FileType::kMainJournal:
      return "JournalFile.";
    case FileType::kWal:
      return "WalJournalFile.";
    default:
      NOTREACHED();
  }
}

std::string_view ClientToTag(Client client) {
  switch (client) {
    case Client::kCodeCache:
      return ".CodeCache";
    case Client::kShaderCache:
      return ".ShaderCache";
    case Client::kTest:
      return ".Test";
  }
}

}  // namespace

std::string GetHistogramName(Client client,
                             std::string_view metric,
                             std::optional<FileType> file_type) {
  return base::StrCat(
      {"SandboxedVfs.",
       file_type.has_value() ? FileTypeToTag(*file_type) : std::string_view(),
       metric, ClientToTag(client)});
}

}  // namespace sqlite_vfs
