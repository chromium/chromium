// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/parcel/parcels_manager.h"
#include "components/commerce/core/parcel/parcels_server_proxy.h"
#include "components/commerce/core/parcel/parcels_storage.h"
#include "components/commerce/core/proto/parcel.pb.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ByMove;
using testing::InSequence;
using testing::Return;

namespace commerce {
namespace {

const std::string kTestTrackingId = "xyz";
const std::string kSoucePageDomain = "www.foo.com";

base::Time GetTestTime() {
  base::Time time;
  EXPECT_TRUE(base::Time::FromString("05/18/20 01:00:00 AM", &time));
  return time;
}

base::Time GetTestDeliveryTime() {
  return GetTestTime() + base::Days(2);
}

std::unique_ptr<std::vector<commerce::ParcelStatus>> BuildParcelStatus(
    const std::string& tracking_id,
    commerce::ParcelStatus::ParcelState state) {
  commerce::ParcelIdentifier identifier;
  identifier.set_tracking_id(tracking_id);
  identifier.set_carrier(commerce::ParcelIdentifier::UPS);
  commerce::ParcelStatus parcel_status;
  parcel_status.set_parcel_state(state);
  *parcel_status.mutable_parcel_identifier() = identifier;
  parcel_status.set_estimated_delivery_time_usec(
      GetTestDeliveryTime().ToDeltaSinceWindowsEpoch().InMicroseconds());
  auto result = std::make_unique<std::vector<commerce::ParcelStatus>>();
  result->emplace_back(parcel_status);
  return result;
}

std::unique_ptr<std::vector<parcel_tracking_db::ParcelTrackingContent>>
BuildParcelTracking(const std::string& tracking_id,
                    commerce::ParcelStatus::ParcelState state) {
  parcel_tracking_db::ParcelTrackingContent tracking;
  tracking.set_last_update_time_usec(
      GetTestTime().ToDeltaSinceWindowsEpoch().InMicroseconds());
  auto status = BuildParcelStatus(tracking_id, state);
  *tracking.mutable_parcel_status() = (*status)[0];
  auto result = std::make_unique<
      std::vector<parcel_tracking_db::ParcelTrackingContent>>();
  result->emplace_back(tracking);
  return result;
}

std::vector<std::pair<commerce::ParcelIdentifier::Carrier, std::string>>
GetTestIdentifiers(const std::string& tracking_id) {
  std::vector<std::pair<commerce::ParcelIdentifier::Carrier, std::string>>
      result;
  result.emplace_back(commerce::ParcelIdentifier::UPS, tracking_id);
  return result;
}

void ExpectGetParcelsCallback(
    bool expected_success,
    const std::vector<ParcelTrackingStatus>& expected_parcel_status,
    bool success,
    std::unique_ptr<std::vector<ParcelTrackingStatus>> parcel_status) {
  ASSERT_EQ(success, expected_success);
  ASSERT_EQ(expected_parcel_status.size(), parcel_status->size());
  for (size_t i = 0; i < expected_parcel_status.size(); ++i) {
    auto status = (*parcel_status)[i];
    auto expected_status = expected_parcel_status[i];
    ASSERT_EQ(expected_status.tracking_id, status.tracking_id);
    ASSERT_EQ(expected_status.carrier, status.carrier);
    ASSERT_EQ(expected_status.state, status.state);
  }
}

class MockServerProxy : public ParcelsServerProxy {
 public:
  MockServerProxy()
      : ParcelsServerProxy(
            nullptr,
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}
  MockServerProxy(const MockServerProxy&) = delete;
  MockServerProxy operator=(const MockServerProxy&) = delete;
  ~MockServerProxy() override = default;

  MOCK_METHOD(void,
              GetParcelStatus,
              (const std::vector<ParcelIdentifier>& parcel_identifiers,
               ParcelsServerProxy::GetParcelStatusCallback callback),
              (override));
  MOCK_METHOD(void,
              StartTrackingParcels,
              (const std::vector<ParcelIdentifier>& parcel_identifiers,
               const std::string& source_page_domain,
               ParcelsServerProxy::GetParcelStatusCallback callback),
              (override));
  MOCK_METHOD(void,
              StopTrackingParcel,
              (const std::string& tracking_id,
               ParcelsServerProxy::StopParcelTrackingCallback callback),
              (override));
  MOCK_METHOD(void,
              StopTrackingParcels,
              (const std::vector<ParcelIdentifier>& parcel_identifiers,
               ParcelsServerProxy::StopParcelTrackingCallback callback),
              (override));
  MOCK_METHOD(void,
              StopTrackingAllParcels,
              (ParcelsServerProxy::StopParcelTrackingCallback callback),
              (override));

