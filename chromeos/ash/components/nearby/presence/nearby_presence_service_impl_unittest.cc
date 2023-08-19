// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/nearby_presence_service_impl.h"

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "chromeos/ash/components/nearby/presence/credentials/fake_nearby_presence_credential_manager.h"
#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager_impl.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_nearby_presence.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_process_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace ash::nearby::presence {

const char kDeviceName[] = "Pepper's Request";
const char kEndpointId[] = "00000001";
const char kStableDeviceId[] = "00000002";
const mojom::ActionType kAction1 = mojom::ActionType::kInstantTetheringAction;
const mojom::ActionType kAction2 = mojom::ActionType::kActiveUnlockAction;
const mojom::ActionType kAction3 = mojom::ActionType::kPhoneHubAction;

namespace {

class FakeScanDelegate : public NearbyPresenceService::ScanDelegate {
 public:
  FakeScanDelegate() = default;
  FakeScanDelegate(const FakeScanDelegate&) = delete;
  FakeScanDelegate& operator=(const FakeScanDelegate&) = delete;
  ~FakeScanDelegate() override = default;

  void OnPresenceDeviceFound(
      const NearbyPresenceService::PresenceDevice& presence_device) override {
    found_called = true;
    std::move(next_scan_delegate_callback_).Run();
  }
  void OnPresenceDeviceChanged(
      const NearbyPresenceService::PresenceDevice& presence_device) override {
    changed_called = true;
    std::move(next_scan_delegate_callback_).Run();
  }
  void OnPresenceDeviceLost(
      const NearbyPresenceService::PresenceDevice& presence_device) override {
    lost_called = true;
    std::move(next_scan_delegate_callback_).Run();
  }
  void OnScanSessionInvalidated() override {}
  bool WasOnPresenceDeviceFoundCalled() { return found_called; }
  bool WasOnPresenceDeviceChangedCalled() { return changed_called; }
  bool WasOnPresenceDeviceLostCalled() { return lost_called; }
  void SetNextScanDelegateCallback(base::OnceClosure callback) {
    next_scan_delegate_callback_ = std::move(callback);
  }

