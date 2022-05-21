// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/segmentation_platform_service_test_base.h"

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

namespace segmentation_platform {

namespace {

class MockFieldTrialRegister : public FieldTrialRegister {
 public:
  MOCK_METHOD2(RegisterFieldTrial,
               void(base::StringPiece trial_name,
                    base::StringPiece group_name));
  MOCK_METHOD3(RegisterSubsegmentFieldTrialIfNeeded,
               void(base::StringPiece trial_name,
                    proto::SegmentId segment_id,
                    int subsegment_rank));
};

std::vector<std::unique_ptr<Config>> CreateTestConfigs() {
  std::vector<std::unique_ptr<Config>> configs;
  {
    std::unique_ptr<Config> config = std::make_unique<Config>();
    config->segmentation_key = kTestSegmentationKey1;
    config->segment_selection_ttl = base::Days(28);
    config->segment_ids = {SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
                           SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE};
    configs.push_back(std::move(config));
  }
  {
    std::unique_ptr<Config> config = std::make_unique<Config>();
    config->segmentation_key = kTestSegmentationKey2;
    config->segment_selection_ttl = base::Days(10);
    config->segment_ids = {SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
                           SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE};
    configs.push_back(std::move(config));
  }
  {
    std::unique_ptr<Config> config = std::make_unique<Config>();
    config->segmentation_key = kTestSegmentationKey3;
    config->segment_selection_ttl = base::Days(14);
    config->segment_ids = {SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB};
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

SegmentationPlatformServiceTestBase::SegmentationPlatformServiceTestBase() =
    default;
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
    for (const auto& segment_id : config->segment_ids)
      all_segment_ids.insert(segment_id);
  }
  auto storage_service = std::make_unique<StorageService>(
      std::move(segment_db), std::move(signal_db),
      std::move(segment_storage_config_db), &test_clock_, ukm_data_manager,
      all_segment_ids, model_provider_factory.get());

  auto params = std::make_unique<SegmentationPlatformServiceImpl::InitParams>();
  params->storage_service = std::move(storage_service);
  params->model_provider =
      std::make_unique<TestModelProviderFactory>(&model_provider_data_);
  params->profile_prefs = &pref_service_;
  params->history_service = history_service;
  params->task_runner = task_runner_;
  params->clock = &test_clock_;
  params->configs = std::move(configs);
  params->field_trial_register = std::make_unique<MockFieldTrialRegister>();
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
  DictionaryPrefUpdate update(&pref_service_, kSegmentationResultPref);
  base::Value* dictionary = update.Get();

  base::Value segmentation_result(base::Value::Type::DICTIONARY);
  segmentation_result.SetIntKey(
      "segment_id", SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
  dictionary->SetKey(kTestSegmentationKey1, std::move(segmentation_result));
}

std::vector<std::unique_ptr<Config>>
SegmentationPlatformServiceTestBase::CreateConfigs() {
  return CreateTestConfigs();
}

}  // namespace segmentation_platform
