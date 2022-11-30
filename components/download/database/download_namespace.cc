// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/database/download_namespace.h"

namespace download {

namespace {
const char kBrowserDownloadNamespace[] = "download";
const char kUnknownNamespace[] = "unknown";
}  // namespace

std::string DownloadNamespaceToString(DownloadNamespace download_namespace) {
  switch (download_namespace) {
    case DownloadNamespace::NAMESPACE_BROWSER_DOWNLOAD:
      return kBrowserDownloadNamespace;
    default:
      return kUnknownNamespace;
  }
}

DownloadNamespace DownloadNamespaceFromString(
    const std::string& namespace_string) {
  if (namespace_string == kBrowserDownloadNamespace)
    return DownloadNamespace::NAMESPACE_BROWSER_DOWNLOAD;
  return DownloadNamespace::NAMESPACE_UNKNOWN;
}

}  // namespace download
