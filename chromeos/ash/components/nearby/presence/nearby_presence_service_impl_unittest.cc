// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/nearby_presence_service_impl.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chromeos/ash/components/nearby/presence/credentials/fake_nearby_presence_credential_manager.h"
#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager_impl.h"
#include "chromeos/ash/components/nearby/presence/enums/nearby_presence_enums.h"
#include "chromeos/ash/components/nearby/presence/nearby_presence_connections_manager.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_nearby_presence.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_connections.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_process_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/push_notification/fake_push_notification_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::nearby::presence {

const char kDeviceName[] = "DeviceName";
const char kEndpointId[] = "00000001";
const char kStableDeviceId[] = "00000002";
const char kMalformedTypeId[] = "not_nearby_presence";
const char kMalformedClientId[] = "not_nearby";
const std::vector<uint8_t> kMacAddress = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
const std::vector<uint8_t> kDeviceId = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab,
                                        0xcd, 0xef, 0x01, 0x23, 0x45, 0x67,
                                        0x89, 0xab, 0xcd, 0xef};
const mojom::ActionType kAction1 = mojom::ActionType::kInstantTetheringAction;
const mojom::ActionType kAction2 = mojom::ActionType::kActiveUnlockAction;
const mojom::ActionType kAction3 = mojom::ActionType::kPhoneHubAction;

constexpr auto kLatencyDelta = base::Milliseconds(123u);

namespace {

class FakeScanDelegate : public NearbyPresenceService::ScanDelegate {
 public:
  FakeScanDelegate() = default;
  FakeScanDelegate(const FakeScanDelegate&) = delete;
  FakeScanDelegate& operator=(const FakeScanDelegate&) = delete;
  ~FakeScanDelegate() override = default;

  void OnPresenceDeviceFound(
      ::nearby::presence::PresenceDevice presence_device) override {
    found_called_ = true;

    EXPECT_EQ(kEndpointId, presence_device.GetEndpointId());
    EXPECT_EQ(::nearby::internal::DEVICE_TYPE_PHONE,
              presence_device.GetDeviceIdentityMetadata().device_type());
    EXPECT_EQ(kDeviceName,
              presence_device.GetDeviceIdentityMetadata().device_name());
    EXPECT_EQ(
        std::string(kMacAddress.begin(), kMacAddress.end()),
        presence_device.GetDeviceIdentityMetadata().bluetooth_mac_address());
    EXPECT_EQ(std::string(kDeviceId.begin(), kDeviceId.end()),
              presence_device.GetDeviceIdentityMetadata().device_id());

    std::move(next_scan_delegate_callback_).Run();
  }

  void OnPresenceDeviceChanged(
      ::nearby::presence::PresenceDevice presence_device) override {
    changed_called_ = true;

    EXPECT_EQ(kEndpointId, presence_device.GetEndpointId());
    EXPECT_EQ(::nearby::internal::DEVICE_TYPE_PHONE,
              presence_device.GetDeviceIdentityMetadata().device_type());
    EXPECT_EQ(kDeviceName,
              presence_device.GetDeviceIdentityMetadata().device_name());
    EXPECT_EQ(
        std::string(kMacAddress.begin(), kMacAddress.end()),
        presence_device.GetDeviceIdentityMetadata().bluetooth_mac_address());
    EXPECT_EQ(std::string(kDeviceId.begin(), kDeviceId.end()),
              presence_device.GetDeviceIdentityMetadata().device_id());

    std::move(next_scan_delegate_callback_).Run();
  }

  void OnPresenceDeviceLost(
      ::nearby::presence::PresenceDevice presence_device) override {
    lost_called_ = true;

    EXPECT_EQ(kEndpointId, presence_device.GetEndpointId());
    EXPECT_EQ(::nearby::internal::DEVICE_TYPE_PHONE,
              presence_device.GetDeviceIdentityMetadata().device_type());
    EXPECT_EQ(kDeviceName,
              presence_device.GetDeviceIdentityMetadata().device_name());
    EXPECT_EQ(
        std::string(kMacAddress.begin(), kMacAddress.end()),
        presence_device.GetDeviceIdentityMetadata().bluetooth_mac_address());
    EXPECT_EQ(std::string(kDeviceId.begin(), kDeviceId.end()),
              presence_device.GetDeviceIdentityMetadata().device_id());

    std::move(next_scan_delegate_callback_).Run();
  }

