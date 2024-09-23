// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/software_feature_manager_impl.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_feature_status_setter.h"
#include "chromeos/ash/services/device_sync/feature_status_change.h"
#include "chromeos/ash/services/device_sync/mock_cryptauth_client.h"
#include "chromeos/ash/services/device_sync/proto/enum_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::device_sync {

namespace {

using ::testing::_;
using ::testing::Invoke;

enum class Result {
  kSuccess,
  kErrorSettingSoftwareFeature,
  kErrorSettingFeatureStatus,
  kErrorFindingEligible
};

// Arbitrarily choose different error types which match to Result types.
const NetworkRequestError kErrorSettingSoftwareFeatureNetworkRequestError =
    NetworkRequestError::kOffline;
const NetworkRequestError kErrorSettingFeatureStatusNetworkRequestError =
    NetworkRequestError::kBadRequest;
const NetworkRequestError kErrorFindingEligibleNetworkRequestError =
    NetworkRequestError::kEndpointNotFound;

const char kBetterTogetherHostCallbackBluetoothAddress[] =
    "BETTER_TOGETHER_HOST";
const char kBetterTogetherClientCallbackBluetoothAddress[] =
    "BETTER_TOGETHER_CLIENT";

const char kInstanceId0[] = "instanceId0";
const char kInstanceId1[] = "instanceId1";
const char kInstanceId2[] = "instanceId2";

std::vector<cryptauth::ExternalDeviceInfo>
CreateExternalDeviceInfosForRemoteDevices(
    const multidevice::RemoteDeviceRefList remote_devices) {
  std::vector<cryptauth::ExternalDeviceInfo> device_infos;
  for (const auto& remote_device : remote_devices) {
    // Add an cryptauth::ExternalDeviceInfo with the same public key as the
    // multidevice::RemoteDevice.
    cryptauth::ExternalDeviceInfo info;
    info.set_public_key(remote_device.public_key());
    device_infos.push_back(info);
  }
  return device_infos;
}

}  // namespace

