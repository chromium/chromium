// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_SERVICE_TEST_BASE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_SERVICE_TEST_BASE_H_

#include <memory>
#include <vector>

#include "base/test/simple_test_clock.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/execution/mock_model_provider.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/proto/signal.pb.h"
#include "components/segmentation_platform/internal/proto/signal_storage_config.pb.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace history {
class HistoryService;
}

namespace segmentation_platform {

struct Config;
class SegmentationPlatformServiceImpl;
class UkmDataManager;

extern const char kTestSegmentationKey1[];
extern const char kTestSegmentationKey2[];
extern const char kTestSegmentationKey3[];
extern const char kTestSegmentationKey4[];
extern const char kTestProfileId[];

// Wrapper around SegmentationPlatformServiceImpl for testing. Holds and manages
// a single platform instance.
class SegmentationPlatformServiceTestBase {
 public:
  SegmentationPlatformServiceTestBase();
  virtual ~SegmentationPlatformServiceTestBase();

  // Creates the platform service, does not wait for initialization to complete.
  void InitPlatform(UkmDataManager* ukm_data_manager,
                    history::HistoryService* history_service);

  // Destroys the platform, and setup.
  void DestroyPlatform();

  // Called to register additional segmentation prefs before creating the
  // platform.
  virtual void SetUpPrefs();
  // Called to create a config before creating the platform. Uses a default
  // config with 3 keys: kTestSegmentationKey* with different selection TTLs.
  virtual std::vector<std::unique_ptr<Config>> CreateConfigs();

  leveldb_proto::test::FakeDB<proto::SegmentInfo>& segment_db() {
    return *segment_db_;
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::map<std::string, proto::SegmentInfo> segment_db_entries_;
  std::map<std::string, proto::SignalData> signal_db_entries_;
  std::map<std::string, proto::SignalStorageConfigs>
      segment_storage_config_db_entries_;
  raw_ptr<leveldb_proto::test::FakeDB<proto::SegmentInfo>, DanglingUntriaged>
      segment_db_;
  raw_ptr<leveldb_proto::test::FakeDB<proto::SignalData>, DanglingUntriaged>
      signal_db_;
  raw_ptr<leveldb_proto::test::FakeDB<proto::SignalStorageConfigs>,
          DanglingUntriaged>
      segment_storage_config_db_;
  TestModelProviderFactory::Data model_provider_data_;
  TestingPrefServiceSimple pref_service_;
  base::SimpleTestClock test_clock_;
  std::unique_ptr<syncer::DeviceInfoTracker> device_info_tracker_;
  std::unique_ptr<SegmentationPlatformServiceImpl>
      segmentation_platform_service_impl_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_PLATFORM_SERVICE_TEST_BASE_H_
