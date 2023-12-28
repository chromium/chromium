// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_MOCK_UKM_DATA_MANAGER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_MOCK_UKM_DATA_MANAGER_H_

#include "base/files/file_path.h"
#include "components/segmentation_platform/internal/signals/ukm_config.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace segmentation_platform {

class MockUkmDataManager : public UkmDataManager {
 public:
  MockUkmDataManager();
  ~MockUkmDataManager() override;

  MOCK_METHOD(void,
              Initialize,
              (const base::FilePath& database_path, bool),
              (override));

  MOCK_METHOD(void, StartObservation, (UkmObserver*), (override));

  MOCK_METHOD(bool, IsUkmEngineEnabled, (), (override));

  MOCK_METHOD(void, StartObservingUkm, (const UkmConfig& config), (override));

  MOCK_METHOD(void, PauseOrResumeObservation, (bool pause), (override));

  MOCK_METHOD(UrlSignalHandler*, GetOrCreateUrlHandler, (), (override));

  MOCK_METHOD(UkmDatabase*, GetUkmDatabase, (), (override));

  MOCK_METHOD(bool, HasUkmDatabase, (), (override));

  MOCK_METHOD(void, OnEntryAdded, (ukm::mojom::UkmEntryPtr), (override));

  MOCK_METHOD(void,
              OnUkmSourceUpdated,
              (ukm::SourceId, const std::vector<GURL>&),
              (override));

  MOCK_METHOD(void, AddRef, (), (override));
  MOCK_METHOD(void, RemoveRef, (), (override));
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_MOCK_UKM_DATA_MANAGER_H_
