// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_VFS_METRICS_UTIL_H_
#define COMPONENTS_SQLITE_VFS_METRICS_UTIL_H_

#include <optional>
#include <string>
#include <string_view>

namespace sqlite_vfs {

enum class Client;
enum class FileType;

// Returns the name of a histogram of the form:
// "SandboxedVfs{,.DbFile,.JournalFile,.WalJournalFile}.metric.client".
std::string GetHistogramName(Client client,
                             std::string_view metric,
                             std::optional<FileType> file_type = std::nullopt);

}  // namespace sqlite_vfs

#endif  // COMPONENTS_SQLITE_VFS_METRICS_UTIL_H_