class DeviceSyncSoftwareFeatureManagerImplTest
    : public testing::Test,
      public MockCryptAuthClientFactory::Observer,
      public FakeCryptAuthFeatureStatusSetter::Delegate {
 public:
  DeviceSyncSoftwareFeatureManagerImplTest()
      : all_test_external_device_infos_(
            CreateExternalDeviceInfosForRemoteDevices(
                multidevice::CreateRemoteDeviceRefListForTest(5))),
        test_eligible_external_devices_infos_(
            {all_test_external_device_infos_[0],
             all_test_external_device_infos_[1],
             all_test_external_device_infos_[2]}),
        test_ineligible_external_devices_infos_(
            {all_test_external_device_infos_[3],
             all_test_external_device_infos_[4]}) {}

  DeviceSyncSoftwareFeatureManagerImplTest(
      const DeviceSyncSoftwareFeatureManagerImplTest&) = delete;
  DeviceSyncSoftwareFeatureManagerImplTest& operator=(
      const DeviceSyncSoftwareFeatureManagerImplTest&) = delete;

  void SetUp() override {
    mock_cryptauth_client_factory_ =
        std::make_unique<MockCryptAuthClientFactory>(
            MockCryptAuthClientFactory::MockType::MAKE_NICE_MOCKS);
    mock_cryptauth_client_factory_->AddObserver(this);

    fake_cryptauth_feature_status_setter_ =
        std::make_unique<FakeCryptAuthFeatureStatusSetter>();
    fake_cryptauth_feature_status_setter_->set_delegate(this);

    software_feature_manager_ = SoftwareFeatureManagerImpl::Factory::Create(
        mock_cryptauth_client_factory_.get(),
        fake_cryptauth_feature_status_setter_.get());
  }

  void TearDown() override {
    mock_cryptauth_client_factory_->RemoveObserver(this);
    fake_cryptauth_feature_status_setter_->set_delegate(nullptr);
  }

  void OnCryptAuthClientCreated(MockCryptAuthClient* client) override {
    ON_CALL(*client, ToggleEasyUnlock(_, _, _))
        .WillByDefault(Invoke(
            this,
            &DeviceSyncSoftwareFeatureManagerImplTest::MockToggleEasyUnlock));
    ON_CALL(*client, FindEligibleUnlockDevices(_, _, _))
        .WillByDefault(Invoke(this, &DeviceSyncSoftwareFeatureManagerImplTest::
                                        MockFindEligibleUnlockDevices));
  }

  // Mock CryptAuthClient::ToggleEasyUnlock() implementation.
  void MockToggleEasyUnlock(const cryptauth::ToggleEasyUnlockRequest& request,
                            CryptAuthClient::ToggleEasyUnlockCallback callback,
                            CryptAuthClient::ErrorCallback error_callback) {
    last_toggle_request_ = request;
    toggle_easy_unlock_callback_ = std::move(callback);
    error_callback_ = std::move(error_callback);
    error_code_ = kErrorSettingSoftwareFeatureNetworkRequestError;
  }

  // Mock CryptAuthClient::FindEligibleUnlockDevices() implementation.
  void MockFindEligibleUnlockDevices(
      const cryptauth::FindEligibleUnlockDevicesRequest& request,
      CryptAuthClient::FindEligibleUnlockDevicesCallback callback,
      CryptAuthClient::ErrorCallback error_callback) {
    last_find_request_ = request;
    find_eligible_unlock_devices_callback_ = std::move(callback);
    error_callback_ = std::move(error_callback);
    error_code_ = kErrorFindingEligibleNetworkRequestError;
  }

  // FakeCryptAuthFeatureStatusSetter::Delegate:
  void OnSetFeatureStatusCalled() override {
    error_code_ = kErrorSettingFeatureStatusNetworkRequestError;
  }

  cryptauth::FindEligibleUnlockDevicesResponse
  CreateFindEligibleUnlockDevicesResponse() {
    cryptauth::FindEligibleUnlockDevicesResponse
        find_eligible_unlock_devices_response;
    for (const auto& device_info : test_eligible_external_devices_infos_) {
      find_eligible_unlock_devices_response.add_eligible_devices()->CopyFrom(
          device_info);
    }
    for (const auto& device_info : test_ineligible_external_devices_infos_) {
      find_eligible_unlock_devices_response.add_ineligible_devices()
          ->mutable_device()
          ->CopyFrom(device_info);
    }
    return find_eligible_unlock_devices_response;
  }

  void VerifyDeviceEligibility() {
    // Ensure that resulting devices are not empty.  Otherwise, following for
    // loop checks will succeed on empty resulting devices.
    EXPECT_TRUE(result_eligible_devices_.size() > 0);
    EXPECT_TRUE(result_ineligible_devices_.size() > 0);
    for (const auto& device_info : result_eligible_devices_) {
      EXPECT_TRUE(base::Contains(test_eligible_external_devices_infos_,
                                 device_info.public_key(),
                                 &cryptauth::ExternalDeviceInfo::public_key));
    }
    for (const auto& ineligible_device : result_ineligible_devices_) {
      EXPECT_TRUE(base::Contains(test_ineligible_external_devices_infos_,
                                 ineligible_device.device().public_key(),
                                 &cryptauth::ExternalDeviceInfo::public_key));
    }
    result_eligible_devices_.clear();
    result_ineligible_devices_.clear();
  }

  void SetSoftwareFeatureState(multidevice::SoftwareFeature feature,
                               const cryptauth::ExternalDeviceInfo& device_info,
                               bool enabled,
                               bool is_exclusive = false) {
    software_feature_manager_->SetSoftwareFeatureState(
        device_info.public_key(), feature, enabled,
        base::BindOnce(&DeviceSyncSoftwareFeatureManagerImplTest::
                           OnSoftwareFeatureStateSet,
                       base::Unretained(this)),
        base::BindOnce(&DeviceSyncSoftwareFeatureManagerImplTest::OnError,
                       base::Unretained(this)),
        is_exclusive);
  }

  void SetFeatureStatus(const std::string& device_id,
                        multidevice::SoftwareFeature feature,
                        FeatureStatusChange status_change) {
    software_feature_manager_->SetFeatureStatus(
        device_id, feature, status_change,
        base::BindOnce(
            &DeviceSyncSoftwareFeatureManagerImplTest::OnFeatureStatusSet,
            base::Unretained(this)),
        base::BindOnce(&DeviceSyncSoftwareFeatureManagerImplTest::OnError,
                       base::Unretained(this)));
  }

  void FindEligibleDevices(multidevice::SoftwareFeature feature) {
    software_feature_manager_->FindEligibleDevices(
        feature,
        base::BindOnce(
            &DeviceSyncSoftwareFeatureManagerImplTest::OnEligibleDevicesFound,
            base::Unretained(this)),
        base::BindOnce(&DeviceSyncSoftwareFeatureManagerImplTest::OnError,
                       base::Unretained(this)));
  }

  void VerifyLastSetFeatureStatusRequest(
      const std::string& expected_device_id,
      multidevice::SoftwareFeature expected_feature,
      FeatureStatusChange expected_status_change) {
    ASSERT_EQ(1u, fake_cryptauth_feature_status_setter_->requests().size());
    EXPECT_EQ(
        expected_device_id,
        fake_cryptauth_feature_status_setter_->requests().back().device_id);
    EXPECT_EQ(expected_feature,
              fake_cryptauth_feature_status_setter_->requests().back().feature);
    EXPECT_EQ(
        expected_status_change,
        fake_cryptauth_feature_status_setter_->requests().back().status_change);
  }

  void OnSoftwareFeatureStateSet() { result_ = Result::kSuccess; }

  void OnFeatureStatusSet() { result_ = Result::kSuccess; }

  void OnEligibleDevicesFound(
      const std::vector<cryptauth::ExternalDeviceInfo>& eligible_devices,
      const std::vector<cryptauth::IneligibleDevice>& ineligible_devices) {
    result_ = Result::kSuccess;
    result_eligible_devices_ = eligible_devices;
    result_ineligible_devices_ = ineligible_devices;
  }

  void OnError(NetworkRequestError error) {
    if (error == kErrorSettingSoftwareFeatureNetworkRequestError)
      result_ = Result::kErrorSettingSoftwareFeature;
    else if (error == kErrorSettingFeatureStatusNetworkRequestError)
      result_ = Result::kErrorSettingFeatureStatus;
    else if (error == kErrorFindingEligibleNetworkRequestError)
      result_ = Result::kErrorFindingEligible;
    else
      NOTREACHED_IN_MIGRATION();
  }

  void InvokeSetSoftwareFeatureCallback() {
    CryptAuthClient::ToggleEasyUnlockCallback success_callback =
        std::move(toggle_easy_unlock_callback_);
    ASSERT_TRUE(!success_callback.is_null());
    std::move(success_callback).Run(cryptauth::ToggleEasyUnlockResponse());
  }

  void InvokeSetFeatureStatusCallback() {
    ASSERT_EQ(1u, fake_cryptauth_feature_status_setter_->requests().size());

    base::OnceClosure success_callback =
        std::move(fake_cryptauth_feature_status_setter_->requests()
                      .back()
                      .success_callback);
    ASSERT_FALSE(success_callback.is_null());
    fake_cryptauth_feature_status_setter_->requests().pop_back();

    std::move(success_callback).Run();
  }

  void InvokeFindEligibleDevicesCallback(
      const cryptauth::FindEligibleUnlockDevicesResponse&
          retrieved_devices_response) {
    CryptAuthClient::FindEligibleUnlockDevicesCallback success_callback =
        std::move(find_eligible_unlock_devices_callback_);
    ASSERT_TRUE(!success_callback.is_null());
    std::move(success_callback).Run(retrieved_devices_response);
  }

  void InvokeErrorCallback() {
    CryptAuthClient::ErrorCallback error_callback = std::move(error_callback_);
    ASSERT_TRUE(!error_callback.is_null());
    std::move(error_callback).Run(*error_code_);
  }

  void InvokeSetFeatureStatusErrorCallback() {
    ASSERT_EQ(1u, fake_cryptauth_feature_status_setter_->requests().size());

    base::OnceCallback<void(NetworkRequestError)> error_callback =
        std::move(fake_cryptauth_feature_status_setter_->requests()
                      .back()
                      .error_callback);
    ASSERT_FALSE(error_callback.is_null());
    fake_cryptauth_feature_status_setter_->requests().pop_back();

    std::move(error_callback).Run(*error_code_);
  }

  Result GetResultAndReset() {
    EXPECT_TRUE(result_);
    Result result = *result_;
    result_.reset();
    return result;
  }

  const std::vector<cryptauth::ExternalDeviceInfo>
      all_test_external_device_infos_;
  const std::vector<cryptauth::ExternalDeviceInfo>
      test_eligible_external_devices_infos_;
  const std::vector<cryptauth::ExternalDeviceInfo>
      test_ineligible_external_devices_infos_;

  std::unique_ptr<FakeCryptAuthFeatureStatusSetter>
      fake_cryptauth_feature_status_setter_;
  std::unique_ptr<MockCryptAuthClientFactory> mock_cryptauth_client_factory_;
  std::unique_ptr<SoftwareFeatureManager> software_feature_manager_;

  CryptAuthClient::ErrorCallback error_callback_;

  // Set when a CryptAuthClient function returns. If empty, no callback has been
  // invoked.
  std::optional<Result> result_;

  // The code passed to the error callback; varies depending on what
  // CryptAuthClient function is invoked.
  std::optional<NetworkRequestError> error_code_;

  // For SetSoftwareFeatureState() tests.
  cryptauth::ToggleEasyUnlockRequest last_toggle_request_;
  CryptAuthClient::ToggleEasyUnlockCallback toggle_easy_unlock_callback_;

  // For FindEligibleDevices() tests.
  cryptauth::FindEligibleUnlockDevicesRequest last_find_request_;
  CryptAuthClient::FindEligibleUnlockDevicesCallback
      find_eligible_unlock_devices_callback_;
  std::vector<cryptauth::ExternalDeviceInfo> result_eligible_devices_;
  std::vector<cryptauth::IneligibleDevice> result_ineligible_devices_;
};

