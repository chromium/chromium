// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_item_impl_delegate.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "components/download/public/common/auto_resumption_handler.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item_impl.h"

namespace download {

// Infrastructure in DownloadItemImplDelegate to assert invariant that
// delegate always outlives all attached DownloadItemImpls.
DownloadItemImplDelegate::DownloadItemImplDelegate() : count_(0) {}

DownloadItemImplDelegate::~DownloadItemImplDelegate() {
  DCHECK_EQ(0, count_);
}

void DownloadItemImplDelegate::Attach() {
  ++count_;
}

void DownloadItemImplDelegate::Detach() {
  DCHECK_LT(0, count_);
  --count_;
}

void DownloadItemImplDelegate::DetermineDownloadTarget(
    DownloadItemImpl* download,
    const DownloadTargetCallback& callback) {
  // TODO(rdsmith/asanka): Do something useful if forced file path is null.
  base::FilePath target_path(download->GetForcedFilePath());
  callback.Run(target_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, target_path,
               DOWNLOAD_INTERRUPT_REASON_NONE);
}

bool DownloadItemImplDelegate::ShouldCompleteDownload(
    DownloadItemImpl* download,
    const base::Closure& complete_callback) {
  return true;
}

bool DownloadItemImplDelegate::ShouldOpenDownload(
    DownloadItemImpl* download,
    const ShouldOpenDownloadCallback& callback) {
  return false;
}

bool DownloadItemImplDelegate::ShouldOpenFileBasedOnExtension(
    const base::FilePath& path) {
  return false;
}

void DownloadItemImplDelegate::CheckForFileRemoval(
    DownloadItemImpl* download_item) {}

std::string DownloadItemImplDelegate::GetApplicationClientIdForFileScanning()
    const {
  return std::string();
}

void DownloadItemImplDelegate::ResumeInterruptedDownload(
    std::unique_ptr<DownloadUrlParameters> params,
    const GURL& site_url) {}

void DownloadItemImplDelegate::UpdatePersistence(DownloadItemImpl* download) {}

void DownloadItemImplDelegate::OpenDownload(DownloadItemImpl* download) {}

bool DownloadItemImplDelegate::IsMostRecentDownloadItemAtFilePath(
    DownloadItemImpl* download) {
  return true;
}

void DownloadItemImplDelegate::ShowDownloadInShell(DownloadItemImpl* download) {
}

void DownloadItemImplDelegate::DownloadRemoved(DownloadItemImpl* download) {}

void DownloadItemImplDelegate::DownloadInterrupted(DownloadItemImpl* download) {
}

bool DownloadItemImplDelegate::IsOffTheRecord() const {
  return false;
}

bool DownloadItemImplDelegate::IsActiveNetworkMetered() const {
  return download::AutoResumptionHandler::Get()
             ? download::AutoResumptionHandler::Get()->IsActiveNetworkMetered()
             : false;
}

void DownloadItemImplDelegate::ReportBytesWasted(DownloadItemImpl* download) {}

service_manager::Connector*
DownloadItemImplDelegate::GetServiceManagerConnector() {
  return nullptr;
}

QuarantineConnectionCallback
DownloadItemImplDelegate::GetQuarantineConnectionCallback() {
  return base::NullCallback();
}

}  // namespace download
