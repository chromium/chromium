// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/segmentation_internals/segmentation_internals_page_handler_impl.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

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
    const std::vector<std::pair<std::string, std::string>>& segment_info) {
  std::vector<segmentation_internals::mojom::SegmentInfoPtr> available_segments;
  for (const auto& info : segment_info) {
    auto segment_data = segmentation_internals::mojom::SegmentInfo::New();
    segment_data->optimization_target = info.first;
    segment_data->segment_data = info.second;
    available_segments.push_back(std::move(segment_data));
  }
  page_->OnSegmentInfoAvailable(std::move(available_segments));
}
