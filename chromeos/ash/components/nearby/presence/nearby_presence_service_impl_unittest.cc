// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/nearby_presence_service_impl.h"

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_nearby_presence.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_process_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace ash::nearby::presence {

const char kDeviceName[] = "Pepper's Request";
const char kEndpointId[] = "00000001";
const char kStableDeviceId[] = "00000002";

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
  }
  void OnPresenceDeviceChanged(
      const NearbyPresenceService::PresenceDevice& presence_device) override {
    changed_called = true;
  }
  void OnPresenceDeviceLost(
      const NearbyPresenceService::PresenceDevice& presence_device) override {
    lost_called = true;
  }

  void OnScanSessionInvalidated() override {}
  bool WasOnPresenceDeviceFoundCalled() { return found_called; }
  bool WasOnPresenceDeviceChangedCalled() { return changed_called; }
  bool WasOnPresenceDeviceLostCalled() { return lost_called; }

 private:
  bool found_called = false;
  bool changed_called = false;
  bool lost_called = false;
};

}  // namespace

class NearbyPresenceServiceImplTest : public testing::Test {
 public:
  NearbyPresenceServiceImplTest() = default;
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
                  testing::ReturnRef(nearby_presence_.shared_remote()));
          return std::move(nearby_process_reference_);
        });

    nearby_presence_service = std::make_unique<NearbyPresenceServiceImpl>(
        pref_service_.get(), &nearby_process_manager_);
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

  content::BrowserTaskEnvironment task_environment_;
  FakeNearbyPresence nearby_presence_;
  testing::NiceMock<MockNearbyProcessManager> nearby_process_manager_;
  std::unique_ptr<
      ash::nearby::MockNearbyProcessManager::MockNearbyProcessReference>
      nearby_process_reference_;

  std::unique_ptr<NearbyPresenceServiceImpl> nearby_presence_service;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<ash::nearby::presence::NearbyPresenceService::ScanSession>
      scan_session_;
  base::WeakPtrFactory<NearbyPresenceServiceImplTest> weak_ptr_factory_{this};
};

TEST_F(NearbyPresenceServiceImplTest, StartScan) {
  NearbyPresenceService::ScanFilter filter(
      ash::nearby::presence::NearbyPresenceService::IdentityType::kPrivate,
      /*actions=*/{});
  FakeScanDelegate scan_delegate;
  auto run_loop = base::RunLoop();
  // Call start scan and verify it calls the OnPresenceDeviceFound delegate
  // function.
  nearby_presence_service->StartScan(
      filter, &scan_delegate,
      base::BindOnce(&NearbyPresenceServiceImplTest::TestOnScanStarted,
                     weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()));

  // Allow StartScan() to finish before calling the callback.
  base::RunLoop().RunUntilIdle();
  nearby_presence_.RunStartScanCallback();
  run_loop.Run();
  nearby_presence_.ReturnScanObserver()->OnDeviceFound(
      mojom::PresenceDevice::New(kEndpointId, kDeviceName,
                                 mojom::PresenceDeviceType::kPhone,
                                 kStableDeviceId));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(scan_delegate.WasOnPresenceDeviceFoundCalled());
  EXPECT_TRUE(IsScanSessionActive());
}

TEST_F(NearbyPresenceServiceImplTest, StartScan_DeviceChanged) {
  NearbyPresenceService::ScanFilter filter(
      ash::nearby::presence::NearbyPresenceService::IdentityType::kPrivate,
      /*actions=*/{});
  FakeScanDelegate scan_delegate;
  auto run_loop = base::RunLoop();
  // Call start scan and verify it calls the OnPresenceDeviceFound delegate
  // function.
  nearby_presence_service->StartScan(
      filter, &scan_delegate,
      base::BindOnce(&NearbyPresenceServiceImplTest::TestOnScanStarted,
                     weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()));

  // Allow StartScan() to finish before calling the callback.
  base::RunLoop().RunUntilIdle();
  nearby_presence_.RunStartScanCallback();
  run_loop.Run();
  nearby_presence_.ReturnScanObserver()->OnDeviceChanged(
      mojom::PresenceDevice::New(kEndpointId, kDeviceName,
                                 mojom::PresenceDeviceType::kPhone,
                                 kStableDeviceId));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(scan_delegate.WasOnPresenceDeviceChangedCalled());
  EXPECT_TRUE(IsScanSessionActive());
}