 private:
  base::OnceClosure next_scan_delegate_callback_;
  bool found_called = false;
  bool changed_called = false;
  bool lost_called = false;
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
          return std::move(nearby_process_reference_);
        });

    nearby_presence_service_ = std::make_unique<NearbyPresenceServiceImpl>(
        pref_service_.get(), &nearby_process_manager_,
        identity_test_env_.identity_manager(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
  }

  void TestStartScan(ash::nearby::presence::NearbyPresenceService::IdentityType
                         identity_type) {
    NearbyPresenceService::ScanFilter filter(identity_type,
                                             /*actions=*/{});
    FakeScanDelegate scan_delegate;
    {
      auto run_loop = base::RunLoop();

      // Call start scan and verify it calls the OnPresenceDeviceFound delegate
      // function.
      nearby_presence_service_->StartScan(
          filter, &scan_delegate,
          base::BindOnce(&NearbyPresenceServiceImplTest::TestOnScanStarted,
                         weak_ptr_factory_.GetWeakPtr(),
                         run_loop.QuitClosure()));

      run_loop.Run();
    }

    {
      auto run_loop = base::RunLoop();
      scan_delegate.SetNextScanDelegateCallback(run_loop.QuitClosure());

      std::vector<mojom::ActionType> actions;

      actions.push_back(kAction1);
      actions.push_back(kAction2);
      actions.push_back(kAction3);
      fake_nearby_presence_.ReturnScanObserver()->OnDeviceFound(
          mojom::PresenceDevice::New(kEndpointId, kDeviceName,
                                     mojom::PresenceDeviceType::kPhone, actions,
                                     kStableDeviceId));
      run_loop.Run();
    }

    EXPECT_TRUE(scan_delegate.WasOnPresenceDeviceFoundCalled());
    EXPECT_TRUE(IsScanSessionActive());
  }

  void TestOnScanStarted(
      base::OnceClosure on_complete,
      std::unique_ptr<ash::nearby::presence::NearbyPresenceService::ScanSession>
          scan_session,
      ash::nearby::presence::mojom::StatusCode status) {
    scan_session_ = std::move(scan_session);
    std::move(on_complete).Run();
  }

  void EndScanSession() { scan_session_.reset(); }

  bool IsScanSessionActive() { return scan_session_ != nullptr; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  FakeNearbyPresence fake_nearby_presence_;
  testing::NiceMock<MockNearbyProcessManager> nearby_process_manager_;
  std::unique_ptr<
      ash::nearby::MockNearbyProcessManager::MockNearbyProcessReference>
      nearby_process_reference_;

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<NearbyPresenceService> nearby_presence_service_;
  std::unique_ptr<ash::nearby::presence::NearbyPresenceService::ScanSession>
      scan_session_;
  base::WeakPtrFactory<NearbyPresenceServiceImplTest> weak_ptr_factory_{this};
};

TEST_F(NearbyPresenceServiceImplTest, StartPrivateScan) {
  TestStartScan(
      ash::nearby::presence::NearbyPresenceService::IdentityType::kPrivate);
}

TEST_F(NearbyPresenceServiceImplTest, StartPublicScan) {
  TestStartScan(
      ash::nearby::presence::NearbyPresenceService::IdentityType::kPublic);
}

TEST_F(NearbyPresenceServiceImplTest, StartTrustedScan) {
  TestStartScan(
      ash::nearby::presence::NearbyPresenceService::IdentityType::kTrusted);
}

TEST_F(NearbyPresenceServiceImplTest, StartScan_DeviceChanged) {
  NearbyPresenceService::ScanFilter filter(
      ash::nearby::presence::NearbyPresenceService::IdentityType::kPrivate,
      /*actions=*/{});
  FakeScanDelegate scan_delegate;
  {
    auto run_loop = base::RunLoop();

    // Call start scan and verify it calls the OnPresenceDeviceFound delegate
    // function.
    nearby_presence_service_->StartScan(
        filter, &scan_delegate,
        base::BindOnce(&NearbyPresenceServiceImplTest::TestOnScanStarted,
                       weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()));

    run_loop.Run();
  }

  {
    auto run_loop = base::RunLoop();
    scan_delegate.SetNextScanDelegateCallback(run_loop.QuitClosure());

    std::vector<mojom::ActionType> actions;
    ;
    actions.push_back(kAction1);
    actions.push_back(kAction2);
    fake_nearby_presence_.ReturnScanObserver()->OnDeviceChanged(
        mojom::PresenceDevice::New(kEndpointId, kDeviceName,
                                   mojom::PresenceDeviceType::kPhone, actions,
                                   kStableDeviceId));
    run_loop.Run();
  }

  EXPECT_TRUE(scan_delegate.WasOnPresenceDeviceChangedCalled());
  EXPECT_TRUE(IsScanSessionActive());
}

TEST_F(NearbyPresenceServiceImplTest, StartScan_DeviceLost) {
  NearbyPresenceService::ScanFilter filter(
      ash::nearby::presence::NearbyPresenceService::IdentityType::kPrivate,
      /*actions=*/{});
  FakeScanDelegate scan_delegate;
  {
    auto run_loop = base::RunLoop();

    // Call start scan and verify it calls the OnPresenceDeviceFound delegate
    // function.
    nearby_presence_service_->StartScan(
        filter, &scan_delegate,
        base::BindOnce(&NearbyPresenceServiceImplTest::TestOnScanStarted,
                       weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()));

    run_loop.Run();
  }

  {
    auto run_loop = base::RunLoop();
    scan_delegate.SetNextScanDelegateCallback(run_loop.QuitClosure());

    std::vector<mojom::ActionType> actions;
    ;
    fake_nearby_presence_.ReturnScanObserver()->OnDeviceLost(
        mojom::PresenceDevice::New(kEndpointId, kDeviceName,
                                   mojom::PresenceDeviceType::kPhone, actions,
                                   kStableDeviceId));
    run_loop.Run();
  }

  EXPECT_TRUE(scan_delegate.WasOnPresenceDeviceLostCalled());
  EXPECT_TRUE(IsScanSessionActive());
}

TEST_F(NearbyPresenceServiceImplTest, EndScan) {
  NearbyPresenceService::ScanFilter filter(
      ash::nearby::presence::NearbyPresenceService::IdentityType::kPrivate,
      /*actions=*/{});
  FakeScanDelegate scan_delegate;

  {
    auto run_loop = base::RunLoop();

    // Call start scan and verify it calls the OnPresenceDeviceFound delegate
    // function.
    nearby_presence_service_->StartScan(
        filter, &scan_delegate,
        base::BindOnce(&NearbyPresenceServiceImplTest::TestOnScanStarted,
                       weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()));

    run_loop.Run();
  }

  {
    auto run_loop = base::RunLoop();
    scan_delegate.SetNextScanDelegateCallback(run_loop.QuitClosure());

    std::vector<mojom::ActionType> actions;
    actions.push_back(kAction1);
    fake_nearby_presence_.ReturnScanObserver()->OnDeviceFound(
        mojom::PresenceDevice::New(kEndpointId, kDeviceName,
                                   mojom::PresenceDeviceType::kPhone, actions,
                                   kStableDeviceId));

    // Allow the ScanObserver function to finish before checking EXPECTs.
    run_loop.Run();
  }

  EXPECT_TRUE(scan_delegate.WasOnPresenceDeviceFoundCalled());
  EXPECT_TRUE(IsScanSessionActive());

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
      ash::nearby::presence::NearbyPresenceService::IdentityType::kPrivate,
      /*actions=*/{});
  FakeScanDelegate scan_delegate;

  EndScanSession();
  EXPECT_FALSE(IsScanSessionActive());

  {
    auto run_loop = base::RunLoop();

    // Call start scan and verify it calls the OnPresenceDeviceFound delegate
    // function.
    nearby_presence_service_->StartScan(
        filter, &scan_delegate,
        base::BindOnce(&NearbyPresenceServiceImplTest::TestOnScanStarted,
                       weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()));

    run_loop.Run();
  }

  EXPECT_TRUE(IsScanSessionActive());
}

