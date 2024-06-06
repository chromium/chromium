// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/key_delivery.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/reporting/encryption/encryption_module.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::Return;
using ::testing::WithArg;

namespace reporting {
namespace {

const base::TimeDelta kPeriod = base::Seconds(5);

class KeyDeliveryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    encryption_module_ = EncryptionModule::Create(
        /*renew_encryption_key_period=*/base::Minutes(30));
  }

  void TearDown() override {
    // Let key_delivery destruct on sequence
    task_environment_.RunUntilIdle();
  }

  ::testing::MockFunction<void(UploaderInterface::UploadReason,
                               UploaderInterface::UploaderInterfaceResultCb)>
      async_upload_start_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<EncryptionModuleInterface> encryption_module_;
};

class MockUploader : public UploaderInterface {
 public:
  static std::unique_ptr<MockUploader> Create(
      base::RepeatingClosure complete_cb) {
    auto uploader = std::make_unique<::testing::StrictMock<MockUploader>>();
    EXPECT_CALL(*uploader, ProcessRecord).Times(0);
    EXPECT_CALL(*uploader, ProcessGap).Times(0);
    EXPECT_CALL(*uploader, Completed(Eq(Status::StatusOK())))
        .WillOnce(Invoke([complete_cb]() { complete_cb.Run(); }));
    return uploader;
  }

  MockUploader() = default;
  MockUploader(const MockUploader&) = delete;
  MockUploader& operator=(const MockUploader&) = delete;

  MOCK_METHOD(void,
              ProcessRecord,
              (EncryptedRecord record,
               ScopedReservation scoped_reservation,
               base::OnceCallback<void(bool)> processed_cb),
              (override));

  MOCK_METHOD(void,
              ProcessGap,
              (SequenceInformation start,
               uint64_t count,
               base::OnceCallback<void(bool)> processed_cb),
              (override));

  MOCK_METHOD(void, Completed, (Status final_status), (override));
};

TEST_F(KeyDeliveryTest, DeliveryOnRequest) {
  auto key_delivery = KeyDelivery::Create(
      kPeriod, encryption_module_,
      base::BindRepeating(
          &::testing::MockFunction<void(
              UploaderInterface::UploadReason,
              UploaderInterface::UploaderInterfaceResultCb)>::Call,
          base::Unretained(&async_upload_start_)));

  EXPECT_CALL(async_upload_start_,
              Call(Eq(UploaderInterface::UploadReason::KEY_DELIVERY), _))
      .WillOnce(WithArg<1>(Invoke(
          [&key_delivery](UploaderInterface::UploaderInterfaceResultCb cb) {
            auto uploader = MockUploader::Create(base::BindRepeating(
                &KeyDelivery::OnKeyUpdateResult,
                base::Unretained(key_delivery.get()), Status::StatusOK()));
            std::move(cb).Run(std::move(uploader));
          })));

  test::TestEvent<Status> key_event;
  key_delivery->Request(key_event.cb());
  EXPECT_OK(key_event.result());
}

TEST_F(KeyDeliveryTest, FailedDeliveryOnRequest) {
  auto key_delivery = KeyDelivery::Create(
      kPeriod, encryption_module_,
      base::BindRepeating(
          &::testing::MockFunction<void(
              UploaderInterface::UploadReason,
              UploaderInterface::UploaderInterfaceResultCb)>::Call,
          base::Unretained(&async_upload_start_)));

  EXPECT_CALL(async_upload_start_,
              Call(Eq(UploaderInterface::UploadReason::KEY_DELIVERY), _))
      .WillOnce(WithArg<1>(Invoke(
          [&key_delivery](UploaderInterface::UploaderInterfaceResultCb cb) {
            auto uploader = MockUploader::Create(
                base::BindRepeating(&KeyDelivery::OnKeyUpdateResult,
                                    base::Unretained(key_delivery.get()),
                                    Status{error::CANCELLED, "For testing"}));
            std::move(cb).Run(std::move(uploader));
          })));

  test::TestEvent<Status> key_event;
  key_delivery->Request(key_event.cb());
  EXPECT_THAT(key_event.result(),
              Property(&Status::error_code, Eq(error::CANCELLED)));
}

