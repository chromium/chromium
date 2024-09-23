// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/invalidations/sync_invalidations_service_impl.h"

#include "base/functional/callback_helpers.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/sync/invalidations/interested_data_types_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace syncer {
namespace {

class MockDataTypesHandler : public InterestedDataTypesHandler {
 public:
  MOCK_METHOD(void, OnInterestedDataTypesChanged, (), (override));
  MOCK_METHOD(void,
              SetCommittedAdditionalInterestedDataTypesCallback,
              (base::RepeatingCallback<void(const DataTypeSet&)> callback),
              (override));
};

class SyncInvalidationsServiceImplTest : public testing::Test {
 public:
  SyncInvalidationsServiceImplTest()
      : sync_invalidations_service_impl_(&fake_gcm_driver_,
                                         /*instance_id_driver=*/
                                         nullptr) {
    sync_invalidations_service_impl_.SetInterestedDataTypesHandler(&handler_);
  }

  ~SyncInvalidationsServiceImplTest() override {
    sync_invalidations_service_impl_.SetInterestedDataTypesHandler(nullptr);
  }

 protected:
  testing::NiceMock<MockDataTypesHandler> handler_;
  gcm::FakeGCMDriver fake_gcm_driver_;
  SyncInvalidationsServiceImpl sync_invalidations_service_impl_;
};

TEST_F(SyncInvalidationsServiceImplTest, ShouldReturnGivenDataTypes) {
  sync_invalidations_service_impl_.SetInterestedDataTypes(
      {BOOKMARKS, PREFERENCES});
  EXPECT_EQ(DataTypeSet({BOOKMARKS, PREFERENCES}),
            sync_invalidations_service_impl_.GetInterestedDataTypes());
  sync_invalidations_service_impl_.SetInterestedDataTypes(
      {PREFERENCES, PASSWORDS});
  EXPECT_EQ(DataTypeSet({PREFERENCES, PASSWORDS}),
            sync_invalidations_service_impl_.GetInterestedDataTypes());
}

TEST_F(SyncInvalidationsServiceImplTest, ShouldNotifyOnChange) {
  EXPECT_CALL(handler_, OnInterestedDataTypesChanged);
  sync_invalidations_service_impl_.SetInterestedDataTypes(
      {PASSWORDS, AUTOFILL});
}

TEST_F(SyncInvalidationsServiceImplTest,
       ShouldInitializeOnFirstSetInterestedDataTypes) {
  EXPECT_FALSE(
      sync_invalidations_service_impl_.GetInterestedDataTypes().has_value());
  sync_invalidations_service_impl_.SetInterestedDataTypes(
      {BOOKMARKS, PREFERENCES});
  EXPECT_TRUE(
      sync_invalidations_service_impl_.GetInterestedDataTypes().has_value());
  sync_invalidations_service_impl_.SetInterestedDataTypes(
      {BOOKMARKS, PREFERENCES, NIGORI});
  EXPECT_TRUE(
      sync_invalidations_service_impl_.GetInterestedDataTypes().has_value());
}

}  // namespace
}  // namespace syncer
