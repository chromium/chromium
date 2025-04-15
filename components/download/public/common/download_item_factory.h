// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The DownloadItemFactory is used to produce different DownloadItems.
// It is separate from the DownloadManager to allow download manager
// unit tests to control the items produced.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_FACTORY_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_FACTORY_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_job.h"
#include "url/origin.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace download {

struct DownloadCreateInfo;
class DownloadItemImpl;
class DownloadItemImplDelegate;

class COMPONENTS_DOWNLOAD_EXPORT DownloadItemFactory {
 public:
  virtual ~DownloadItemFactory();

  virtual DownloadItemImpl* CreatePersistedItem(
      DownloadItemImplDelegate* delegate,
      const std::string& guid,
      uint32_t download_id,
      const base::FilePath& current_path,
      const base::FilePath& target_path,
      const std::vector<GURL>& url_chain,
      const GURL& referrer_url,
      const std::string& serialized_embedder_data,
      const GURL& tab_url,
      const GURL& tab_refererr_url,
      const std::optional<url::Origin>& initiator_origin,
      const std::string& mime_type,
      const std::string& original_mime_type,
      base::Time start_time,
      base::Time end_time,
      const std::string& etag,
      const std::string& last_modified,
      int64_t received_bytes,
      int64_t total_bytes,
      const std::string& hash,
      DownloadItem::DownloadState state,
      DownloadDangerType danger_type,
      DownloadInterruptReason interrupt_reason,
      bool opened,
      base::Time last_access_time,
      bool transient,
      const std::vector<DownloadItem::ReceivedSlice>& received_slices) = 0;

  virtual DownloadItemImpl* CreateActiveItem(
      DownloadItemImplDelegate* delegate,
      uint32_t download_id,
      const DownloadCreateInfo& info) = 0;

  virtual DownloadItemImpl* CreateSavePageItem(
      DownloadItemImplDelegate* delegate,
      uint32_t download_id,
      const base::FilePath& path,
      const GURL& url,
      const std::string& mime_type,
      download::DownloadJob::CancelRequestCallback cancel_request_callback) = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_FACTORY_H_