  void OnScanSessionInvalidated() override {
    scan_session_invalidated_called_ = true;
  }
  bool WasScanSessionInvalidatedCalled() {
    return scan_session_invalidated_called_;
  }
  bool WasOnPresenceDeviceFoundCalled() { return found_called_; }
  bool WasOnPresenceDeviceChangedCalled() { return changed_called_; }
  bool WasOnPresenceDeviceLostCalled() { return lost_called_; }
  void SetNextScanDelegateCallback(base::OnceClosure callback) {
    next_scan_delegate_callback_ = std::move(callback);
  }

 private:
  base::OnceClosure next_scan_delegate_callback_;
  bool found_called_ = false;
  bool changed_called_ = false;
  bool lost_called_ = false;
  bool scan_session_invalidated_called_ = false;
};

}  // namespace

class NearbyPresenceServiceImplTest : public testing::Test {
 public:
  NearbyPresenceServiceImplTest() {
    shared_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            base::BindOnce([]() -> network::mojom::URLLoaderFactory* {
              ADD_FAILURE() << "Did not expect this to actually be used";
              return nullptr;
            }));
  }

  ~NearbyPresenceServiceImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    EXPECT_CALL(nearby_process_manager_, GetNearbyProcessReference)
        .WillRepeatedly([&](ash::nearby::NearbyProcessManager::
                                NearbyProcessStoppedCallback) {
          nearby_process_reference_ =
              std::make_unique<ash::nearby::MockNearbyProcessManager::
                                   MockNearbyProcessReference>();

          EXPECT_CALL(*(nearby_process_reference_.get()), GetNearbyPresence)
              .WillRepeatedly(
                  testing::ReturnRef(fake_nearby_presence_.shared_remote()));
          ON_CALL(*(nearby_process_reference_.get()), GetNearbyConnections)
              .WillByDefault(
                  testing::ReturnRef(nearby_connections_.shared_remote()));
          return std::move(nearby_process_reference_);
        });
    push_notification_service_ =
        std::make_unique<push_notification::FakePushNotificationService>();

    nearby_presence_service_ = std::make_unique<NearbyPresenceServiceImpl>(
        pref_service_.get(), &nearby_process_manager_,
        identity_test_env_.identity_manager(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        push_notification_service_.get());

    histogram_tester_ = std::make_unique<base::HistogramTester>();

    InitializeNearbyPresenceService();
  }

  void TestStartScan(::nearby::internal::IdentityType identity_type) {
    NearbyPresenceService::ScanFilter filter(identity_type,
                                             /*actions=*/{});
    {
      auto run_loop = base::RunLoop();

      // Call start scan and verify it calls the OnPresenceDeviceFound delegate
      // function.
      nearby_presence_service_->StartScan(
          filter, &scan_delegate_,
          base::BindOnce(&NearbyPresenceServiceImplTest::TestOnScanStarted,
                         weak_ptr_factory_.GetWeakPtr(),
                         run_loop.QuitClosure()));

      run_loop.Run();
    }

    task_environment_.FastForwardBy(kLatencyDelta);

    {
      auto run_loop = base::RunLoop();
      scan_delegate_.SetNextScanDelegateCallback(run_loop.QuitClosure());

      std::vector<mojom::ActionType> actions;

      actions.push_back(kAction1);
      actions.push_back(kAction2);
      actions.push_back(kAction3);

      fake_nearby_presence_.ReturnScanObserver()->OnDeviceFound(
          mojom::PresenceDevice::New(
              kEndpointId, actions, kStableDeviceId,
              mojom::Metadata::New(mojom::PresenceDeviceType::kPhone,
                                   kDeviceName, kMacAddress, kDeviceId),
              /*decrypt_shared_credential=*/nullptr));
      run_loop.Run();
    }

    EXPECT_TRUE(scan_delegate_.WasOnPresenceDeviceFoundCalled());
    EXPECT_TRUE(IsScanSessionActive());
    histogram_tester()->ExpectTimeBucketCount(
        "Nearby.Presence.DeviceFound.Latency", kLatencyDelta, 1);
  }

  void TestOnScanStarted(
      base::OnceClosure on_complete,
      std::unique_ptr<ash::nearby::presence::NearbyPresenceService::ScanSession>
          scan_session,
      ash::nearby::presence::enums::StatusCode status) {
    scan_session_ = std::move(scan_session);
    std::move(on_complete).Run();
  }

  void EndScanSession() { scan_session_.reset(); }

  bool IsScanSessionActive() { return scan_session_ != nullptr; }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  FakeNearbyPresence fake_nearby_presence_;
  FakeScanDelegate scan_delegate_;
  testing::NiceMock<ash::nearby::MockNearbyConnections> nearby_connections_;
  testing::NiceMock<MockNearbyProcessManager> nearby_process_manager_;
  std::unique_ptr<
      ash::nearby::MockNearbyProcessManager::MockNearbyProcessReference>
      nearby_process_reference_;

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<push_notification::FakePushNotificationService>
      push_notification_service_;
  std::unique_ptr<NearbyPresenceService> nearby_presence_service_;
  std::unique_ptr<ash::nearby::presence::NearbyPresenceService::ScanSession>
      scan_session_;
  raw_ptr<FakeNearbyPresenceCredentialManager> fake_credential_manager_ptr_;
  base::WeakPtrFactory<NearbyPresenceServiceImplTest> weak_ptr_factory_{this};

 private:
  // This work is collected into a helper function to ensure it all happens
  // together. Most notably, `SetNextCredentialManagerInstanceForTesting()`
  // leaves `NearbyPresenceCredentialManagerImpl::Creator` in a dangling state
  // (holding onto a static test instance) until
  // `NearbyPresenceCredentialManagerImpl::Creator::Create()` is called (in
  // `NearbyPresenceService::Initialize`).
  //
  // `NearbyPresenceService` should also not be used until it is initialized,
  // and this function helps codify that.
  void InitializeNearbyPresenceService() {
    auto fake_credential_manager =
        std::make_unique<FakeNearbyPresenceCredentialManager>();
    fake_credential_manager_ptr_ = fake_credential_manager.get();
    NearbyPresenceCredentialManagerImpl::Creator::
        SetNextCredentialManagerInstanceForTesting(
            std::move(fake_credential_manager));
    EXPECT_FALSE(fake_credential_manager_ptr_->WasUpdateCredentialsCalled());

    base::MockCallback<base::OnceClosure> mock_on_initialized_callback;
    EXPECT_CALL(mock_on_initialized_callback, Run);
    nearby_presence_service_->Initialize(mock_on_initialized_callback.Get());
  }
};

