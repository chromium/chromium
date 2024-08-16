// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/processing/sync_device_info_observer.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_util.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::processing {

namespace {

#define AS_FLOAT_VAL(x) ProcessedValue(static_cast<float>(x))

using syncer::DeviceInfo;
using OsType = syncer::DeviceInfo::OsType;
using DeviceCountByOsTypeMap = std::map<OsType, int>;
using syncer::FakeDeviceInfoTracker;
using DeviceType = sync_pb::SyncEnums::DeviceType;
using DeviceCountByOsTypeMap = std::map<DeviceInfo::OsType, int>;

const sync_pb::SyncEnums_DeviceType kLocalDeviceType =
    sync_pb::SyncEnums_DeviceType_TYPE_LINUX;
const DeviceInfo::OsType kLocalDeviceOS = DeviceInfo::OsType::kLinux;
const DeviceInfo::FormFactor kLocalDeviceFormFactor =
    DeviceInfo::FormFactor::kDesktop;

std::unique_ptr<DeviceInfo> CreateDeviceInfo(
    const std::string& guid,
    DeviceType device_type,
    OsType os_type,
    base::Time last_updated = base::Time::Now()) {
  return std::make_unique<DeviceInfo>(
      guid, "name", "chrome_version", "user_agent", device_type, os_type,
      kLocalDeviceFormFactor, "device_id", "manufacturer_name", "model_name",
      "full_hardware_class", last_updated,
      syncer::DeviceInfoUtil::GetPulseInterval(),
      /*send_tab_to_self_receiving_enabled=*/
      false,
      /*send_tab_to_self_receiving_type=*/
      sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
      std::nullopt,
      /*paask_info=*/std::nullopt,
      /*fcm_registration_token=*/std::string(),
      /*interested_data_types=*/syncer::DataTypeSet(),
      /*floating_workspace_last_signin_timestamp=*/std::nullopt);
}

scoped_refptr<InputContext> CreateInputContext() {
  auto input_context = base::MakeRefCounted<InputContext>();
  input_context->metadata_args.emplace("active_days_limit", 14);
  return input_context;
}

class SyncDeviceInfoObserverTest : public testing::Test {
 public:
  SyncDeviceInfoObserverTest() = default;
  ~SyncDeviceInfoObserverTest() override = default;

  void SetUp() override {
    device_info_tracker_ = std::make_unique<FakeDeviceInfoTracker>();
    sync_device_info_observer_ =
        std::make_unique<SyncDeviceInfoObserver>(device_info_tracker_.get());
  }

  void TearDown() override {
    sync_device_info_observer_.reset();
    device_info_tracker_.reset();
  }

  void OnProcessFinishedCallback(base::OnceClosure closure,
                                 bool expected_error,
                                 std::vector<float> expected_result,
                                 bool actual_error,
                                 Tensor actual_result) {
    EXPECT_EQ(expected_error, actual_error);
    for (int index = 0; index < 10; index++) {
      EXPECT_EQ(actual_result[index].type, ProcessedValue::Type::FLOAT);
      EXPECT_EQ(expected_result[index], actual_result[index].float_val);
    }
    std::move(closure).Run();
  }

 protected:
  std::unique_ptr<FakeDeviceInfoTracker> device_info_tracker_;
  std::unique_ptr<SyncDeviceInfoObserver> sync_device_info_observer_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SyncDeviceInfoObserverTest, OnDeviceInfoChange_LocalDevice) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<DeviceInfo> local_device_info =
      CreateDeviceInfo("local_device", kLocalDeviceType, kLocalDeviceOS);
  // Adding a device triggers OnDeviceInfoChange().
  device_info_tracker_->Add(local_device_info.get());

  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Linux", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Windows", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Mac", 0, 1);
}