TEST_F(NearbyPresenceServiceImplTest, StartScan_DeviceLost) {
  NearbyPresenceService::ScanFilter filter(
      ash::nearby::presence::NearbyPresenceService::IdentityType::kPrivate,
      /*actions=*/{});
  FakeScanDelegate scan_delegate;
  auto run_loop = base::RunLoop();
  // Call start scan and verify it calls the OnPresenceDeviceFound delegate
  // function.
  nearby_presence_service->StartScan(
      filter, &scan_delegate,
      base::BindOnce(&NearbyPresenceServiceImplTest::TestOnScanStarted,
                     weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()));

  // Allow StartScan() to finish before calling the callback.
  base::RunLoop().RunUntilIdle();
  nearby_presence_.RunStartScanCallback();
  run_loop.Run();
  nearby_presence_.ReturnScanObserver()->OnDeviceLost(
      mojom::PresenceDevice::New(kEndpointId, kDeviceName,
                                 mojom::PresenceDeviceType::kPhone,
                                 kStableDeviceId));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(scan_delegate.WasOnPresenceDeviceLostCalled());
  EXPECT_TRUE(IsScanSessionActive());
}

TEST_F(NearbyPresenceServiceImplTest, EndScan) {
  NearbyPresenceService::ScanFilter filter(
      ash::nearby::presence::NearbyPresenceService::IdentityType::kPrivate,
      /*actions=*/{});
  FakeScanDelegate scan_delegate;
  auto run_loop = base::RunLoop();

  // Call start scan and verify it calls the OnPresenceDeviceFound delegate
  // function.
  nearby_presence_service->StartScan(
      filter, &scan_delegate,
      base::BindOnce(&NearbyPresenceServiceImplTest::TestOnScanStarted,
                     weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()));

  // Allow StartScan() to finish before calling the callback.
  base::RunLoop().RunUntilIdle();
  nearby_presence_.RunStartScanCallback();
  run_loop.Run();
  nearby_presence_.ReturnScanObserver()->OnDeviceFound(
      mojom::PresenceDevice::New(kEndpointId, kDeviceName,
                                 mojom::PresenceDeviceType::kPhone,
                                 kStableDeviceId));

  // Allow the ScanObserver function to finish before checking EXPECTs.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(scan_delegate.WasOnPresenceDeviceFoundCalled());
  EXPECT_TRUE(IsScanSessionActive());

  EndScanSession();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsScanSessionActive());
  EXPECT_TRUE(nearby_presence_.WasOnDisconnectCalled());
}

TEST_F(NearbyPresenceServiceImplTest, EndScanBeforeStart) {
  NearbyPresenceService::ScanFilter filter(
      ash::nearby::presence::NearbyPresenceService::IdentityType::kPrivate,
      /*actions=*/{});
  FakeScanDelegate scan_delegate;
  auto run_loop = base::RunLoop();

  EndScanSession();
  EXPECT_FALSE(IsScanSessionActive());

  // Call start scan and verify it calls the OnPresenceDeviceFound delegate
  // function.
  nearby_presence_service->StartScan(
      filter, &scan_delegate,
      base::BindOnce(&NearbyPresenceServiceImplTest::TestOnScanStarted,
                     weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()));

  // Allow StartScan() to finish before calling the callback.
  base::RunLoop().RunUntilIdle();
  nearby_presence_.RunStartScanCallback();
  run_loop.Run();
  EXPECT_TRUE(IsScanSessionActive());
}

TEST_F(NearbyPresenceServiceImplTest, NullProcessReference) {
  NearbyPresenceService::ScanFilter filter(
      ash::nearby::presence::NearbyPresenceService::IdentityType::kPrivate,
      /*actions=*/{});
  FakeScanDelegate scan_delegate;
  auto run_loop = base::RunLoop();

  EXPECT_CALL(nearby_process_manager_, GetNearbyProcessReference)
      .WillRepeatedly(
          [&](ash::nearby::NearbyProcessManager::NearbyProcessStoppedCallback) {
            return nullptr;
          });

  nearby_presence_service->StartScan(
      filter, &scan_delegate,
      base::BindOnce(&NearbyPresenceServiceImplTest::TestOnScanStarted,
                     weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()));
  EXPECT_FALSE(scan_delegate.WasOnPresenceDeviceFoundCalled());
}

TEST_F(NearbyPresenceServiceImplTest, Reset) {
  // Test that stopping the Nearby Process does not cause any crashes.
  // TODO(b/277819923): When metric is added for Nearby Process shutdown
  // reason, test the metric is correctly recorded here.
  nearby_process_reference_.reset();
}

}  // namespace ash::nearby::presence