TEST_F(NearbyPresenceServiceImplTest, Initialize) {
  auto credential_manager =
      std::make_unique<FakeNearbyPresenceCredentialManager>();
  auto* fake_credential_manager = credential_manager.get();
  NearbyPresenceCredentialManagerImpl::Creator::SetCredentialManagerForTesting(
      std::move(credential_manager));

  base::MockCallback<base::OnceClosure> mock_on_initialized_callback;
  EXPECT_CALL(mock_on_initialized_callback, Run);
  nearby_presence_service_->Initialize(mock_on_initialized_callback.Get());

  nearby_presence_service_->UpdateCredentials();
  EXPECT_TRUE(fake_credential_manager->WasUpdateCredentialsCalled());
}

TEST_F(NearbyPresenceServiceImplTest, UpdateCredentials) {
  auto credential_manager =
      std::make_unique<FakeNearbyPresenceCredentialManager>();
  auto* fake_credential_manager = credential_manager.get();
  NearbyPresenceCredentialManagerImpl::Creator::SetCredentialManagerForTesting(
      std::move(credential_manager));

  EXPECT_FALSE(fake_credential_manager->WasUpdateCredentialsCalled());
  nearby_presence_service_->UpdateCredentials();
  EXPECT_TRUE(fake_credential_manager->WasUpdateCredentialsCalled());
}

TEST_F(NearbyPresenceServiceImplTest, NullProcessReference) {
  NearbyPresenceService::ScanFilter filter(
      ash::nearby::presence::NearbyPresenceService::IdentityType::kPrivate,
      /*actions=*/{});
  FakeScanDelegate scan_delegate;

  EXPECT_CALL(nearby_process_manager_, GetNearbyProcessReference)
      .WillRepeatedly(
          [&](ash::nearby::NearbyProcessManager::NearbyProcessStoppedCallback) {
            return nullptr;
          });

  {
    auto run_loop = base::RunLoop();
    nearby_presence_service_->StartScan(
        filter, &scan_delegate,
        base::BindOnce(&NearbyPresenceServiceImplTest::TestOnScanStarted,
                       weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()));
  }

  EXPECT_FALSE(scan_delegate.WasOnPresenceDeviceFoundCalled());
}

TEST_F(NearbyPresenceServiceImplTest, Reset) {
  // Test that stopping the Nearby Process does not cause any crashes.
  // TODO(b/277819923): When metric is added for Nearby Process shutdown
  // reason, test the metric is correctly recorded here.
  nearby_process_reference_.reset();
}

}  // namespace ash::nearby::presence
