// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_item_impl_delegate.h"

#include "base/check_op.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item_impl.h"
#include "components/download/public/common/download_item_rename_handler.h"
#include "components/download/public/common/download_target_info.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/download/public/common/android/auto_resumption_handler.h"
#endif

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
    DownloadTargetCallback callback) {
  DownloadTargetInfo target_info;
  target_info.target_path = download->GetForcedFilePath();
  target_info.intermediate_path = download->GetForcedFilePath();

  std::move(callback).Run(std::move(target_info));
}

bool DownloadItemImplDelegate::ShouldCompleteDownload(
    DownloadItemImpl* download,
    base::OnceClosure complete_callback) {
  return true;
}

bool DownloadItemImplDelegate::ShouldOpenDownload(
    DownloadItemImpl* download,
    ShouldOpenDownloadCallback callback) {
  // TODO(qinmin): When this returns false it means this should run the callback
  // at some point.
  return false;
}

bool DownloadItemImplDelegate::ShouldAutomaticallyOpenFile(
    const GURL& url,
    const base::FilePath& path) {
  return false;
}

bool DownloadItemImplDelegate::ShouldAutomaticallyOpenFileByPolicy(
    const GURL& url,
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
    const std::string& serialized_embedder_download_data) {}

void DownloadItemImplDelegate::UpdatePersistence(DownloadItemImpl* download) {}

void DownloadItemImplDelegate::OpenDownload(DownloadItemImpl* download) {}

void DownloadItemImplDelegate::ShowDownloadInShell(DownloadItemImpl* download) {
}

void DownloadItemImplDelegate::DownloadRemoved(DownloadItemImpl* download) {}

void DownloadItemImplDelegate::DownloadInterrupted(DownloadItemImpl* download) {
}

bool DownloadItemImplDelegate::IsOffTheRecord() const {
  return false;
}

bool DownloadItemImplDelegate::IsActiveNetworkMetered() const {
#if BUILDFLAG(IS_ANDROID)
  return download::AutoResumptionHandler::Get() &&
         download::AutoResumptionHandler::Get()->IsActiveNetworkMetered();
#else
  return false;
#endif
}

void DownloadItemImplDelegate::ReportBytesWasted(DownloadItemImpl* download) {}

void DownloadItemImplDelegate::BindWakeLockProvider(
    mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) {}

QuarantineConnectionCallback
DownloadItemImplDelegate::GetQuarantineConnectionCallback() {
  return base::NullCallback();
}

std::unique_ptr<DownloadItemRenameHandler>
DownloadItemImplDelegate::GetRenameHandlerForDownload(
    DownloadItemImpl* download_item) {
  return nullptr;
}

}  // namespace download
