// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_presence.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/src/presence/fake_presence_client.h"
#include "third_party/nearby/src/presence/presence_client_impl.h"
#include "third_party/nearby/src/presence/presence_service.h"

namespace {

const char kRequestName[] = "Pepper's Request";

class FakePresenceClientImplFactory
    : public nearby::presence::PresenceClientImpl::Factory {
  using BorrowablePresenceService =
      ::nearby::Borrowable<::nearby::presence::PresenceService*>;

 public:
  ~FakePresenceClientImplFactory() override = default;

  nearby::presence::FakePresenceClient*
  get_last_created_fake_presence_client() {
    return last_created_fake_presence_client_;
  }

 private:
  // PresenceClientImpl::Factory:
  std::unique_ptr<nearby::presence::PresenceClient> CreateInstance(
      BorrowablePresenceService service) override {
    auto fake_presence_client =
        std::make_unique<nearby::presence::FakePresenceClient>();
    last_created_fake_presence_client_ = fake_presence_client.get();
    return fake_presence_client;
  }

  raw_ptr<nearby::presence::FakePresenceClient>
      last_created_fake_presence_client_ = nullptr;
};

}  // namespace

namespace ash::nearby::presence {
class NearbyPresenceTest : public testing::Test,
                           public ::ash::nearby::presence::mojom::ScanObserver {
 public:
  NearbyPresenceTest() {
    ::nearby::presence::PresenceClientImpl::Factory::SetFactoryForTesting(
        &fake_presence_client_factory_);

    nearby_presence_ = std::make_unique<NearbyPresence>(
        remote_.BindNewPipeAndPassReceiver(),
        base::BindOnce(&NearbyPresenceTest::OnDisconnect,
                       base::Unretained(this)));

    remote_->SetScanObserver(scan_observer_.BindNewPipeAndPassRemote());
  }

  ~NearbyPresenceTest() override = default;

  void OnDisconnect() {}

  void OnScanStarted(
      base::OnceClosure on_complete,
      mojo::PendingRemote<::ash::nearby::presence::mojom::ScanSession>
          scan_session,
      ash::nearby::presence::mojom::StatusCode status) {
    was_on_scan_started_called = true;
    returned_status_ = status;
    if (scan_session_) {
      scan_session_.Bind(std::move(scan_session));
    }
    std::move(on_complete).Run();
  }

  void CallStartScan(base::OnceClosure on_complete) {
    std::vector<ash::nearby::presence::mojom::IdentityType> type_vector;
    type_vector.push_back(
        ash::nearby::presence::mojom::IdentityType::kIdentityTypePrivate);
    std::vector<mojom::PresenceScanFilterPtr> filters_vector;
    mojom::PresenceScanFilterPtr filter =
        ash::nearby::presence::mojom::PresenceScanFilter::New(
            mojom::PresenceDeviceType::kChromeos);
    filters_vector.push_back(std::move(filter));

    mojom::ScanRequestPtr scan_request = mojom::ScanRequest::New(
        kRequestName, type_vector, std::move(filters_vector));

    remote_->StartScan(
        std::move(scan_request),
        base::BindOnce(&NearbyPresenceTest::OnScanStarted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(on_complete)));
  }

  bool ScanSessionRemoteIsBound() { return scan_session_.is_bound(); }

  void OnDeviceFound(
      ash::nearby::presence::mojom::PresenceDevicePtr device) override {
    num_devices_found_++;
  }

  void OnDeviceChanged(
      ash::nearby::presence::mojom::PresenceDevicePtr device) override {
    num_devices_changed_++;
  }

  void OnDeviceLost(
      ash::nearby::presence::mojom::PresenceDevicePtr device) override {
    num_devices_lost_++;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  bool was_on_scan_started_called = false;
  ash::nearby::presence::mojom::StatusCode returned_status_;
  mojo::Remote<mojom::NearbyPresence> remote_;

  mojo::Receiver<::ash::nearby::presence::mojom::ScanObserver> scan_observer_{
      this};
  FakePresenceClientImplFactory fake_presence_client_factory_;

  int num_devices_found_ = 0;
  int num_devices_changed_ = 0;
  int num_devices_lost_ = 0;
  mojo::Remote<::ash::nearby::presence::mojom::ScanSession> scan_session_;

 private:
  std::unique_ptr<NearbyPresence> nearby_presence_;
  base::WeakPtrFactory<NearbyPresenceTest> weak_ptr_factory_{this};
};

TEST_F(NearbyPresenceTest, RunStartScan_StatusOk) {
  auto run_loop = base::RunLoop();

  CallStartScan(run_loop.QuitClosure());

  // RunUntilIdle is used here to make sure StartScan() is able to pass
  // FakePresenceClient the callback before it is called on the next line.
  base::RunLoop().RunUntilIdle();
  fake_presence_client_factory_.get_last_created_fake_presence_client()
      ->CallStartScanCallback(absl::OkStatus());
  run_loop.Run();

  EXPECT_TRUE(was_on_scan_started_called);
}

TEST_F(NearbyPresenceTest, RunStartScan_StatusNotOk) {
  auto run_loop = base::RunLoop();

  CallStartScan(run_loop.QuitClosure());

  // RunUntilIdle is used here to make sure StartScan() is able to pass
  // FakePresenceClient the callback before it is called on the next line.
  base::RunLoop().RunUntilIdle();

  absl::Status status(absl::StatusCode::kCancelled, "");
  fake_presence_client_factory_.get_last_created_fake_presence_client()
      ->CallStartScanCallback(status);
  run_loop.Run();

  EXPECT_TRUE(was_on_scan_started_called);
  EXPECT_FALSE(ScanSessionRemoteIsBound());
  EXPECT_EQ(ash::nearby::presence::mojom::StatusCode::kFailure,
            returned_status_);
}

TEST_F(NearbyPresenceTest, RunStartScan_DeviceFoundCallback) {
  auto run_loop = base::RunLoop();
  CallStartScan(run_loop.QuitClosure());
  base::RunLoop().RunUntilIdle();
  fake_presence_client_factory_.get_last_created_fake_presence_client()
      ->CallStartScanCallback(absl::OkStatus());
  run_loop.Run();
  fake_presence_client_factory_.get_last_created_fake_presence_client()
      ->CallOnDiscovered();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(was_on_scan_started_called);
  EXPECT_EQ(1, num_devices_found_);
}

TEST_F(NearbyPresenceTest, RunStartScan_DeviceChangedCallback) {
  auto run_loop = base::RunLoop();

  CallStartScan(run_loop.QuitClosure());
  base::RunLoop().RunUntilIdle();

  fake_presence_client_factory_.get_last_created_fake_presence_client()
      ->CallStartScanCallback(absl::OkStatus());
  run_loop.Run();
  fake_presence_client_factory_.get_last_created_fake_presence_client()
      ->CallOnUpdated();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(was_on_scan_started_called);
  EXPECT_EQ(1, num_devices_changed_);
}

TEST_F(NearbyPresenceTest, RunStartScan_DeviceLostCallback) {
  auto run_loop = base::RunLoop();

  CallStartScan(run_loop.QuitClosure());
  base::RunLoop().RunUntilIdle();
  fake_presence_client_factory_.get_last_created_fake_presence_client()
      ->CallStartScanCallback(absl::OkStatus());
  run_loop.Run();
  fake_presence_client_factory_.get_last_created_fake_presence_client()
      ->CallOnLost();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(was_on_scan_started_called);
  EXPECT_EQ(1, num_devices_lost_);
}

}  // namespace ash::nearby::presence