TEST_F(NearbyPresenceServiceImplTest, StartPrivateScan) {
  TestStartScan(::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP);
}

TEST_F(NearbyPresenceServiceImplTest, StartPublicScan) {
  TestStartScan(::nearby::internal::IdentityType::IDENTITY_TYPE_PUBLIC);
}

TEST_F(NearbyPresenceServiceImplTest, StartTrustedScan) {
  TestStartScan(::nearby::internal::IdentityType::IDENTITY_TYPE_CONTACTS_GROUP);
}

TEST_F(NearbyPresenceServiceImplTest, ConfirmScanSessionDtorIsCalled) {
  TestStartScan(::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP);
  scan_session_.reset();

  // The scan_session_ destructor should invalidate all ScanDelegates. Confirm
  // that the scan session destructor is called when the scan session is
  // destroyed.
  EXPECT_TRUE(scan_delegate_.WasScanSessionInvalidatedCalled());
}

TEST_F(NearbyPresenceServiceImplTest, StartScan_DeviceChanged) {
  NearbyPresenceService::ScanFilter filter(
      ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP,
      /*actions=*/{});
  {
    auto run_loop = base::RunLoop();

    // Call start scan and verify it calls the OnPresenceDeviceFound delegate
    // function.
    nearby_presence_service_->StartScan(
        filter, &scan_delegate_,
        base::BindOnce(&NearbyPresenceServiceImplTest::TestOnScanStarted,
                       weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()));

    run_loop.Run();
  }

  {
    auto run_loop = base::RunLoop();
    scan_delegate_.SetNextScanDelegateCallback(run_loop.QuitClosure());

    std::vector<mojom::ActionType> actions;
    actions.push_back(kAction1);
    actions.push_back(kAction2);

    fake_nearby_presence_.ReturnScanObserver()->OnDeviceChanged(
        mojom::PresenceDevice::New(
            kEndpointId, actions, kStableDeviceId,
            mojom::Metadata::New(mojom::PresenceDeviceType::kPhone, kDeviceName,
                                 kMacAddress, kDeviceId),
            /*decrypt_shared_credential=*/nullptr));
    run_loop.Run();
  }

  EXPECT_TRUE(scan_delegate_.WasOnPresenceDeviceChangedCalled());
  EXPECT_TRUE(IsScanSessionActive());
  histogram_tester()->ExpectBucketCount("Nearby.Presence.ScanRequest.Result",
                                        enums::StatusCode::kAbslOk, 1);
}

