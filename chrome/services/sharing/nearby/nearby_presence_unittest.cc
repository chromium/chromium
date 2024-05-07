// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_presence.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/nearby/presence/conversions/nearby_presence_conversions.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/nearby/src/internal/interop/fake_device_provider.h"
#include "third_party/nearby/src/presence/fake_presence_client.h"
#include "third_party/nearby/src/presence/fake_presence_service.h"
#include "third_party/nearby/src/presence/presence_client_impl.h"
#include "third_party/nearby/src/presence/presence_service_impl.h"

namespace {

const char kRequestName[] = "Pepper's Request";
const char kMacAddress[] = "AA:BB:CC:DD:EE:FF";

const char kDeviceName[] = "Test's Chromebook";
const char kAccountName[] = "test.tester@gmail.com";
const std::vector<uint8_t> kDeviceId = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab,
                                        0xcd, 0xef, 0x01, 0x23, 0x45, 0x67,
                                        0x89, 0xab, 0xcd, 0xef};
const std::vector<uint8_t> kSecretId1 = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
const std::vector<uint8_t> kSecretId2 = {0x22, 0x22, 0x22, 0x22, 0x22, 0x22};
const std::vector<uint8_t> kSecretId3 = {0x33, 0x33, 0x33, 0x33, 0x33, 0x33};

const long kSharedCredId1 = 111;
const long kSharedCredId2 = 222;
const long kSharedCredId3 = 333;

ash::nearby::presence::mojom::MetadataPtr BuildTestMetadata() {
  ash::nearby::presence::mojom::MetadataPtr metadata =
      ash::nearby::presence::mojom::Metadata::New();
  metadata->device_name = kDeviceName;
  metadata->device_id = kDeviceId;
  return metadata;
}

::nearby::internal::DeviceIdentityMetaData BuildTestPresenceClientMetadata() {
  ::nearby::internal::DeviceIdentityMetaData metadata;
  metadata.set_device_name(kDeviceName);
  metadata.set_bluetooth_mac_address(kMacAddress);
  metadata.set_device_id(std::string(kDeviceId.begin(), kDeviceId.end()));
  return metadata;
}

}  // namespace

