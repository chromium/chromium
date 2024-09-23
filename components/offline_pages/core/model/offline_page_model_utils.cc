// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/offline_page_model_utils.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/filename_generation/filename_generation.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_item.h"

namespace offline_pages {

namespace model_utils {

OfflinePagesNamespaceEnumeration ToNamespaceEnum(
    const std::string& name_space) {
  if (name_space == kDefaultNamespace)
    return OfflinePagesNamespaceEnumeration::DEFAULT;
  else if (name_space == kBookmarkNamespace)
    return OfflinePagesNamespaceEnumeration::BOOKMARK;
  else if (name_space == kLastNNamespace)
    return OfflinePagesNamespaceEnumeration::LAST_N;
  else if (name_space == kAsyncNamespace)
    return OfflinePagesNamespaceEnumeration::ASYNC_LOADING;
  else if (name_space == kCCTNamespace)
    return OfflinePagesNamespaceEnumeration::CUSTOM_TABS;
  else if (name_space == kDownloadNamespace)
    return OfflinePagesNamespaceEnumeration::DOWNLOAD;
  else if (name_space == kNTPSuggestionsNamespace)
    return OfflinePagesNamespaceEnumeration::NTP_SUGGESTION;
  else if (name_space == kBrowserActionsNamespace)
    return OfflinePagesNamespaceEnumeration::BROWSER_ACTIONS;
  else if (name_space == kLivePageSharingNamespace)
    return OfflinePagesNamespaceEnumeration::LIVE_PAGE_SHARING;
  else if (name_space == kAutoAsyncNamespace)
    return OfflinePagesNamespaceEnumeration::ASYNC_AUTO_LOADING;

  return OfflinePagesNamespaceEnumeration::DEFAULT;
}

std::string AddHistogramSuffix(const std::string& name_space,
                               const char* histogram_name) {
  if (name_space.empty()) {
    NOTREACHED_IN_MIGRATION();
    return histogram_name;
  }
  std::string adjusted_histogram_name(histogram_name);
  adjusted_histogram_name += ".";
  adjusted_histogram_name += name_space;
  return adjusted_histogram_name;
}

base::FilePath GenerateUniqueFilenameForOfflinePage(
    const std::u16string& title,
    const GURL& url,
    const base::FilePath& target_dir) {
  std::string kMHTMLMimeType = "multipart/related";

  // Get the suggested file name based on title and url.
  base::FilePath suggested_path =
      target_dir.Append(filename_generation::GenerateFilename(
          title, url, false /* can_save_as_complete */, kMHTMLMimeType));

  // Truncation based on the maximum length the suffix may have " (99)".
  const int kMaxSuffixLength = 5;
  int max_path_component_length =
      base::GetMaximumPathComponentLength(target_dir);
  if (max_path_component_length != -1) {
    int limit = max_path_component_length -
                suggested_path.Extension().length() - kMaxSuffixLength;
    if (limit <= 0 ||
        !filename_generation::TruncateFilename(&suggested_path, limit)) {
      return base::FilePath();
    }
  }

  // Find a unique name based on |suggested_path|.
  suggested_path = base::GetUniquePath(suggested_path);

  return suggested_path;
}

}  // namespace model_utils

}  // namespace offline_pages
