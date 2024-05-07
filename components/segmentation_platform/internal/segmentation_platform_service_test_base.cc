// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/segmentation_platform_service_test_base.h"

#include <string_view>

#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/execution/mock_model_provider.h"
#include "components/segmentation_platform/internal/segmentation_platform_service_impl.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/field_trial_register.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/fake_device_info_tracker.h"

namespace segmentation_platform {

namespace {

using syncer::DeviceInfoTracker;

class MockFieldTrialRegister : public FieldTrialRegister {
 public:
  MOCK_METHOD2(RegisterFieldTrial,
               void(std::string_view trial_name, std::string_view group_name));
  MOCK_METHOD3(RegisterSubsegmentFieldTrialIfNeeded,
               void(std::string_view trial_name,
                    proto::SegmentId segment_id,
                    int subsegment_rank));
};

std::vector<std::unique_ptr<Config>> CreateTestConfigs() {
  std::vector<std::unique_ptr<Config>> configs;
  {
    std::unique_ptr<Config> config = std::make_unique<Config>();
    config->segmentation_key = kTestSegmentationKey1;
    config->segment_selection_ttl = base::Days(28);
    config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
    config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
    configs.push_back(std::move(config));
  }
  {
    std::unique_ptr<Config> config = std::make_unique<Config>();
    config->segmentation_key = kTestSegmentationKey2;
    config->segment_selection_ttl = base::Days(10);
    config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
    config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE);
    configs.push_back(std::move(config));
  }
  {
    std::unique_ptr<Config> config = std::make_unique<Config>();
    config->segmentation_key = kTestSegmentationKey3;
    config->segment_selection_ttl = base::Days(14);
    config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
    configs.push_back(std::move(config));
  }
  {
    std::unique_ptr<Config> config = std::make_unique<Config>();
    config->segmentation_key = kTestSegmentationKey4;
    config->segment_selection_ttl = base::Days(14);
    config->AddSegmentId(
        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER);
    config->auto_execute_and_cache = false;
    configs.push_back(std::move(config));
  }
  {
    // Empty config.
    std::unique_ptr<Config> config = std::make_unique<Config>();
    config->segmentation_key = "test_key";
    configs.push_back(std::move(config));
  }

  return configs;
}

}  // namespace

constexpr char kTestSegmentationKey1[] = "test_key1";
constexpr char kTestSegmentationKey2[] = "test_key2";
constexpr char kTestSegmentationKey3[] = "test_key3";
constexpr char kTestSegmentationKey4[] = "test_key4";
constexpr char kTestProfileId[] = "test_id";

SegmentationPlatformServiceTestBase::SegmentationPlatformServiceTestBase() {
  device_info_tracker_ = std::make_unique<syncer::FakeDeviceInfoTracker>();
}

SegmentationPlatformServiceTestBase::~SegmentationPlatformServiceTestBase() =
    default;

void SegmentationPlatformServiceTestBase::InitPlatform(
    UkmDataManager* ukm_data_manager,
    history::HistoryService* history_service) {
  task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();

  auto segment_db =
      std::make_unique<leveldb_proto::test::FakeDB<proto::SegmentInfo>>(
          &segment_db_entries_);
  auto signal_db =
      std::make_unique<leveldb_proto::test::FakeDB<proto::SignalData>>(
          &signal_db_entries_);
  auto segment_storage_config_db = std::make_unique<
      leveldb_proto::test::FakeDB<proto::SignalStorageConfigs>>(
      &segment_storage_config_db_entries_);
  segment_db_ = segment_db.get();
  signal_db_ = signal_db.get();
  segment_storage_config_db_ = segment_storage_config_db.get();
  auto model_provider_factory =
      std::make_unique<TestModelProviderFactory>(&model_provider_data_);

  SegmentationPlatformService::RegisterProfilePrefs(pref_service_.registry());
  SetUpPrefs();

  std::vector<std::unique_ptr<Config>> configs = CreateTestConfigs();
  base::flat_set<SegmentId> all_segment_ids;
  for (const auto& config : configs) {
    for (const auto& segment_id : config->segments)
      all_segment_ids.insert(segment_id.first);
  }
  auto storage_service = std::make_unique<StorageService>(
      std::move(segment_db), std::move(signal_db),
      std::move(segment_storage_config_db), task_runner_, &test_clock_,
      ukm_data_manager, std::move(configs), model_provider_factory.get(),
      &pref_service_, "profile_id", base::DoNothing());

  auto params = std::make_unique<SegmentationPlatformServiceImpl::InitParams>();
  params->profile_id = kTestProfileId;
  params->storage_service = std::move(storage_service);
  params->model_provider = std::move(model_provider_factory);
  params->profile_prefs = &pref_service_;
  params->history_service = history_service;
  params->task_runner = task_runner_;
  params->clock = &test_clock_;
  params->field_trial_register = std::make_unique<MockFieldTrialRegister>();
  params->device_info_tracker = device_info_tracker_.get();
  params->input_delegate_holder =
      std::make_unique<processing::InputDelegateHolder>();
  segmentation_platform_service_impl_ =
      std::make_unique<SegmentationPlatformServiceImpl>(std::move(params));
}

void SegmentationPlatformServiceTestBase::DestroyPlatform() {
  segmentation_platform_service_impl_.reset();
  // Allow for the SegmentationModelExecutor owned by SegmentationModelHandler
  // to be destroyed.
  task_runner_->RunUntilIdle();
}

void SegmentationPlatformServiceTestBase::SetUpPrefs() {
  ScopedDictPrefUpdate update(&pref_service_, kSegmentationResultPref);
  base::Value::Dict& dictionary = update.Get();

  base::Value::Dict segmentation_result;
  segmentation_result.Set("segment_id",
                          SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
  dictionary.Set(kTestSegmentationKey1, std::move(segmentation_result));
}

std::vector<std::unique_ptr<Config>>
SegmentationPlatformServiceTestBase::CreateConfigs() {
  return CreateTestConfigs();
}

}  // namespace segmentation_platform