namespace ash::nearby::presence {

// Derived class allows access to the protected constructor of NearbyPresence
// for unit tests.
class TestNearbyPresence : public NearbyPresence {
 public:
  TestNearbyPresence(
      std::unique_ptr<::nearby::presence::PresenceService> presence_service,
      mojo::PendingReceiver<mojom::NearbyPresence> nearby_presence,
      base::OnceClosure on_disconnect)
      : NearbyPresence(std::move(presence_service),
                       std::move(nearby_presence),
                       std::move(on_disconnect)) {}
};

class NearbyPresenceTest : public testing::Test,
                           public ::ash::nearby::presence::mojom::ScanObserver {
 public:
  NearbyPresenceTest() {
    auto fake_presence_service =
        std::make_unique<::nearby::presence::FakePresenceService>();
    fake_presence_service_ = fake_presence_service.get();

    nearby_presence_ = std::make_unique<TestNearbyPresence>(
        std::move(fake_presence_service), remote_.BindNewPipeAndPassReceiver(),
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
      mojo_base::mojom::AbslStatusCode status) {
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
        ash::nearby::presence::mojom::IdentityType::kIdentityTypePrivateGroup);
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
    last_device_found_name_ = device->metadata->device_name;
    std::move(next_on_device_found_callback_).Run();
  }

  void OnDeviceChanged(
      ash::nearby::presence::mojom::PresenceDevicePtr device) override {
    num_devices_changed_++;
    last_device_changed_name_ = device->metadata->device_name;
    std::move(next_on_device_changed_callback_).Run();
  }

  void OnDeviceLost(
      ash::nearby::presence::mojom::PresenceDevicePtr device) override {
    num_devices_lost_++;
    last_device_lost_name_ = device->metadata->device_name;
    std::move(next_on_device_lost_callback_).Run();
  }

  void SetNextOnDeviceFoundCallback(base::OnceClosure callback) {
    next_on_device_found_callback_ = std::move(callback);
  }

  void SetNextOnDeviceChangedCallback(base::OnceClosure callback) {
    next_on_device_changed_callback_ = std::move(callback);
  }

  void SetNextOnDeviceLostCallback(base::OnceClosure callback) {
    next_on_device_lost_callback_ = std::move(callback);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  bool was_on_scan_started_called = false;
  mojo_base::mojom::AbslStatusCode returned_status_;
  mojo::Remote<mojom::NearbyPresence> remote_;

  mojo::Receiver<::ash::nearby::presence::mojom::ScanObserver> scan_observer_{
      this};
  raw_ptr<::nearby::presence::FakePresenceService, DanglingUntriaged>
      fake_presence_service_ = nullptr;
  std::unique_ptr<NearbyPresence> nearby_presence_;

  int num_devices_found_ = 0;
  int num_devices_changed_ = 0;
  int num_devices_lost_ = 0;
  std::string last_device_found_name_;
  std::string last_device_changed_name_;
  std::string last_device_lost_name_;
  mojo::Remote<::ash::nearby::presence::mojom::ScanSession> scan_session_;
  base::OnceClosure next_on_device_found_callback_;
  base::OnceClosure next_on_device_changed_callback_;
  base::OnceClosure next_on_device_lost_callback_;

 private:
  base::WeakPtrFactory<NearbyPresenceTest> weak_ptr_factory_{this};
};

TEST_F(NearbyPresenceTest, RunStartScan_StatusOk) {
  auto run_loop = base::RunLoop();
  CallStartScan(run_loop.QuitClosure());
  // Nearby Presence StartScan() needs to be able to finish before the start
  // scan callback can be called. Since there is no callback at the end of
  // NearbyPresence::StartScan() RunUntilIdle is necessary here.
  base::RunLoop().RunUntilIdle();
  fake_presence_service_->GetMostRecentFakePresenceClient()
      ->CallStartScanCallback(absl::OkStatus());
  run_loop.Run();

  EXPECT_TRUE(was_on_scan_started_called);
}

TEST_F(NearbyPresenceTest, RunStartScan_StatusNotOk) {
  auto run_loop = base::RunLoop();
  CallStartScan(run_loop.QuitClosure());
  // Nearby Presence StartScan() needs to be able to finish before the start
  // scan callback can be called. Since there is no callback at the end of
  // NearbyPresence::StartScan() RunUntilIdle is necessary here.
  base::RunLoop().RunUntilIdle();
  absl::Status status(absl::StatusCode::kCancelled, "");
  fake_presence_service_->GetMostRecentFakePresenceClient()
      ->CallStartScanCallback(status);
  run_loop.Run();

  EXPECT_TRUE(was_on_scan_started_called);
  EXPECT_FALSE(ScanSessionRemoteIsBound());
  EXPECT_EQ(mojo_base::mojom::AbslStatusCode::kCancelled, returned_status_);
}

TEST_F(NearbyPresenceTest, RunStartScan_DeviceFoundCallback) {
  {
    auto run_loop = base::RunLoop();
    CallStartScan(run_loop.QuitClosure());
    // Nearby Presence StartScan() needs to be able to finish before the start
    // scan callback can be called. Since there is no callback at the end of
    // NearbyPresence::StartScan() RunUntilIdle is necessary here.
    base::RunLoop().RunUntilIdle();
    fake_presence_service_->GetMostRecentFakePresenceClient()
        ->CallStartScanCallback(absl::OkStatus());
    run_loop.Run();
  }

  ::nearby::presence::PresenceDevice device{BuildTestPresenceClientMetadata()};
  {
    auto run_loop = base::RunLoop();
    SetNextOnDeviceFoundCallback(run_loop.QuitClosure());
    fake_presence_service_->GetMostRecentFakePresenceClient()->CallOnDiscovered(
        device);

    run_loop.Run();
  }

  EXPECT_TRUE(was_on_scan_started_called);
  EXPECT_EQ(last_device_found_name_,
            device.GetDeviceIdentityMetadata().device_name());
  EXPECT_EQ(1, num_devices_found_);
}

TEST_F(NearbyPresenceTest, RunStartScan_DeviceChangedCallback) {
  {
    auto run_loop = base::RunLoop();
    CallStartScan(run_loop.QuitClosure());
    // Nearby Presence StartScan() needs to be able to finish before the start
    // scan callback can be called. Since there is no callback at the end of
    // NearbyPresence::StartScan() RunUntilIdle is necessary here.
    base::RunLoop().RunUntilIdle();
    fake_presence_service_->GetMostRecentFakePresenceClient()
        ->CallStartScanCallback(absl::OkStatus());
    run_loop.Run();
  }

  ::nearby::presence::PresenceDevice device{BuildTestPresenceClientMetadata()};
  {
    auto run_loop = base::RunLoop();
    SetNextOnDeviceChangedCallback(run_loop.QuitClosure());
    fake_presence_service_->GetMostRecentFakePresenceClient()->CallOnUpdated(
        device);

    run_loop.Run();
  }

  EXPECT_TRUE(was_on_scan_started_called);
  EXPECT_EQ(last_device_changed_name_,
            device.GetDeviceIdentityMetadata().device_name());
  EXPECT_EQ(1, num_devices_changed_);
}

TEST_F(NearbyPresenceTest, RunStartScan_DeviceLostCallback) {
  {
    auto run_loop = base::RunLoop();
    CallStartScan(run_loop.QuitClosure());
    // Nearby Presence StartScan() needs to be able to finish before the start
    // scan callback can be called. Since there is no callback at the end of
    // NearbyPresence::StartScan() RunUntilIdle is necessary here.
    base::RunLoop().RunUntilIdle();
    fake_presence_service_->GetMostRecentFakePresenceClient()
        ->CallStartScanCallback(absl::OkStatus());
    run_loop.Run();
  }

  ::nearby::presence::PresenceDevice device{BuildTestPresenceClientMetadata()};
  {
    auto run_loop = base::RunLoop();
    SetNextOnDeviceLostCallback(run_loop.QuitClosure());
    fake_presence_service_->GetMostRecentFakePresenceClient()->CallOnLost(
        device);

    run_loop.Run();
  }

  EXPECT_TRUE(was_on_scan_started_called);
  EXPECT_EQ(last_device_lost_name_,
            device.GetDeviceIdentityMetadata().device_name());
  EXPECT_EQ(1, num_devices_lost_);
}

TEST_F(NearbyPresenceTest, RunUpdateLocalDeviceMetadata) {
  nearby_presence_->UpdateLocalDeviceMetadata(BuildTestMetadata());

  ::nearby::internal::DeviceIdentityMetaData local_device_metadata =
      fake_presence_service_->GetDeviceIdentityMetaData();
  EXPECT_EQ(kDeviceName, local_device_metadata.device_name());
  EXPECT_EQ(std::string(kDeviceId.begin(), kDeviceId.end()),
            local_device_metadata.device_id());
}

TEST_F(NearbyPresenceTest,
       UpdateLocalDeviceMetadataAndGenerateCredentials_Success) {
  ::nearby::internal::SharedCredential shared_credential1;
  shared_credential1.set_id(kSharedCredId1);
  ::nearby::internal::SharedCredential shared_credential2;
  shared_credential2.set_id(kSharedCredId2);
  ::nearby::internal::SharedCredential shared_credential3;
  shared_credential3.set_id(kSharedCredId3);
  fake_presence_service_->SetUpdateLocalDeviceMetadataResponse(
      absl::Status(/*response_code=*/absl::StatusCode::kOk,
                   /*msg=*/std::string()),
      /*shared_credentials=*/{shared_credential1, shared_credential2,
                              shared_credential3});

  base::RunLoop run_loop;
  nearby_presence_->UpdateLocalDeviceMetadataAndGenerateCredentials(
      BuildTestMetadata(),
      base::BindLambdaForTesting(
          [&](std::vector<mojom::SharedCredentialPtr> shared_credentials,
              mojo_base::mojom::AbslStatusCode status) {
            EXPECT_EQ(3u, shared_credentials.size());
            EXPECT_EQ(kSharedCredId1, shared_credentials[0]->id);
            EXPECT_EQ(kSharedCredId2, shared_credentials[1]->id);
            EXPECT_EQ(kSharedCredId3, shared_credentials[2]->id);
            EXPECT_EQ(mojo_base::mojom::AbslStatusCode::kOk, status);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(NearbyPresenceTest,
       UpdateLocalDeviceMetadataAndGenerateCredentials_Fail) {
  fake_presence_service_->SetUpdateLocalDeviceMetadataResponse(
      absl::Status(/*response_code=*/absl::StatusCode::kCancelled,
                   /*msg=*/std::string()),
      /*shared_credentials=*/{});

  base::RunLoop run_loop;
  nearby_presence_->UpdateLocalDeviceMetadataAndGenerateCredentials(
      BuildTestMetadata(),
      base::BindLambdaForTesting(
          [&](std::vector<mojom::SharedCredentialPtr> shared_credentials,
              mojo_base::mojom::AbslStatusCode status) {
            EXPECT_TRUE(shared_credentials.empty());
            EXPECT_EQ(mojo_base::mojom::AbslStatusCode::kCancelled, status);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(NearbyPresenceTest, UpdateRemoteSharedCredentials_Success) {
  std::vector<mojom::SharedCredentialPtr> remote_creds;
  mojom::SharedCredentialPtr shared_credential1 =
      mojom::SharedCredential::New();
  shared_credential1->id = kSharedCredId1;
  remote_creds.push_back(std::move(shared_credential1));
  mojom::SharedCredentialPtr shared_credential2 =
      mojom::SharedCredential::New();
  shared_credential2->id = kSharedCredId2;
  remote_creds.push_back(std::move(shared_credential2));
  mojom::SharedCredentialPtr shared_credential3 =
      mojom::SharedCredential::New();
  shared_credential3->id = kSharedCredId3;
  remote_creds.push_back(std::move(shared_credential3));

  fake_presence_service_->SetUpdateRemoteSharedCredentialsResult(
      absl::Status(absl::StatusCode::kOk, /*msg=*/std::string()));

  base::RunLoop run_loop;
  nearby_presence_->UpdateRemoteSharedCredentials(
      std::move(remote_creds), kAccountName,
      base::BindLambdaForTesting([&](mojo_base::mojom::AbslStatusCode status) {
        EXPECT_EQ(mojo_base::mojom::AbslStatusCode::kOk, status);
        run_loop.Quit();
      }));
  run_loop.Run();

  auto creds = fake_presence_service_->GetRemoteSharedCredentials();
  EXPECT_FALSE(creds.empty());
  EXPECT_EQ(3u, creds.size());
  EXPECT_EQ(kSharedCredId1, creds[0].id());
  EXPECT_EQ(kSharedCredId2, creds[1].id());
  EXPECT_EQ(kSharedCredId3, creds[2].id());
}

TEST_F(NearbyPresenceTest, UpdateRemoteSharedCredentials_Fail) {
  std::vector<mojom::SharedCredentialPtr> remote_creds;
  mojom::SharedCredentialPtr shared_credential1 =
      mojom::SharedCredential::New();
  shared_credential1->id = kSharedCredId1;
  remote_creds.push_back(std::move(shared_credential1));
  mojom::SharedCredentialPtr shared_credential2 =
      mojom::SharedCredential::New();
  shared_credential2->id = kSharedCredId2;
  remote_creds.push_back(std::move(shared_credential2));
  mojom::SharedCredentialPtr shared_credential3 =
      mojom::SharedCredential::New();
  shared_credential3->id = kSharedCredId3;
  remote_creds.push_back(std::move(shared_credential3));

  fake_presence_service_->SetUpdateRemoteSharedCredentialsResult(
      absl::Status(absl::StatusCode::kCancelled, /*msg=*/std::string()));

  base::RunLoop run_loop;
  nearby_presence_->UpdateRemoteSharedCredentials(
      std::move(remote_creds), kAccountName,
      base::BindLambdaForTesting([&](mojo_base::mojom::AbslStatusCode status) {
        EXPECT_EQ(mojo_base::mojom::AbslStatusCode::kCancelled, status);
        run_loop.Quit();
      }));
  run_loop.Run();

  auto creds = fake_presence_service_->GetRemoteSharedCredentials();
  EXPECT_TRUE(creds.empty());
}

TEST_F(NearbyPresenceTest, GetLocalSharedCredentials_Success) {
  ::nearby::internal::SharedCredential shared_credential1;
  shared_credential1.set_id(kSharedCredId1);
  ::nearby::internal::SharedCredential shared_credential2;
  shared_credential2.set_id(kSharedCredId2);
  ::nearby::internal::SharedCredential shared_credential3;
  shared_credential3.set_id(kSharedCredId3);

  fake_presence_service_->SetLocalPublicCredentialsResult(
      /*status_code=*/absl::Status(absl::StatusCode::kOk,
                                   /*msg=*/std::string()),
      /*shared_credentials=*/{shared_credential1, shared_credential2,
                              shared_credential3});

  base::RunLoop run_loop;
  nearby_presence_->GetLocalSharedCredentials(
      kAccountName,
      base::BindLambdaForTesting(
          [&](std::vector<mojom::SharedCredentialPtr> shared_creds,
              mojo_base::mojom::AbslStatusCode status) {
            EXPECT_EQ(mojo_base::mojom::AbslStatusCode::kOk, status);
            EXPECT_FALSE(shared_creds.empty());
            EXPECT_EQ(3u, shared_creds.size());
            EXPECT_EQ(kSharedCredId1, shared_creds[0]->id);
            EXPECT_EQ(kSharedCredId2, shared_creds[1]->id);
            EXPECT_EQ(kSharedCredId3, shared_creds[2]->id);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(NearbyPresenceTest, GetLocalSharedCredentials_Failure) {
  fake_presence_service_->SetLocalPublicCredentialsResult(
      /*status_code=*/absl::Status(absl::StatusCode::kCancelled,
                                   /*msg=*/std::string()),
      /*shared_credentials=*/{});

  base::RunLoop run_loop;
  nearby_presence_->GetLocalSharedCredentials(
      kAccountName,
      base::BindLambdaForTesting(
          [&](std::vector<mojom::SharedCredentialPtr> shared_creds,
              mojo_base::mojom::AbslStatusCode status) {
            EXPECT_EQ(mojo_base::mojom::AbslStatusCode::kCancelled, status);
            EXPECT_TRUE(shared_creds.empty());
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(NearbyPresenceTest, GetLocalDeviceProvider) {
  ::nearby::FakeDeviceProvider fake_device_provider;
  fake_presence_service_->SetDeviceProvider(&fake_device_provider);
  EXPECT_TRUE(nearby_presence_->GetLocalDeviceProvider());
}

}  // namespace ash::nearby::presence