TEST_F(DeviceSyncSoftwareFeatureManagerImplTest,
       TestOrderUponMultipleRequests) {
  SetSoftwareFeatureState(multidevice::SoftwareFeature::kBetterTogetherHost,
                          test_eligible_external_devices_infos_[0],
                          true /* enable */);
  SetFeatureStatus(kInstanceId0,
                   multidevice::SoftwareFeature::kBetterTogetherHost,
                   FeatureStatusChange::kEnableExclusively);
  FindEligibleDevices(multidevice::SoftwareFeature::kBetterTogetherHost);
  SetSoftwareFeatureState(multidevice::SoftwareFeature::kBetterTogetherClient,
                          test_eligible_external_devices_infos_[1],
                          false /* enable */);
  SetFeatureStatus(kInstanceId1,
                   multidevice::SoftwareFeature::kBetterTogetherClient,
                   FeatureStatusChange::kEnableNonExclusively);
  FindEligibleDevices(multidevice::SoftwareFeature::kBetterTogetherClient);

  EXPECT_EQ(SoftwareFeatureEnumToString(
                cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST),
            last_toggle_request_.feature());
  EXPECT_EQ(true, last_toggle_request_.enable());
  EXPECT_EQ(false, last_toggle_request_.is_exclusive());
  InvokeSetSoftwareFeatureCallback();
  EXPECT_EQ(Result::kSuccess, GetResultAndReset());

  VerifyLastSetFeatureStatusRequest(
      kInstanceId0, multidevice::SoftwareFeature::kBetterTogetherHost,
      FeatureStatusChange::kEnableExclusively);
  InvokeSetFeatureStatusCallback();
  EXPECT_EQ(Result::kSuccess, GetResultAndReset());

  EXPECT_EQ(SoftwareFeatureEnumToString(
                cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST),
            last_find_request_.feature());
  EXPECT_EQ(kBetterTogetherHostCallbackBluetoothAddress,
            last_find_request_.callback_bluetooth_address());
  InvokeFindEligibleDevicesCallback(CreateFindEligibleUnlockDevicesResponse());
  EXPECT_EQ(Result::kSuccess, GetResultAndReset());
  VerifyDeviceEligibility();

  EXPECT_EQ(SoftwareFeatureEnumToString(
                cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT),
            last_toggle_request_.feature());
  EXPECT_EQ(false, last_toggle_request_.enable());
  EXPECT_EQ(false, last_toggle_request_.is_exclusive());
  InvokeSetSoftwareFeatureCallback();
  EXPECT_EQ(Result::kSuccess, GetResultAndReset());

  VerifyLastSetFeatureStatusRequest(
      kInstanceId1, multidevice::SoftwareFeature::kBetterTogetherClient,
      FeatureStatusChange::kEnableNonExclusively);
  InvokeSetFeatureStatusCallback();
  EXPECT_EQ(Result::kSuccess, GetResultAndReset());

  EXPECT_EQ(SoftwareFeatureEnumToString(
                cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT),
            last_find_request_.feature());
  EXPECT_EQ(kBetterTogetherClientCallbackBluetoothAddress,
            last_find_request_.callback_bluetooth_address());
  InvokeFindEligibleDevicesCallback(CreateFindEligibleUnlockDevicesResponse());
  EXPECT_EQ(Result::kSuccess, GetResultAndReset());
  VerifyDeviceEligibility();
}