TEST_F(NearbyPresenceServiceImplTest, StartScan_DeviceLost) {
  NearbyPresenceService::ScanFilter filter(
      ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP,
      /*actions=*/{});
  {
    auto run_loop = base::RunLoop();

    // Call start scan and verify it calls the OnPresenceDeviceFound delegate
    // function.
    nearby_presence_service_->StartScan(
        filter, &scan_delegate_,
        base::BindOnce(&NearbyPresenceServiceImplTest::TestOnScanStarted,
                       weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()));

    run_loop.Run();
  }

  {
    auto run_loop = base::RunLoop();
    scan_delegate_.SetNextScanDelegateCallback(run_loop.QuitClosure());

    std::vector<mojom::ActionType> actions;
    fake_nearby_presence_.ReturnScanObserver()->OnDeviceLost(
        mojom::PresenceDevice::New(
            kEndpointId, actions, kStableDeviceId,
            mojom::Metadata::New(mojom::PresenceDeviceType::kPhone, kDeviceName,
                                 kMacAddress, kDeviceId),
            /*decrypt_shared_credential=*/nullptr));
    run_loop.Run();
  }

  EXPECT_TRUE(scan_delegate_.WasOnPresenceDeviceLostCalled());
  EXPECT_TRUE(IsScanSessionActive());
  histogram_tester()->ExpectBucketCount("Nearby.Presence.ScanRequest.Result",
                                        enums::StatusCode::kAbslOk, 1);
}

TEST_F(NearbyPresenceServiceImplTest, EndScan) {
  NearbyPresenceService::ScanFilter filter(
      ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP,
      /*actions=*/{});

  {
    auto run_loop = base::RunLoop();

    // Call start scan and verify it calls the OnPresenceDeviceFound delegate
    // function.
    nearby_presence_service_->StartScan(
        filter, &scan_delegate_,
        base::BindOnce(&NearbyPresenceServiceImplTest::TestOnScanStarted,
                       weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()));

    run_loop.Run();
  }

  task_environment_.FastForwardBy(kLatencyDelta);

  {
    auto run_loop = base::RunLoop();
    scan_delegate_.SetNextScanDelegateCallback(run_loop.QuitClosure());

    std::vector<mojom::ActionType> actions;
    actions.push_back(kAction1);
    fake_nearby_presence_.ReturnScanObserver()->OnDeviceFound(
        mojom::PresenceDevice::New(
            kEndpointId, actions, kStableDeviceId,
            mojom::Metadata::New(mojom::PresenceDeviceType::kPhone, kDeviceName,
                                 kMacAddress, kDeviceId),
            /*decrypt_shared_credential=*/nullptr));

    // Allow the ScanObserver function to finish before checking EXPECTs.
    run_loop.Run();
  }

  EXPECT_TRUE(scan_delegate_.WasOnPresenceDeviceFoundCalled());
  EXPECT_TRUE(IsScanSessionActive());
  histogram_tester()->ExpectTimeBucketCount(
      "Nearby.Presence.DeviceFound.Latency", kLatencyDelta, 1);

  {
    auto run_loop = base::RunLoop();
    fake_nearby_presence_.SetOnDisconnectCallback(run_loop.QuitClosure());
    EndScanSession();
    run_loop.Run();
  }

  EXPECT_FALSE(IsScanSessionActive());
}

