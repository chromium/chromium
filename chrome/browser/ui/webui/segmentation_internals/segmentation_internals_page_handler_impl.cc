// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/segmentation_internals/segmentation_internals_page_handler_impl.h"

#include "base/functional/bind.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

using segmentation_platform::proto::SegmentId;

SegmentationInternalsPageHandlerImpl::SegmentationInternalsPageHandlerImpl(
    mojo::PendingReceiver<segmentation_internals::mojom::PageHandler> receiver,
    mojo::PendingRemote<segmentation_internals::mojom::Page> page,
    segmentation_platform::SegmentationPlatformService* segmentation_service)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      service_proxy_(segmentation_service->GetServiceProxy()) {
  if (service_proxy_)
    service_proxy_->AddObserver(this);
}

SegmentationInternalsPageHandlerImpl::~SegmentationInternalsPageHandlerImpl() {
  if (service_proxy_)
    service_proxy_->RemoveObserver(this);
}

void SegmentationInternalsPageHandlerImpl::GetServiceStatus() {
  if (!service_proxy_)
    return;
  service_proxy_->GetServiceStatus();
}

void SegmentationInternalsPageHandlerImpl::ExecuteModel(int segment_id) {
  if (!service_proxy_)
    return;
  service_proxy_->ExecuteModel(static_cast<SegmentId>(segment_id));
}

void SegmentationInternalsPageHandlerImpl::OverwriteResult(int segment_id,
                                                           float result) {
  if (!service_proxy_)
    return;
  service_proxy_->OverwriteResult(static_cast<SegmentId>(segment_id), result);
}

void SegmentationInternalsPageHandlerImpl::SetSelected(
    const std::string& segmentation_key,
    int segment_id) {
  if (!service_proxy_)
    return;

  service_proxy_->SetSelectedSegment(segmentation_key,
                                     static_cast<SegmentId>(segment_id));
}

void SegmentationInternalsPageHandlerImpl::OnServiceStatusChanged(
    bool is_initialized,
    int status_flag) {
  page_->OnServiceStatusChanged(is_initialized, status_flag);
}

void SegmentationInternalsPageHandlerImpl::OnClientInfoAvailable(
    const std::vector<segmentation_platform::ServiceProxy::ClientInfo>&
        client_info) {
  std::vector<segmentation_internals::mojom::ClientInfoPtr> available_clients;
  for (const auto& info : client_info) {
    auto client = segmentation_internals::mojom::ClientInfo::New();
    client->segmentation_key = info.segmentation_key;
    if (info.selected_segment) {
      client->selected_segment =
          segmentation_platform::proto::SegmentId_Name(*info.selected_segment);
    } else {
      client->selected_segment = "Not Ready";
    }
    for (const auto& status : info.segment_status) {
      auto segment_data = segmentation_internals::mojom::SegmentInfo::New();
      segment_data->segment_name =
          segmentation_platform::proto::SegmentId_Name(status.segment_id);
      segment_data->segment_id = status.segment_id;
      segment_data->segment_data = status.segment_metadata;
      segment_data->prediction_result = status.prediction_result;
      segment_data->prediction_timestamp = status.prediction_timestamp;
      segment_data->can_execute_segment = status.can_execute_segment;
      client->segment_info.emplace_back(std::move(segment_data));
    }
    available_clients.emplace_back(std::move(client));
  }
  page_->OnClientInfoAvailable(std::move(available_clients));
}