TEST_F(DeviceSyncSoftwareFeatureManagerImplTest,
       TestMultipleSetUnlocksRequests) {
  SetSoftwareFeatureState(multidevice::SoftwareFeature::kBetterTogetherHost,
                          test_eligible_external_devices_infos_[0],
                          true /* enable */);
  SetSoftwareFeatureState(multidevice::SoftwareFeature::kBetterTogetherClient,
                          test_eligible_external_devices_infos_[1],
                          false /* enable */);
  SetSoftwareFeatureState(multidevice::SoftwareFeature::kBetterTogetherHost,
                          test_eligible_external_devices_infos_[2],
                          true /* enable */);

  EXPECT_EQ(SoftwareFeatureEnumToString(
                cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST),
            last_toggle_request_.feature());
  EXPECT_EQ(true, last_toggle_request_.enable());
  EXPECT_EQ(false, last_toggle_request_.is_exclusive());
  InvokeErrorCallback();
  EXPECT_EQ(Result::kErrorSettingSoftwareFeature, GetResultAndReset());

  EXPECT_EQ(SoftwareFeatureEnumToString(
                cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT),
            last_toggle_request_.feature());
  EXPECT_EQ(false, last_toggle_request_.enable());
  EXPECT_EQ(false, last_toggle_request_.is_exclusive());
  InvokeSetSoftwareFeatureCallback();
  EXPECT_EQ(Result::kSuccess, GetResultAndReset());

  EXPECT_EQ(SoftwareFeatureEnumToString(
                cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST),
            last_toggle_request_.feature());
  EXPECT_EQ(true, last_toggle_request_.enable());
  EXPECT_EQ(false, last_toggle_request_.is_exclusive());
  InvokeSetSoftwareFeatureCallback();
  EXPECT_EQ(Result::kSuccess, GetResultAndReset());
}

