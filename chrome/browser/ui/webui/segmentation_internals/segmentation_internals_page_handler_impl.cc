// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/segmentation_internals/segmentation_internals_page_handler_impl.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"

SegmentationInternalsPageHandlerImpl::SegmentationInternalsPageHandlerImpl(
    mojo::PendingReceiver<segmentation_internals::mojom::PageHandler> receiver,
    mojo::PendingRemote<segmentation_internals::mojom::Page> page,
    Profile* profile)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      service_proxy_(segmentation_platform::SegmentationPlatformServiceFactory::
                         GetForProfile(profile)
                             ->GetServiceProxy()) {
  if (service_proxy_)
    service_proxy_->AddObserver(this);
}

SegmentationInternalsPageHandlerImpl::~SegmentationInternalsPageHandlerImpl() {
  if (service_proxy_)
    service_proxy_->RemoveObserver(this);
}

void SegmentationInternalsPageHandlerImpl::GetSegment(
    const std::string& key,
    GetSegmentCallback callback) {
  if (!service_proxy_) {
    OnGetSelectedSegmentDone(std::move(callback),
                             segmentation_platform::SegmentSelectionResult());
    return;
  }
  service_proxy_->GetSelectedSegment(
      key, base::BindOnce(
               &SegmentationInternalsPageHandlerImpl::OnGetSelectedSegmentDone,
               weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SegmentationInternalsPageHandlerImpl::OnGetSelectedSegmentDone(
    GetSegmentCallback callback,
    const segmentation_platform::SegmentSelectionResult& result) {
  auto segment_data = segmentation_internals::mojom::SegmentData::New();
  segment_data->is_ready = result.is_ready;
  segment_data->optimization_target =
      result.segment ? result.segment.value() : -1;
  std::move(callback).Run(std::move(segment_data));
}

void SegmentationInternalsPageHandlerImpl::GetServiceStatus() {
  if (service_proxy_) {
    service_proxy_->GetServiceStatus();
  }
}

void SegmentationInternalsPageHandlerImpl::OnServiceStatusChanged(
    bool is_initialized,
    int status_flag) {
  page_->OnServiceStatusChanged(is_initialized, status_flag);
}

void SegmentationInternalsPageHandlerImpl::OnSegmentInfoAvailable(
    const std::vector<std::string>& segment_info) {
  page_->OnSegmentInfoAvailable(segment_info);
}
