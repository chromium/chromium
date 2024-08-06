// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/entity_data.h"

#include "components/sync/base/data_type.h"
#include "components/sync/base/unique_position.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class EntityDataTest : public testing::Test {
 protected:
  EntityDataTest() = default;
  ~EntityDataTest() override = default;
};

TEST_F(EntityDataTest, IsDeleted) {
  EntityData data;
  EXPECT_TRUE(data.is_deleted());

  AddDefaultFieldValue(BOOKMARKS, &data.specifics);
  EXPECT_FALSE(data.is_deleted());
}

}  // namespace syncer