  // Mocks the GetParcelStatus and StartTrackingParcels response from server.
  void MockParcelStatusResponses(bool succeeded,
                                 const std::string& tracking_id,
                                 ParcelStatus::ParcelState state) {
    ON_CALL(*this, GetParcelStatus)
        .WillByDefault(
            [succeeded, tracking_id, state](
                const std::vector<ParcelIdentifier>& parcel_identifiers,
                ParcelsServerProxy::GetParcelStatusCallback callback) {
              std::move(callback).Run(succeeded,
                                      BuildParcelStatus(tracking_id, state));
            });
    ON_CALL(*this, StartTrackingParcels)
        .WillByDefault(
            [succeeded, tracking_id, state](
                const std::vector<ParcelIdentifier>& parcel_identifiers,
                const std::string& source_page_domain,
                ParcelsServerProxy::GetParcelStatusCallback callback) {
              CHECK_EQ(parcel_identifiers[0].tracking_id(), tracking_id);
              std::move(callback).Run(succeeded,
                                      BuildParcelStatus(tracking_id, state));
            });
  }

  // Mock the server response for stop tracking requests.
  void MockStopTrackingResponses(bool succeeded) {
    ON_CALL(*this, StopTrackingParcel)
        .WillByDefault([succeeded](const std::string& tracking_id,
                                   StopParcelTrackingCallback callback) {
          std::move(callback).Run(succeeded);
        });
    ON_CALL(*this, StopTrackingParcels)
        .WillByDefault(
            [succeeded](const std::vector<ParcelIdentifier>& parcel_identifiers,
                        StopParcelTrackingCallback callback) {
              std::move(callback).Run(succeeded);
            });
    ON_CALL(*this, StopTrackingAllParcels)
        .WillByDefault([succeeded](StopParcelTrackingCallback callback) {
          std::move(callback).Run(succeeded);
        });
  }

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
};

class MockParcelsStorage : public ParcelsStorage {
 public:
  MockParcelsStorage() : ParcelsStorage(nullptr, nullptr) {}
  MockParcelsStorage(const MockParcelsStorage&) = delete;
  MockParcelsStorage operator=(const MockParcelsStorage&) = delete;
  ~MockParcelsStorage() override = default;

  MOCK_METHOD(void, Init, (OnInitializedCallback callback), (override));
  MOCK_METHOD(
      std::unique_ptr<std::vector<parcel_tracking_db::ParcelTrackingContent>>,
      GetAllParcelTrackingContents,
      (),
      (override));
  MOCK_METHOD(void,
              UpdateParcelStatus,
              (const std::vector<ParcelStatus>& parcel_status,
               StorageUpdateCallback callback),
              (override));
  MOCK_METHOD(void,
              DeleteParcelStatus,
              (const std::string& tracking_id, StorageUpdateCallback callback),
              (override));
  MOCK_METHOD(void,
              DeleteParcelsStatus,
              (const std::vector<ParcelIdentifier>&,
               StorageUpdateCallback callback),
              (override));
  MOCK_METHOD(void,
              DeleteAllParcelStatus,
              (StorageUpdateCallback callback),
              (override));
  MOCK_METHOD(void, ModifyOldDoneParcels, (), (override));

  void MockInitCallback(bool succeeded) {
    ON_CALL(*this, Init)
        .WillByDefault(
            [succeeded](ParcelsStorage::OnInitializedCallback callback) {
              std::move(callback).Run(succeeded);
            });
    if (succeeded) {
      EXPECT_CALL(*this, ModifyOldDoneParcels()).Times(1);
    }
  }

  void MockGetAllParcelTrackingContents(const std::string& tracking_id,
                                        ParcelStatus::ParcelState state) {
    ON_CALL(*this, GetAllParcelTrackingContents)
        .WillByDefault([tracking_id, state]() {
          return BuildParcelTracking(tracking_id, state);
        });
  }
};

class ParcelsManagerTest : public testing::Test {
 public:
  ParcelsManagerTest() = default;
  ~ParcelsManagerTest() override = default;