TEST_F(SyncDeviceInfoObserverTest, OnDeviceInfoChange_DifferentGuids) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<DeviceInfo> local_device_info =
      CreateDeviceInfo("local_device", kLocalDeviceType, kLocalDeviceOS);
  std::unique_ptr<DeviceInfo> local_device_info_2 =
      CreateDeviceInfo("local_device_2", kLocalDeviceType, kLocalDeviceOS);
  device_info_tracker_->Add(
      {local_device_info.get(), local_device_info_2.get()});

  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Linux", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Windows", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Mac", 0, 1);
}

TEST_F(SyncDeviceInfoObserverTest, OnDeviceInfoChange_DifferentOS) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<DeviceInfo> local_device_info =
      CreateDeviceInfo("local_device", kLocalDeviceType, kLocalDeviceOS);
  std::unique_ptr<DeviceInfo> local_device_info_2 =
      CreateDeviceInfo("local_device_2", kLocalDeviceType, OsType::kMac);
  device_info_tracker_->Add(
      {local_device_info.get(), local_device_info_2.get()});
  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Linux", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Mac", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Windows", 0, 1);
}

TEST_F(SyncDeviceInfoObserverTest, OnDeviceInfoChange_InactiveDevice) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<DeviceInfo> local_device_info =
      CreateDeviceInfo("local_device", kLocalDeviceType, kLocalDeviceOS);
  std::unique_ptr<DeviceInfo> local_device_info_2 =
      CreateDeviceInfo("local_device_2", kLocalDeviceType, OsType::kMac,
                       base::Time::Now() - base::Days(20));
  device_info_tracker_->Add(
      {local_device_info.get(), local_device_info_2.get()});

  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Linux", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Mac", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Windows", 0, 1);
}

TEST_F(SyncDeviceInfoObserverTest, AddingDeviceBeforeProcess) {
  FeatureProcessorState state;
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);

  auto* sync_input = writer.AddCustomInput(MetadataWriter::CustomInput{
      .tensor_length = 10,
      .fill_policy = proto::CustomInput::FILL_SYNC_DEVICE_INFO,
      .name = "SyncDeviceInfo"});
  (*sync_input->mutable_additional_args())["wait_for_device_info_in_seconds"] =
      "2";
  state.set_input_context_for_testing(CreateInputContext());

  std::unique_ptr<DeviceInfo> local_device_info =
      CreateDeviceInfo("local_device", kLocalDeviceType, kLocalDeviceOS);
  // Adding a device triggers OnDeviceInfoChange().
  device_info_tracker_->Add(local_device_info.get());

  // Should not be included in the count.
  std::unique_ptr<DeviceInfo> current_device =
      CreateDeviceInfo("current", kLocalDeviceType, kLocalDeviceOS);
  device_info_tracker_->Add(current_device.get());
  device_info_tracker_->SetLocalCacheGuid("current");

  std::vector<float> expected_result = {0, 0, 0, 0, 0, 1, 0, 0, 0, 0};
  base::RunLoop loop;
  sync_device_info_observer_->Process(
      *sync_input, state,
      base::BindOnce(&SyncDeviceInfoObserverTest::OnProcessFinishedCallback,
                     base::Unretained(this), loop.QuitClosure(), false,
                     expected_result));
  loop.Run();
}

TEST_F(SyncDeviceInfoObserverTest,
       ProcessWithTimeoutAndAddingDeviceWithoutWait) {
  FeatureProcessorState state;
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);

  auto* sync_input = writer.AddCustomInput(MetadataWriter::CustomInput{
      .tensor_length = 10,
      .fill_policy = proto::CustomInput::FILL_SYNC_DEVICE_INFO,
      .name = "SyncDeviceInfo"});
  auto input_context = CreateInputContext();
  input_context->metadata_args.emplace("wait_for_device_info_in_seconds",
                                       ProcessedValue(2));
  state.set_input_context_for_testing(input_context);

  std::vector<float> expected_result = {0, 0, 0, 0, 0, 1, 0, 0, 0, 0};
  base::RunLoop loop;
  sync_device_info_observer_->Process(
      *sync_input, state,
      base::BindOnce(&SyncDeviceInfoObserverTest::OnProcessFinishedCallback,
                     base::Unretained(this), loop.QuitClosure(), false,
                     expected_result));
  std::unique_ptr<DeviceInfo> local_device_info =
      CreateDeviceInfo("local_device", kLocalDeviceType, kLocalDeviceOS);
  // Adding a device triggers OnDeviceInfoChange().
  device_info_tracker_->Add(local_device_info.get());
  loop.Run();
}