TEST_F(KeyDeliveryTest, PeriodicDelivery) {
  auto key_delivery = KeyDelivery::Create(
      kPeriod, encryption_module_,
      base::BindRepeating(
          &::testing::MockFunction<void(
              UploaderInterface::UploadReason,
              UploaderInterface::UploaderInterfaceResultCb)>::Call,
          base::Unretained(&async_upload_start_)));

  EXPECT_CALL(async_upload_start_,
              Call(Eq(UploaderInterface::UploadReason::KEY_DELIVERY), _))
      .WillOnce(WithArg<1>(Invoke(
          [&key_delivery](UploaderInterface::UploaderInterfaceResultCb cb) {
            auto uploader = MockUploader::Create(
                base::BindRepeating(&KeyDelivery::OnKeyUpdateResult,
                                    base::Unretained(key_delivery.get()),
                                    Status{error::CANCELLED, "For testing"}));
            std::move(cb).Run(std::move(uploader));
          })))
      .WillOnce(WithArg<1>(Invoke(
          [&key_delivery](UploaderInterface::UploaderInterfaceResultCb cb) {
            auto uploader = MockUploader::Create(base::BindRepeating(
                &KeyDelivery::OnKeyUpdateResult,
                base::Unretained(key_delivery.get()), Status::StatusOK()));
            std::move(cb).Run(std::move(uploader));
          })));
  // Start periodic updates, like `Storage` does when key is found.
  key_delivery->StartPeriodicKeyUpdate();

  task_environment_.FastForwardBy(2 * kPeriod);
}

TEST_F(KeyDeliveryTest, ImplicitPeriodicDelivery) {
  auto key_delivery = KeyDelivery::Create(
      kPeriod, encryption_module_,
      base::BindRepeating(
          &::testing::MockFunction<void(
              UploaderInterface::UploadReason,
              UploaderInterface::UploaderInterfaceResultCb)>::Call,
          base::Unretained(&async_upload_start_)));

  EXPECT_CALL(async_upload_start_,
              Call(Eq(UploaderInterface::UploadReason::KEY_DELIVERY), _))
      .WillOnce(WithArg<1>(Invoke(
          [&key_delivery](UploaderInterface::UploaderInterfaceResultCb cb) {
            auto uploader = MockUploader::Create(base::BindRepeating(
                &KeyDelivery::OnKeyUpdateResult,
                base::Unretained(key_delivery.get()), Status::StatusOK()));
            std::move(cb).Run(std::move(uploader));
          })))
      .WillOnce(WithArg<1>(Invoke(
          [&key_delivery](UploaderInterface::UploaderInterfaceResultCb cb) {
            auto uploader = MockUploader::Create(
                base::BindRepeating(&KeyDelivery::OnKeyUpdateResult,
                                    base::Unretained(key_delivery.get()),
                                    Status{error::CANCELLED, "For testing"}));
            std::move(cb).Run(std::move(uploader));
          })))
      .WillOnce(WithArg<1>(Invoke(
          [&key_delivery](UploaderInterface::UploaderInterfaceResultCb cb) {
            auto uploader = MockUploader::Create(base::BindRepeating(
                &KeyDelivery::OnKeyUpdateResult,
                base::Unretained(key_delivery.get()), Status::StatusOK()));
            std::move(cb).Run(std::move(uploader));
          })));

  // Request key and start periodic updates, like `Storage` does when key is not
  // found.
  test::TestEvent<Status> key_event;
  key_delivery->Request(key_event.cb());
  EXPECT_OK(key_event.result());

  task_environment_.FastForwardBy(2 * kPeriod);
}

TEST_F(KeyDeliveryTest, ExpirationWhileRequestsPending) {
  auto key_delivery = KeyDelivery::Create(
      kPeriod, encryption_module_,
      base::BindRepeating(
          &::testing::MockFunction<void(
              UploaderInterface::UploadReason,
              UploaderInterface::UploaderInterfaceResultCb)>::Call,
          base::Unretained(&async_upload_start_)));

  EXPECT_CALL(async_upload_start_,
              Call(Eq(UploaderInterface::UploadReason::KEY_DELIVERY), _))
      .Times(1);

  // Request key and discard `key_delivery`.
  test::TestEvent<Status> key_event;
  key_delivery->Request(key_event.cb());
  key_delivery.reset();
  EXPECT_THAT(key_event.result(),
              Property(&Status::error_code, Eq(error::UNAVAILABLE)));
}
}  // namespace
}  // namespace reporting
