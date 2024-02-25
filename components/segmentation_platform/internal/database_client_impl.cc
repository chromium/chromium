// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database_client_impl.h"
#include "components/segmentation_platform/internal/execution/processing/feature_list_query_processor.h"
#include "components/segmentation_platform/internal/scheduler/execution_service.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"
#include "components/segmentation_platform/public/database_client.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"

namespace segmentation_platform {
namespace {

constexpr SegmentId kSegmentId = SegmentId::DATABASE_API_CLIENTS;

void FeatureCallbackWrapper(DatabaseClient::FeaturesCallback callback,
                            bool error,
                            const ModelProvider::Request& inputs,
                            const ModelProvider::Response&) {
  DatabaseClient::ResultStatus status =
      error ? DatabaseClient::ResultStatus::kError
            : DatabaseClient::ResultStatus::kSuccess;
  std::move(callback).Run(status, inputs);
}

}  // namespace

DatabaseClientImpl::DatabaseClientImpl(ExecutionService* execution_service,
                                       UkmDataManager* data_manager)
    : execution_service_(execution_service), data_manager_(data_manager) {}

DatabaseClientImpl::~DatabaseClientImpl() = default;

void DatabaseClientImpl::ProcessFeatures(
    const proto::SegmentationModelMetadata& metadata,
    base::Time end_time,
    FeaturesCallback callback) {
  execution_service_->feature_processor()->ProcessFeatureList(
      metadata, nullptr, kSegmentId, end_time, base::Time(),
      processing::FeatureListQueryProcessor::ProcessOption::kInputsOnly,
      base::BindOnce(&FeatureCallbackWrapper, std::move(callback)));
}

void DatabaseClientImpl::AddEvent(const StructuredEvent& event) {
  ukm::mojom::UkmEntryPtr entry = ukm::mojom::UkmEntry::New();
  entry->event_hash = event.event_id.value();
  for (const auto& it : event.metric_hash_to_value) {
    entry->metrics[it.first.value()] = it.second;
  }
  data_manager_->GetUkmDatabase()->StoreUkmEntry(std::move(entry));
}

}  // namespace segmentation_platform
