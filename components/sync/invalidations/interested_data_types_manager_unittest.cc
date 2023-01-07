// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/invalidations/interested_data_types_manager.h"

#include "base/callback_helpers.h"
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
              (base::RepeatingCallback<void(const ModelTypeSet&)> callback),
              (override));
};

class InterestedDataTypesManagerTest : public testing::Test {
 public:
  InterestedDataTypesManagerTest() {
    manager_.SetInterestedDataTypesHandler(&handler_);
  }

  ~InterestedDataTypesManagerTest() override {
    manager_.SetInterestedDataTypesHandler(nullptr);
  }

 protected:
  testing::NiceMock<MockDataTypesHandler> handler_;
  InterestedDataTypesManager manager_;
};

TEST_F(InterestedDataTypesManagerTest, ShouldReturnGivenDataTypes) {
  manager_.SetInterestedDataTypes(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, PREFERENCES),
            manager_.GetInterestedDataTypes());
  manager_.SetInterestedDataTypes(ModelTypeSet(PREFERENCES, PASSWORDS));
  EXPECT_EQ(ModelTypeSet(PREFERENCES, PASSWORDS),
            manager_.GetInterestedDataTypes());
}

TEST_F(InterestedDataTypesManagerTest, ShouldNotifyOnChange) {
  EXPECT_CALL(handler_, OnInterestedDataTypesChanged);
  manager_.SetInterestedDataTypes(ModelTypeSet(PASSWORDS, AUTOFILL));
}

TEST_F(InterestedDataTypesManagerTest,
       ShouldInitializeOnFirstSetInterestedDataTypes) {
  EXPECT_FALSE(manager_.GetInterestedDataTypes().has_value());
  manager_.SetInterestedDataTypes(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_TRUE(manager_.GetInterestedDataTypes().has_value());
  manager_.SetInterestedDataTypes(ModelTypeSet(BOOKMARKS, PREFERENCES, NIGORI));
  EXPECT_TRUE(manager_.GetInterestedDataTypes().has_value());
}

}  // namespace
}  // namespace syncer
