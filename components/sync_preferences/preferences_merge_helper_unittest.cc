// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/preferences_merge_helper.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_preferences {
namespace {

TEST(PreferencesMergeHelperTest, MergeListValues) {
  auto local_value =
      base::Value::List().Append("local_value").Append("common_value");
  auto server_value =
      base::Value::List().Append("server_value").Append("common_value");

  auto expected_value = base::Value::List()
                            .Append("server_value")
                            .Append("common_value")
                            .Append("local_value");
  EXPECT_EQ(helper::MergeListValues(local_value, server_value), expected_value);
}

TEST(PreferencesMergeHelperTest, MergeDictionaryValues) {
  auto local_value = base::Value::Dict()
                         .Set("local_key", "local_value")
                         .Set("common_key", "local_value");
  auto server_value = base::Value::Dict()
                          .Set("server_key", "server_value")
                          .Set("common_key", "server_value");

  auto expected_value = base::Value::Dict()
                            .Set("server_key", "server_value")
                            .Set("common_key", "server_value")
                            .Set("local_key", "local_value");
  EXPECT_EQ(helper::MergeDictionaryValues(local_value, server_value),
            expected_value);
}

// Tests for MergePreference() exists in pref_model_associator_unittest.cc.
// TODO(crbug.com/1416479): Move those tests here.

}  // namespace
}  // namespace sync_preferences