  void SetUp() override {
    clock_.SetNow(GetTestTime());
    auto mock_server_proxy = std::make_unique<MockServerProxy>();
    auto mock_storage = std::make_unique<MockParcelsStorage>();
    mock_server_proxy_ = mock_server_proxy.get();
    mock_storage_ = mock_storage.get();

    parcels_manager_ = std::make_unique<ParcelsManager>(
        std::move(mock_server_proxy), std::move(mock_storage), &clock_);

    ON_CALL(*mock_storage_, UpdateParcelStatus)
        .WillByDefault([](const std::vector<ParcelStatus>& parcel_status,
                          ParcelsStorage::StorageUpdateCallback callback) {
          std::move(callback).Run(true);
        });
  }

  void TearDown() override {
    mock_server_proxy_ = nullptr;
    mock_storage_ = nullptr;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  raw_ptr<MockServerProxy> mock_server_proxy_;
  raw_ptr<MockParcelsStorage> mock_storage_;
  std::unique_ptr<ParcelsManager> parcels_manager_;
  base::SimpleTestClock clock_;
};

TEST_F(ParcelsManagerTest, TestStartTrackingParcels) {
  EXPECT_CALL(*mock_storage_, Init(_)).Times(1);
  mock_server_proxy_->MockParcelStatusResponses(true, kTestTrackingId,
                                                ParcelStatus::NEW);
  mock_storage_->MockInitCallback(true);

  EXPECT_CALL(*mock_server_proxy_, StartTrackingParcels(_, _, _)).Times(1);
  EXPECT_CALL(*mock_storage_, UpdateParcelStatus(_, _)).Times(1);
  base::RunLoop run_loop;
  parcels_manager_->StartTrackingParcels(
      GetTestIdentifiers(kTestTrackingId), kSoucePageDomain,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool success,
             std::unique_ptr<std::vector<ParcelTrackingStatus>> parcel_status) {
            ASSERT_TRUE(success);
            ASSERT_EQ(1, static_cast<int>(parcel_status->size()));
            auto status = (*parcel_status)[0];
            ASSERT_EQ(kTestTrackingId, status.tracking_id);
            ASSERT_EQ(commerce::ParcelIdentifier::UPS, status.carrier);
            ASSERT_EQ(ParcelStatus::NEW, status.state);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ParcelsManagerTest, TestStartTrackingParcels_ServerError) {
  EXPECT_CALL(*mock_storage_, Init(_)).Times(1);
  mock_server_proxy_->MockParcelStatusResponses(false, kTestTrackingId,
                                                ParcelStatus::NEW);
  mock_storage_->MockInitCallback(true);

  EXPECT_CALL(*mock_server_proxy_, StartTrackingParcels(_, _, _)).Times(1);
  EXPECT_CALL(*mock_storage_, UpdateParcelStatus(_, _)).Times(0);
  base::RunLoop run_loop;
  parcels_manager_->StartTrackingParcels(
      GetTestIdentifiers(kTestTrackingId), kSoucePageDomain,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool success,
             std::unique_ptr<std::vector<ParcelTrackingStatus>> parcel_status) {
            ASSERT_FALSE(success);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ParcelsManagerTest,
       TestGetAllParcelStatuses_LocalStorageHasFreshStatus) {
  EXPECT_CALL(*mock_storage_, Init(_)).Times(1);
  mock_storage_->MockGetAllParcelTrackingContents(kTestTrackingId,
                                                  ParcelStatus::NEW);
  mock_server_proxy_->MockParcelStatusResponses(true, kTestTrackingId,
                                                ParcelStatus::PICKED_UP);
  mock_storage_->MockInitCallback(true);

  EXPECT_CALL(*mock_server_proxy_, GetParcelStatus(_, _)).Times(0);
  EXPECT_CALL(*mock_storage_, GetAllParcelTrackingContents()).Times(1);
  EXPECT_CALL(*mock_storage_, UpdateParcelStatus(_, _)).Times(0);
  base::RunLoop run_loop;
  parcels_manager_->GetAllParcelStatuses(base::BindOnce(
      [](base::RunLoop* run_loop, bool success,
         std::unique_ptr<std::vector<ParcelTrackingStatus>> parcel_status) {
        ASSERT_TRUE(success);
        ASSERT_EQ(1, static_cast<int>(parcel_status->size()));
        auto status = (*parcel_status)[0];
        ASSERT_EQ(kTestTrackingId, status.tracking_id);
        ASSERT_EQ(commerce::ParcelIdentifier::UPS, status.carrier);
        ASSERT_EQ(ParcelStatus::NEW, status.state);
        run_loop->Quit();
      },
      &run_loop));
  run_loop.Run();
}

TEST_F(ParcelsManagerTest,
       TestGetAllParcelStatusesCalledTwice_LocalStorageHasFreshStatus) {
  EXPECT_CALL(*mock_storage_, Init(_)).Times(1);
  mock_storage_->MockGetAllParcelTrackingContents(kTestTrackingId,
                                                  ParcelStatus::NEW);
  mock_server_proxy_->MockParcelStatusResponses(true, kTestTrackingId,
                                                ParcelStatus::PICKED_UP);
  mock_storage_->MockInitCallback(true);

  EXPECT_CALL(*mock_server_proxy_, GetParcelStatus(_, _)).Times(0);
  EXPECT_CALL(*mock_storage_, GetAllParcelTrackingContents()).Times(2);
  EXPECT_CALL(*mock_storage_, UpdateParcelStatus(_, _)).Times(0);
  std::vector<ParcelTrackingStatus> expected;
  ParcelTrackingStatus expected_status;
  expected_status.carrier = commerce::ParcelIdentifier::UPS;
  expected_status.state = ParcelStatus::NEW;
  expected_status.tracking_id = kTestTrackingId;
  expected.emplace_back(expected_status);
  parcels_manager_->GetAllParcelStatuses(
      base::BindOnce(&ExpectGetParcelsCallback, true, expected));
  parcels_manager_->GetAllParcelStatuses(
      base::BindOnce(&ExpectGetParcelsCallback, true, expected));
  task_environment_.RunUntilIdle();
}

TEST_F(ParcelsManagerTest,
       TestGetAllParcelStatuses_LocalStorageHasStaleStatus) {
  EXPECT_CALL(*mock_storage_, Init(_)).Times(1);
  mock_storage_->MockGetAllParcelTrackingContents(kTestTrackingId,
                                                  ParcelStatus::NEW);
  mock_server_proxy_->MockParcelStatusResponses(true, kTestTrackingId,
                                                ParcelStatus::PICKED_UP);
  mock_storage_->MockInitCallback(true);

  std::vector<ParcelTrackingStatus> expected;
  ParcelTrackingStatus expected_status;
  expected_status.carrier = commerce::ParcelIdentifier::UPS;
  expected_status.state = ParcelStatus::NEW;
  expected_status.tracking_id = kTestTrackingId;
  expected.emplace_back(expected_status);

  // Advance the clock by 10 hours, shouldn't trigger server request.
  clock_.Advance(base::Hours(10));
  EXPECT_CALL(*mock_server_proxy_, GetParcelStatus(_, _)).Times(0);
  EXPECT_CALL(*mock_storage_, GetAllParcelTrackingContents()).Times(1);
  EXPECT_CALL(*mock_storage_, UpdateParcelStatus(_, _)).Times(0);
  parcels_manager_->GetAllParcelStatuses(
      base::BindOnce(&ExpectGetParcelsCallback, true, expected));
  task_environment_.RunUntilIdle();

  // Advance clock by another 10 hours, the local state is now stale.
  expected[0].state = ParcelStatus::PICKED_UP;
  clock_.Advance(base::Hours(10));
  EXPECT_CALL(*mock_server_proxy_, GetParcelStatus(_, _)).Times(1);
  EXPECT_CALL(*mock_storage_, GetAllParcelTrackingContents()).Times(1);
  EXPECT_CALL(*mock_storage_, UpdateParcelStatus(_, _)).Times(1);
  parcels_manager_->GetAllParcelStatuses(
      base::BindOnce(&ExpectGetParcelsCallback, true, expected));
  task_environment_.RunUntilIdle();
}

TEST_F(ParcelsManagerTest, TestGetAllParcelStatuses_LocalStorageHasDoneStatus) {
  EXPECT_CALL(*mock_storage_, Init(_)).Times(1);
  mock_storage_->MockGetAllParcelTrackingContents(kTestTrackingId,
                                                  ParcelStatus::FINISHED);
  mock_server_proxy_->MockParcelStatusResponses(true, kTestTrackingId,
                                                ParcelStatus::FINISHED);
  mock_storage_->MockInitCallback(true);

  // Advance clock by 1 day.
  clock_.Advance(base::Days(1));
  EXPECT_CALL(*mock_server_proxy_, GetParcelStatus(_, _)).Times(0);
  EXPECT_CALL(*mock_storage_, GetAllParcelTrackingContents()).Times(1);
  EXPECT_CALL(*mock_storage_, UpdateParcelStatus(_, _)).Times(0);
  base::RunLoop run_loop;
  parcels_manager_->GetAllParcelStatuses(base::BindOnce(
      [](base::RunLoop* run_loop, bool success,
         std::unique_ptr<std::vector<ParcelTrackingStatus>> parcel_status) {
        ASSERT_TRUE(success);
        ASSERT_EQ(1, static_cast<int>(parcel_status->size()));
        auto status = (*parcel_status)[0];
        ASSERT_EQ(kTestTrackingId, status.tracking_id);
        ASSERT_EQ(commerce::ParcelIdentifier::UPS, status.carrier);
        ASSERT_EQ(ParcelStatus::FINISHED, status.state);
        run_loop->Quit();
      },
      &run_loop));
  run_loop.Run();
}

TEST_F(ParcelsManagerTest, TestGetAllParcelStatuses_ServerError) {
  EXPECT_CALL(*mock_storage_, Init(_)).Times(1);
  mock_storage_->MockGetAllParcelTrackingContents(kTestTrackingId,
                                                  ParcelStatus::NEW);
  ON_CALL(*mock_server_proxy_, GetParcelStatus)
      .WillByDefault([](const std::vector<ParcelIdentifier>& parcel_identifiers,
                        ParcelsServerProxy::GetParcelStatusCallback callback) {
        std::move(callback).Run(
            false, std::make_unique<std::vector<commerce::ParcelStatus>>());
      });
  mock_storage_->MockInitCallback(true);

  clock_.Advance(base::Days(1));
  EXPECT_CALL(*mock_server_proxy_, GetParcelStatus(_, _)).Times(1);
  EXPECT_CALL(*mock_storage_, GetAllParcelTrackingContents()).Times(1);
  EXPECT_CALL(*mock_storage_, UpdateParcelStatus(_, _)).Times(0);
  base::RunLoop run_loop;
  parcels_manager_->GetAllParcelStatuses(base::BindOnce(
      [](base::RunLoop* run_loop, bool success,
         std::unique_ptr<std::vector<ParcelTrackingStatus>> parcel_status) {
        ASSERT_TRUE(success);
        ASSERT_EQ(1, static_cast<int>(parcel_status->size()));
        auto status = (*parcel_status)[0];
        ASSERT_EQ(kTestTrackingId, status.tracking_id);
        ASSERT_EQ(commerce::ParcelIdentifier::UPS, status.carrier);
        ASSERT_EQ(ParcelStatus::NEW, status.state);
        run_loop->Quit();
      },
      &run_loop));
  run_loop.Run();
}

TEST_F(ParcelsManagerTest, TestGetAllParcelStatuses_StorageError) {
  EXPECT_CALL(*mock_storage_, Init(_)).Times(1);
  EXPECT_CALL(*mock_storage_, GetAllParcelTrackingContents)
      .WillOnce(Return(
          ByMove(std::make_unique<
                 std::vector<parcel_tracking_db::ParcelTrackingContent>>())));
  mock_storage_->MockInitCallback(false);

  base::RunLoop run_loop;
  parcels_manager_->GetAllParcelStatuses(base::BindOnce(
      [](base::RunLoop* run_loop, bool success,
         std::unique_ptr<std::vector<ParcelTrackingStatus>> parcel_status) {
        ASSERT_FALSE(success);
        ASSERT_EQ(0, static_cast<int>(parcel_status->size()));
        run_loop->Quit();
      },
      &run_loop));
  run_loop.Run();
}

TEST_F(ParcelsManagerTest, TestStopTrackingAllParcels) {
  EXPECT_CALL(*mock_storage_, Init(_)).Times(1);
  mock_storage_->MockInitCallback(true);
  mock_server_proxy_->MockStopTrackingResponses(true);

  EXPECT_CALL(*mock_server_proxy_, StopTrackingAllParcels(_)).Times(1);
  EXPECT_CALL(*mock_storage_, DeleteAllParcelStatus(_)).Times(1);
  base::RunLoop run_loop;
  parcels_manager_->StopTrackingAllParcels(base::BindOnce(
      [](base::RunLoop* run_loop, bool success) {
        ASSERT_TRUE(success);
        run_loop->Quit();
      },
      &run_loop));
  run_loop.Run();
}

TEST_F(ParcelsManagerTest, TestStopTrackingAllParcels_ServerError) {
  EXPECT_CALL(*mock_storage_, Init(_)).Times(1);
  mock_storage_->MockInitCallback(true);
  mock_server_proxy_->MockStopTrackingResponses(false);

  EXPECT_CALL(*mock_server_proxy_, StopTrackingAllParcels(_)).Times(1);
  EXPECT_CALL(*mock_storage_, DeleteAllParcelStatus(_)).Times(0);
  base::RunLoop run_loop;
  parcels_manager_->StopTrackingAllParcels(base::BindOnce(
      [](base::RunLoop* run_loop, bool success) {
        ASSERT_FALSE(success);
        run_loop->Quit();
      },
      &run_loop));
  run_loop.Run();
}

TEST_F(ParcelsManagerTest, TestStopTrackingParcel) {
  EXPECT_CALL(*mock_storage_, Init(_)).Times(1);
  mock_storage_->MockInitCallback(true);
  mock_server_proxy_->MockStopTrackingResponses(true);

  EXPECT_CALL(*mock_server_proxy_, StopTrackingParcel(_, _)).Times(1);
  EXPECT_CALL(*mock_storage_, DeleteParcelStatus(_, _)).Times(1);
  base::RunLoop run_loop;
  parcels_manager_->StopTrackingParcel(
      kTestTrackingId, base::BindOnce(
                           [](base::RunLoop* run_loop, bool success) {
                             ASSERT_TRUE(success);
                             run_loop->Quit();
                           },
                           &run_loop));
  run_loop.Run();
}

TEST_F(ParcelsManagerTest, TestStopTrackingParcel_ServerError) {
  EXPECT_CALL(*mock_storage_, Init(_)).Times(1);
  mock_storage_->MockInitCallback(true);
  mock_server_proxy_->MockStopTrackingResponses(false);

  EXPECT_CALL(*mock_server_proxy_, StopTrackingParcel(_, _)).Times(1);
  EXPECT_CALL(*mock_storage_, DeleteParcelStatus(_, _)).Times(0);
  base::RunLoop run_loop;
  parcels_manager_->StopTrackingParcel(
      kTestTrackingId, base::BindOnce(
                           [](base::RunLoop* run_loop, bool success) {
                             ASSERT_FALSE(success);
                             run_loop->Quit();
                           },
                           &run_loop));
  run_loop.Run();
}

TEST_F(ParcelsManagerTest, TestStopTrackingParcels) {
  EXPECT_CALL(*mock_storage_, Init(_)).Times(1);
  mock_storage_->MockInitCallback(true);
  mock_server_proxy_->MockStopTrackingResponses(true);

  EXPECT_CALL(*mock_server_proxy_, StopTrackingParcels(_, _)).Times(1);
  EXPECT_CALL(*mock_storage_, DeleteParcelsStatus(_, _)).Times(1);
  base::RunLoop run_loop;
  parcels_manager_->StopTrackingParcels(
      GetTestIdentifiers(kTestTrackingId),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool success) {
            ASSERT_TRUE(success);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ParcelsManagerTest, TestStopTrackingParcels_ServerError) {
  EXPECT_CALL(*mock_storage_, Init(_)).Times(1);
  mock_storage_->MockInitCallback(true);
  mock_server_proxy_->MockStopTrackingResponses(false);

  EXPECT_CALL(*mock_server_proxy_, StopTrackingParcels(_, _)).Times(1);
  EXPECT_CALL(*mock_storage_, DeleteParcelsStatus(_, _)).Times(0);
  base::RunLoop run_loop;
  parcels_manager_->StopTrackingParcels(
      GetTestIdentifiers(kTestTrackingId),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool success) {
            ASSERT_FALSE(success);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

}  // namespace
}  // namespace commerce
