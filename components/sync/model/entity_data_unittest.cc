// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/entity_data.h"

#include "components/sync/base/model_type.h"
#include "components/sync/base/unique_position.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class EntityDataTest : public testing::Test {
 protected:
  EntityDataTest() {}
  ~EntityDataTest() override {}
};

TEST_F(EntityDataTest, IsDeleted) {
  EntityData data;
  EXPECT_TRUE(data.is_deleted());

  AddDefaultFieldValue(BOOKMARKS, &data.specifics);
  EXPECT_FALSE(data.is_deleted());
}

}  // namespace syncer