TEST_F(NearbyPresenceServiceImplTest, EndScanBeforeStart) {
  NearbyPresenceService::ScanFilter filter(
      ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP,
      /*actions=*/{});

  EndScanSession();
  EXPECT_FALSE(IsScanSessionActive());

  {
    auto run_loop = base::RunLoop();

    // Call start scan and verify it calls the OnPresenceDeviceFound delegate
    // function.
    nearby_presence_service_->StartScan(
        filter, &scan_delegate_,
        base::BindOnce(&NearbyPresenceServiceImplTest::TestOnScanStarted,
                       weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()));

    run_loop.Run();
  }

  EXPECT_TRUE(IsScanSessionActive());
}

TEST_F(NearbyPresenceServiceImplTest, UpdateCredentials) {
  nearby_presence_service_->UpdateCredentials();
  EXPECT_TRUE(fake_credential_manager_ptr_->WasUpdateCredentialsCalled());
}

TEST_F(NearbyPresenceServiceImplTest, ValidPushNotificationMessageReceived) {
  push_notification::PushNotificationClientManager::PushNotificationMessage
      message;
  message.data.insert_or_assign(push_notification::kNotificationClientIdKey,
                                kNearbyPresencePushNotificationClientId);
  message.data.insert_or_assign(push_notification::kNotificationTypeIdKey,
                                kNearbyPresencePushNotificationTypeId);
  push_notification_service_->GetPushNotificationClientManager()
      ->NotifyPushNotificationClientOfMessage(std::move(message));

  EXPECT_TRUE(fake_credential_manager_ptr_->WasUpdateCredentialsCalled());
}

TEST_F(NearbyPresenceServiceImplTest, InvalidPushNotificationTypeId) {
  push_notification::PushNotificationClientManager::PushNotificationMessage
      message;
  message.data.insert_or_assign(push_notification::kNotificationClientIdKey,
                                kNearbyPresencePushNotificationClientId);
  message.data.insert_or_assign(push_notification::kNotificationTypeIdKey,
                                kMalformedTypeId);
  push_notification_service_->GetPushNotificationClientManager()
      ->NotifyPushNotificationClientOfMessage(std::move(message));

  EXPECT_FALSE(fake_credential_manager_ptr_->WasUpdateCredentialsCalled());
}

TEST_F(NearbyPresenceServiceImplTest, InvalidPushNotificationClientId) {
  push_notification::PushNotificationClientManager::PushNotificationMessage
      message;
  message.data.insert_or_assign(push_notification::kNotificationClientIdKey,
                                kMalformedClientId);
  message.data.insert_or_assign(push_notification::kNotificationTypeIdKey,
                                kNearbyPresencePushNotificationTypeId);
  push_notification_service_->GetPushNotificationClientManager()
      ->NotifyPushNotificationClientOfMessage(std::move(message));

  EXPECT_FALSE(fake_credential_manager_ptr_->WasUpdateCredentialsCalled());
}

TEST_F(NearbyPresenceServiceImplTest, NullProcessReference) {
  NearbyPresenceService::ScanFilter filter(
      ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP,
      /*actions=*/{});

  EXPECT_CALL(nearby_process_manager_, GetNearbyProcessReference)
      .WillRepeatedly(
          [&](ash::nearby::NearbyProcessManager::NearbyProcessStoppedCallback) {
            return nullptr;
          });

  {
    auto run_loop = base::RunLoop();
    nearby_presence_service_->StartScan(
        filter, &scan_delegate_,
        base::BindOnce(&NearbyPresenceServiceImplTest::TestOnScanStarted,
                       weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()));
  }

  EXPECT_FALSE(scan_delegate_.WasOnPresenceDeviceFoundCalled());
}

TEST_F(NearbyPresenceServiceImplTest, Reset) {
  // Test that stopping the Nearby Process does not cause any crashes.
  nearby_process_reference_.reset();
}

TEST_F(NearbyPresenceServiceImplTest, CreateNearbyPresenceConnectionsManager) {
  auto connections_manager =
      nearby_presence_service_->CreateNearbyPresenceConnectionsManager();
  EXPECT_TRUE(connections_manager);
}

}  // namespace ash::nearby::presence
