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
    Profile* profile)
    : receiver_(this, std::move(receiver)),
      segmentation_platform_service_(
          segmentation_platform::SegmentationPlatformServiceFactory::
              GetForProfile(profile)) {}

SegmentationInternalsPageHandlerImpl::~SegmentationInternalsPageHandlerImpl() =
    default;

void SegmentationInternalsPageHandlerImpl::GetSegment(
    const std::string& key,
    GetSegmentCallback callback) {
  segmentation_platform_service_->GetSelectedSegment(
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
