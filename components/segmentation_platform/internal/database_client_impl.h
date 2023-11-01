// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_CLIENT_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_CLIENT_IMPL_H_

#include "components/segmentation_platform/internal/scheduler/execution_service.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"
#include "components/segmentation_platform/public/database_client.h"

namespace segmentation_platform {

class DatabaseClientImpl : public DatabaseClient {
 public:
  DatabaseClientImpl(ExecutionService* execution_service,
                     UkmDataManager* data_manager);
  ~DatabaseClientImpl() override;

  DatabaseClientImpl(const DatabaseClientImpl&) = delete;
  DatabaseClientImpl& operator=(const DatabaseClientImpl&) = delete;

  // DatabaseClient impl:
  void ProcessFeatures(const proto::SegmentationModelMetadata& metadata,
                       base::Time end_time,
                       FeaturesCallback callback) override;
  void AddEvent(const StructuredEvent& event) override;

 private:
  const raw_ptr<ExecutionService> execution_service_;
  const raw_ptr<UkmDataManager> data_manager_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_CLIENT_IMPL_H_
