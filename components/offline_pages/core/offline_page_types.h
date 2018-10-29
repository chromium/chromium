// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_TYPES_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_TYPES_H_

#include <stdint.h>
#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_thumbnail.h"

class GURL;

// This file contains common callbacks used by OfflinePageModel and is a
// temporary step to refactor and interface of the model out of the
// implementation.
namespace offline_pages {
// Result of saving a page offline. Must be kept with sync with
// OfflinePagesSavePageResult in metrics' enum.xml
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offlinepages
enum class SavePageResult {
  SUCCESS,
  CANCELLED,
  DEVICE_FULL,
  CONTENT_UNAVAILABLE,
  ARCHIVE_CREATION_FAILED,
  STORE_FAILURE,
  ALREADY_EXISTS,
  // Certain pages, i.e. file URL or NTP, will not be saved because these
  // are already locally accessible.
  SKIPPED,
  SECURITY_CERTIFICATE_ERROR,
  // Returned when we detect trying to save a chrome error page.
  ERROR_PAGE,
  // Returned when we detect trying to save a chrome interstitial page.
  INTERSTITIAL_PAGE,
  // Failed to compute digest for the archive file.
  DIGEST_CALCULATION_FAILED,
  // Unable to move the file into a public directory.
  FILE_MOVE_FAILED,
  // Unable to add the file to the system download manager.
  ADD_TO_DOWNLOAD_MANAGER_FAILED,
  // Unable to get write permission on public directory.
  PERMISSION_DENIED,
  kMaxValue = PERMISSION_DENIED,
};

// Result of adding an offline page.
enum class AddPageResult {
  SUCCESS,
  STORE_FAILURE,
  ALREADY_EXISTS,
  kMaxValue = ALREADY_EXISTS,
};

// Result of deleting an offline page. Must be kept with sync with
// OfflinePagesDeletePageResult in metrics' enum.xml.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offlinepages
enum class DeletePageResult {
  SUCCESS,
  CANCELLED,
  STORE_FAILURE,
  DEVICE_FAILURE,
  // Deprecated. Deleting pages which are not in metadata store would be
  // returing |SUCCESS|. Should not be used anymore.
  DEPRECATED_NOT_FOUND,
  kMaxValue = DEPRECATED_NOT_FOUND,
};

// The result when trying to share offline page to other apps.
enum class ShareResult {
  // Successfully shared.
  kSuccess,

  // Failed due to no file access permission.
  kFileAccessPermissionDenied,
};

typedef std::vector<int64_t> MultipleOfflineIdResult;
typedef std::vector<OfflinePageItem> MultipleOfflinePageItemResult;

typedef base::OnceCallback<void(SavePageResult, int64_t)> SavePageCallback;
typedef base::OnceCallback<void(AddPageResult, int64_t)> AddPageCallback;
typedef base::OnceCallback<void(DeletePageResult)> DeletePageCallback;
typedef base::OnceCallback<void(const MultipleOfflineIdResult&)>
    MultipleOfflineIdCallback;
typedef base::OnceCallback<void(const OfflinePageItem*)>
    SingleOfflinePageItemCallback;
typedef base::OnceCallback<void(const MultipleOfflinePageItemResult&)>
    MultipleOfflinePageItemCallback;
typedef base::RepeatingCallback<bool(const GURL&)> UrlPredicate;
typedef base::OnceCallback<void(int64_t)> SizeInBytesCallback;
typedef base::OnceCallback<void(std::unique_ptr<OfflinePageThumbnail>)>
    GetThumbnailCallback;
typedef base::OnceCallback<void(bool)> CleanupThumbnailsCallback;

// Callbacks used for publishing an offline page.
using PublishPageCallback =
    base::OnceCallback<void(const base::FilePath&, SavePageResult)>;
using UpdateFilePathDoneCallback = base::OnceCallback<void(bool)>;

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_TYPES_H_