TEST_F(DeviceSyncSoftwareFeatureManagerImplTest,
       TestMultipleSetFeatureStatusRequests) {
  SetFeatureStatus(kInstanceId0,
                   multidevice::SoftwareFeature::kBetterTogetherHost,
                   FeatureStatusChange::kEnableExclusively);
  SetFeatureStatus(kInstanceId1,
                   multidevice::SoftwareFeature::kBetterTogetherClient,
                   FeatureStatusChange::kEnableNonExclusively);
  SetFeatureStatus(kInstanceId2,
                   multidevice::SoftwareFeature::kBetterTogetherHost,
                   FeatureStatusChange::kEnableNonExclusively);

  VerifyLastSetFeatureStatusRequest(
      kInstanceId0, multidevice::SoftwareFeature::kBetterTogetherHost,
      FeatureStatusChange::kEnableExclusively);
  InvokeSetFeatureStatusErrorCallback();
  EXPECT_EQ(Result::kErrorSettingFeatureStatus, GetResultAndReset());

  VerifyLastSetFeatureStatusRequest(
      kInstanceId1, multidevice::SoftwareFeature::kBetterTogetherClient,
      FeatureStatusChange::kEnableNonExclusively);
  InvokeSetFeatureStatusCallback();
  EXPECT_EQ(Result::kSuccess, GetResultAndReset());

  VerifyLastSetFeatureStatusRequest(
      kInstanceId2, multidevice::SoftwareFeature::kBetterTogetherHost,
      FeatureStatusChange::kEnableNonExclusively);
  InvokeSetFeatureStatusCallback();
  EXPECT_EQ(Result::kSuccess, GetResultAndReset());
}

