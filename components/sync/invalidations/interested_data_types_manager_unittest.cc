// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/invalidations/interested_data_types_manager.h"

#include "components/sync/invalidations/interested_data_types_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class MockDataTypesObserver : public InterestedDataTypesObserver {
 public:
  MOCK_METHOD0(OnInterestedDataTypesChanged, void());
};

class InterestedDataTypesManagerTest : public testing::Test {
 protected:
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
  testing::NiceMock<MockDataTypesObserver> observer;
  manager_.AddInterestedDataTypesObserver(&observer);
  EXPECT_CALL(observer, OnInterestedDataTypesChanged());
  manager_.SetInterestedDataTypes(ModelTypeSet(PASSWORDS, AUTOFILL));
  manager_.RemoveInterestedDataTypesObserver(&observer);
}

}  // namespace
}  // namespace syncer
