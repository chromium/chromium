// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/invalidations/subscribed_data_types_manager.h"

#include "components/sync/invalidations/subscribed_data_types_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class MockDataTypesObserver : public SubscribedDataTypesObserver {
 public:
  MOCK_METHOD0(OnSubscribedDataTypesChanged, void());
};

class SubscribedDataTypesManagerTest : public testing::Test {
 protected:
  SubscribedDataTypesManager manager_;
};

TEST_F(SubscribedDataTypesManagerTest, ShouldReturnGivenDataTypes) {
  manager_.SetSubscribedDataTypes(ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, PREFERENCES),
            manager_.GetSubscribedDataTypes());
  manager_.SetSubscribedDataTypes(ModelTypeSet(PREFERENCES, PASSWORDS));
  EXPECT_EQ(ModelTypeSet(PREFERENCES, PASSWORDS),
            manager_.GetSubscribedDataTypes());
}

TEST_F(SubscribedDataTypesManagerTest, ShouldNotifyOnChange) {
  testing::NiceMock<MockDataTypesObserver> observer;
  manager_.AddSubscribedDataTypesObserver(&observer);
  EXPECT_CALL(observer, OnSubscribedDataTypesChanged());
  manager_.SetSubscribedDataTypes(ModelTypeSet(PASSWORDS, AUTOFILL));
  manager_.RemoveSubscribedDataTypesObserver(&observer);
}

}  // namespace
}  // namespace syncer