TEST_F(SyncDeviceInfoObserverTest, ProcessWithNoTimeout) {
  FeatureProcessorState state;
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);

  auto* sync_input = writer.AddCustomInput(MetadataWriter::CustomInput{
      .tensor_length = 10,
      .fill_policy = proto::CustomInput::FILL_SYNC_DEVICE_INFO,
      .name = "SyncDeviceInfo"});
  (*sync_input->mutable_additional_args())["wait_for_device_info_in_seconds"] =
      "0";
  state.set_input_context_for_testing(CreateInputContext());

  // Failure flag is set.
  std::vector<float> expected_result = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  base::RunLoop loop;
  sync_device_info_observer_->Process(
      *sync_input, state,
      base::BindOnce(&SyncDeviceInfoObserverTest::OnProcessFinishedCallback,
                     base::Unretained(this), loop.QuitClosure(), false,
                     expected_result));
  std::unique_ptr<DeviceInfo> local_device_info =
      CreateDeviceInfo("local_device", kLocalDeviceType, kLocalDeviceOS);
  // Adding a device triggers OnDeviceInfoChange().
  device_info_tracker_->Add(local_device_info.get());
  loop.Run();
}

TEST_F(SyncDeviceInfoObserverTest, ProcessWithNotIntegerTimeout) {
  FeatureProcessorState state;
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);

  auto* sync_input = writer.AddCustomInput(MetadataWriter::CustomInput{
      .tensor_length = 10,
      .fill_policy = proto::CustomInput::FILL_SYNC_DEVICE_INFO,
      .name = "SyncDeviceInfo"});
  (*sync_input->mutable_additional_args())["wait_for_device_info_in_seconds"] =
      "true";
  state.set_input_context_for_testing(CreateInputContext());

  // Failure flag is set.
  std::vector<float> expected_result = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  base::RunLoop loop;
  sync_device_info_observer_->Process(
      *sync_input, state,
      base::BindOnce(&SyncDeviceInfoObserverTest::OnProcessFinishedCallback,
                     base::Unretained(this), loop.QuitClosure(), false,
                     expected_result));

  std::unique_ptr<DeviceInfo> local_device_info =
      CreateDeviceInfo("local_device", kLocalDeviceType, kLocalDeviceOS);
  // Adding a device triggers OnDeviceInfoChange().
  device_info_tracker_->Add(local_device_info.get());
  loop.Run();
}

TEST_F(SyncDeviceInfoObserverTest, ProcessWithTimeoutBeforeAddingDevice) {
  FeatureProcessorState state;
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);

  auto* sync_input = writer.AddCustomInput(MetadataWriter::CustomInput{
      .tensor_length = 10,
      .fill_policy = proto::CustomInput::FILL_SYNC_DEVICE_INFO,
      .name = "SyncDeviceInfo"});
  (*sync_input->mutable_additional_args())["wait_for_device_info_in_seconds"] =
      "2";
  state.set_input_context_for_testing(CreateInputContext());

  // Failure flag is set.
  std::vector<float> expected_result = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  base::RunLoop loop;
  sync_device_info_observer_->Process(
      *sync_input, state,
      base::BindOnce(&SyncDeviceInfoObserverTest::OnProcessFinishedCallback,
                     base::Unretained(this), loop.QuitClosure(), false,
                     expected_result));
  loop.Run();
  std::unique_ptr<DeviceInfo> local_device_info =
      CreateDeviceInfo("local_device", kLocalDeviceType, kLocalDeviceOS);
  // Adding a device triggers OnDeviceInfoChange().
  device_info_tracker_->Add(local_device_info.get());
}

}  // namespace

}  // namespace segmentation_platform::processing