TEST_F(DeviceSyncSoftwareFeatureManagerImplTest,
       TestMultipleFindEligibleForUnlockDevicesRequests) {
  FindEligibleDevices(multidevice::SoftwareFeature::kBetterTogetherHost);
  FindEligibleDevices(multidevice::SoftwareFeature::kBetterTogetherClient);
  FindEligibleDevices(multidevice::SoftwareFeature::kBetterTogetherHost);

  EXPECT_EQ(SoftwareFeatureEnumToString(
                cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST),
            last_find_request_.feature());
  EXPECT_EQ(kBetterTogetherHostCallbackBluetoothAddress,
            last_find_request_.callback_bluetooth_address());
  InvokeFindEligibleDevicesCallback(CreateFindEligibleUnlockDevicesResponse());
  EXPECT_EQ(Result::kSuccess, GetResultAndReset());
  VerifyDeviceEligibility();

  EXPECT_EQ(SoftwareFeatureEnumToString(
                cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT),
            last_find_request_.feature());
  EXPECT_EQ(kBetterTogetherClientCallbackBluetoothAddress,
            last_find_request_.callback_bluetooth_address());
  InvokeErrorCallback();
  EXPECT_EQ(Result::kErrorFindingEligible, GetResultAndReset());

  EXPECT_EQ(SoftwareFeatureEnumToString(
                cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST),
            last_find_request_.feature());
  EXPECT_EQ(kBetterTogetherHostCallbackBluetoothAddress,
            last_find_request_.callback_bluetooth_address());
  InvokeFindEligibleDevicesCallback(CreateFindEligibleUnlockDevicesResponse());
  EXPECT_EQ(Result::kSuccess, GetResultAndReset());
  VerifyDeviceEligibility();
}

TEST_F(DeviceSyncSoftwareFeatureManagerImplTest, TestOrderViaMultipleErrors) {
  SetSoftwareFeatureState(multidevice::SoftwareFeature::kBetterTogetherHost,
                          test_eligible_external_devices_infos_[0],
                          true /* enable */);
  SetFeatureStatus(kInstanceId0,
                   multidevice::SoftwareFeature::kBetterTogetherHost,
                   FeatureStatusChange::kEnableExclusively);
  FindEligibleDevices(multidevice::SoftwareFeature::kBetterTogetherHost);

  EXPECT_EQ(SoftwareFeatureEnumToString(
                cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST),
            last_toggle_request_.feature());
  InvokeErrorCallback();
  EXPECT_EQ(Result::kErrorSettingSoftwareFeature, GetResultAndReset());

  VerifyLastSetFeatureStatusRequest(
      kInstanceId0, multidevice::SoftwareFeature::kBetterTogetherHost,
      FeatureStatusChange::kEnableExclusively);
  InvokeSetFeatureStatusErrorCallback();
  EXPECT_EQ(Result::kErrorSettingFeatureStatus, GetResultAndReset());

  EXPECT_EQ(SoftwareFeatureEnumToString(
                cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST),
            last_find_request_.feature());
  EXPECT_EQ(kBetterTogetherHostCallbackBluetoothAddress,
            last_find_request_.callback_bluetooth_address());
  InvokeErrorCallback();
  EXPECT_EQ(Result::kErrorFindingEligible, GetResultAndReset());
}

TEST_F(DeviceSyncSoftwareFeatureManagerImplTest, TestIsExclusive) {
  SetSoftwareFeatureState(multidevice::SoftwareFeature::kBetterTogetherHost,
                          test_eligible_external_devices_infos_[0],
                          true /* enable */, true /* is_exclusive */);

  EXPECT_EQ(SoftwareFeatureEnumToString(
                cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST),
            last_toggle_request_.feature());
  EXPECT_EQ(true, last_toggle_request_.enable());
  EXPECT_EQ(true, last_toggle_request_.is_exclusive());
  InvokeErrorCallback();
  EXPECT_EQ(Result::kErrorSettingSoftwareFeature, GetResultAndReset());
}

TEST_F(DeviceSyncSoftwareFeatureManagerImplTest, TestEasyUnlockSpecialCase) {
  SetSoftwareFeatureState(multidevice::SoftwareFeature::kSmartLockHost,
                          test_eligible_external_devices_infos_[0],
                          false /* enable */);

  EXPECT_EQ(
      SoftwareFeatureEnumToString(cryptauth::SoftwareFeature::EASY_UNLOCK_HOST),
      last_toggle_request_.feature());
  EXPECT_EQ(false, last_toggle_request_.enable());
  // apply_to_all() should be false when disabling EasyUnlock host capabilities.
  EXPECT_EQ(true, last_toggle_request_.apply_to_all());
  EXPECT_FALSE(last_toggle_request_.has_public_key());
  InvokeErrorCallback();
  EXPECT_EQ(Result::kErrorSettingSoftwareFeature, GetResultAndReset());
}

}  // namespace ash::device_sync
