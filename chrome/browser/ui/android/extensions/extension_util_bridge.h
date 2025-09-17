// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_UTIL_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_UTIL_BRIDGE_H_

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"

namespace extensions {

// Get file under downloads by file name. If it doesn't exist, return nullopt.
std::optional<base::FilePath> GetFileUnderDownloads(
    const std::string& file_name);

// Get files under Downloads. If they don't exist, create empty ones. The name
// of the given file is concatenated to the extensions to determine the names of
// the files to be queried or created. For example if the given file has the
// name "foo" and the extensions are ".crx" and ".pem", the files will have
// names "foo.crx" and "foo.pem".
std::optional<std::vector<base::FilePath>> GetOrCreateEmptyFilesUnderDownloads(
    const base::FilePath& file_for_basename,
    const std::vector<std::string>& dot_extensions);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_UTIL_BRIDGE_H_
