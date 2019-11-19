// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/offline_page_model_utils.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string16.h"
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
  else if (name_space == kSuggestedArticlesNamespace)
    return OfflinePagesNamespaceEnumeration::SUGGESTED_ARTICLES;
  else if (name_space == kBrowserActionsNamespace)
    return OfflinePagesNamespaceEnumeration::BROWSER_ACTIONS;
  else if (name_space == kLivePageSharingNamespace)
    return OfflinePagesNamespaceEnumeration::LIVE_PAGE_SHARING;
  else if (name_space == kAutoAsyncNamespace)
    return OfflinePagesNamespaceEnumeration::ASYNC_AUTO_LOADING;

  NOTREACHED();
  return OfflinePagesNamespaceEnumeration::DEFAULT;
}

std::string AddHistogramSuffix(const std::string& name_space,
                               const char* histogram_name) {
  if (name_space.empty()) {
    NOTREACHED();
    return histogram_name;
  }
  std::string adjusted_histogram_name(histogram_name);
  adjusted_histogram_name += ".";
  adjusted_histogram_name += name_space;
  return adjusted_histogram_name;
}

base::FilePath GenerateUniqueFilenameForOfflinePage(
    const base::string16& title,
    const GURL& url,
    const base::FilePath& target_dir) {
  std::string kMHTMLMimeType = "multipart/related";

  // Get the suggested file name based on title and url.
  base::FilePath suggested_path =
      target_dir.Append(filename_generation::GenerateFilename(
          title, url, false /* can_save_as_complete */, kMHTMLMimeType));

  // Find a unique name based on |suggested_path|.
  int uniquifier = base::GetUniquePathNumber(suggested_path);
  base::FilePath::StringType suffix;
  if (uniquifier > 0)
#if defined(OS_WIN)
    suffix = base::StringPrintf(L" (%d)", uniquifier);
#else   // defined(OS_WIN)
    suffix = base::StringPrintf(" (%d)", uniquifier);
#endif  // defined(OS_WIN)

  // Truncation.
  int max_path_component_length =
      base::GetMaximumPathComponentLength(target_dir);
  if (max_path_component_length != -1) {
    int limit = max_path_component_length -
                suggested_path.Extension().length() - suffix.length();
    if (limit <= 0 ||
        !filename_generation::TruncateFilename(&suggested_path, limit))
      return base::FilePath();
  }

  // Adding uniquifier suffix if needed.
  if (uniquifier > 0)
    suggested_path = suggested_path.InsertBeforeExtension(suffix);

  return suggested_path;
}

}  // namespace model_utils

}  // namespace offline_pages
