// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_NAMESPACE_H_
#define COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_NAMESPACE_H_

#include <string>

namespace download {

// The namespace of the download, used for classifying the download.
// Entries in this enum can only be appended instead of being deleted or reused.
enum class DownloadNamespace {
  NAMESPACE_UNKNOWN = 0,
  // Regular browser downloads, either through context menu or navigation.
  NAMESPACE_BROWSER_DOWNLOAD,
};

// Converts a namespace to its string representation.
std::string DownloadNamespaceToString(DownloadNamespace download_namespace);

// Converts a string representation of a namespace to its namespace, or
// NAMESPACE_UNKNOWN if the string doesn't map to one.
DownloadNamespace DownloadNamespaceFromString(
    const std::string& namespace_string);

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_NAMESPACE_H_
