// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/invalidations/interested_data_types_manager.h"

#include "base/bind_helpers.h"
#include "components/sync/invalidations/interested_data_types_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace syncer {
namespace {

class MockDataTypesHandler : public InterestedDataTypesHandler {
 public:
  MOCK_METHOD1(OnInterestedDataTypesChanged, void(base::OnceClosure callback));
};

class InterestedDataTypesManagerTest : public testing::Test {
 protected:
  InterestedDataTypesManager manager_;
};

TEST_F(InterestedDataTypesManagerTest, ShouldReturnGivenDataTypes) {
  manager_.SetInterestedDataTypes(ModelTypeSet(BOOKMARKS, PREFERENCES),
                                  base::DoNothing());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, PREFERENCES),
            manager_.GetInterestedDataTypes());
  manager_.SetInterestedDataTypes(ModelTypeSet(PREFERENCES, PASSWORDS),
                                  base::DoNothing());
  EXPECT_EQ(ModelTypeSet(PREFERENCES, PASSWORDS),
            manager_.GetInterestedDataTypes());
}

TEST_F(InterestedDataTypesManagerTest, ShouldNotifyOnChange) {
  testing::NiceMock<MockDataTypesHandler> handler;
  manager_.SetInterestedDataTypesHandler(&handler);
  EXPECT_CALL(handler, OnInterestedDataTypesChanged(_));
  manager_.SetInterestedDataTypes(ModelTypeSet(PASSWORDS, AUTOFILL),
                                  base::DoNothing());
  manager_.SetInterestedDataTypesHandler(nullptr);
}

}  // namespace
}  // namespace syncer
