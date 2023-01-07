// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_value_map.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

TEST(PrefValueMapTest, SetValue) {
  PrefValueMap map;
  const Value* result = nullptr;
  EXPECT_FALSE(map.GetValue("key", &result));
  EXPECT_FALSE(result);

  EXPECT_TRUE(map.SetValue("key", Value("test")));
  EXPECT_FALSE(map.SetValue("key", Value("test")));
  EXPECT_TRUE(map.SetValue("key", Value("hi mom!")));

  EXPECT_TRUE(map.GetValue("key", &result));
  EXPECT_EQ("hi mom!", *result);
}

TEST(PrefValueMapTest, GetAndSetIntegerValue) {
  PrefValueMap map;
  ASSERT_TRUE(map.SetValue("key", Value(5)));

  int int_value = 0;
  EXPECT_TRUE(map.GetInteger("key", &int_value));
  EXPECT_EQ(5, int_value);

  map.SetInteger("key", -14);
  EXPECT_TRUE(map.GetInteger("key", &int_value));
  EXPECT_EQ(-14, int_value);
}

TEST(PrefValueMapTest, SetDoubleValue) {
  PrefValueMap map;
  ASSERT_TRUE(map.SetValue("key", Value(5.5)));

  const Value* result = nullptr;
  ASSERT_TRUE(map.GetValue("key", &result));
  EXPECT_DOUBLE_EQ(5.5, result->GetDouble());
}

TEST(PrefValueMapTest, RemoveValue) {
  PrefValueMap map;
  EXPECT_FALSE(map.RemoveValue("key"));

  EXPECT_TRUE(map.SetValue("key", Value("test")));
  EXPECT_TRUE(map.GetValue("key", nullptr));

  EXPECT_TRUE(map.RemoveValue("key"));
  EXPECT_FALSE(map.GetValue("key", nullptr));

  EXPECT_FALSE(map.RemoveValue("key"));
}

TEST(PrefValueMapTest, Clear) {
  PrefValueMap map;
  EXPECT_TRUE(map.SetValue("key", Value("test")));
  EXPECT_TRUE(map.GetValue("key", nullptr));

  map.Clear();

  EXPECT_FALSE(map.GetValue("key", nullptr));
}

TEST(PrefValueMapTest, ClearWithPrefix) {
  {
    PrefValueMap map;
    EXPECT_TRUE(map.SetValue("a", Value("test")));
    EXPECT_TRUE(map.SetValue("b", Value("test")));
    EXPECT_TRUE(map.SetValue("bb", Value("test")));
    EXPECT_TRUE(map.SetValue("z", Value("test")));

    map.ClearWithPrefix("b");

    EXPECT_TRUE(map.GetValue("a", nullptr));
    EXPECT_FALSE(map.GetValue("b", nullptr));
    EXPECT_FALSE(map.GetValue("bb", nullptr));
    EXPECT_TRUE(map.GetValue("z", nullptr));
  }
  {
    PrefValueMap map;
    EXPECT_TRUE(map.SetValue("a", Value("test")));
    EXPECT_TRUE(map.SetValue("b", Value("test")));
    EXPECT_TRUE(map.SetValue("bb", Value("test")));
    EXPECT_TRUE(map.SetValue("z", Value("test")));

    map.ClearWithPrefix("z");

    EXPECT_TRUE(map.GetValue("a", nullptr));
    EXPECT_TRUE(map.GetValue("b", nullptr));
    EXPECT_TRUE(map.GetValue("bb", nullptr));
    EXPECT_FALSE(map.GetValue("z", nullptr));
  }
  {
    PrefValueMap map;
    EXPECT_TRUE(map.SetValue("a", Value("test")));
    EXPECT_TRUE(map.SetValue("b", Value("test")));
    EXPECT_TRUE(map.SetValue("bb", Value("test")));
    EXPECT_TRUE(map.SetValue("z", Value("test")));

    map.ClearWithPrefix("c");

    EXPECT_TRUE(map.GetValue("a", nullptr));
    EXPECT_TRUE(map.GetValue("b", nullptr));
    EXPECT_TRUE(map.GetValue("bb", nullptr));
    EXPECT_TRUE(map.GetValue("z", nullptr));
  }
}

TEST(PrefValueMapTest, GetDifferingKeys) {
  PrefValueMap reference;
  EXPECT_TRUE(reference.SetValue("b", Value("test")));
  EXPECT_TRUE(reference.SetValue("c", Value("test")));
  EXPECT_TRUE(reference.SetValue("e", Value("test")));

  PrefValueMap check;
  std::vector<std::string> differing_paths;
  std::vector<std::string> expected_differing_paths;

  reference.GetDifferingKeys(&check, &differing_paths);
  expected_differing_paths.push_back("b");
  expected_differing_paths.push_back("c");
  expected_differing_paths.push_back("e");
  EXPECT_EQ(expected_differing_paths, differing_paths);

  EXPECT_TRUE(check.SetValue("a", Value("test")));
  EXPECT_TRUE(check.SetValue("c", Value("test")));
  EXPECT_TRUE(check.SetValue("d", Value("test")));

  reference.GetDifferingKeys(&check, &differing_paths);
  expected_differing_paths.clear();
  expected_differing_paths.push_back("a");
  expected_differing_paths.push_back("b");
  expected_differing_paths.push_back("d");
  expected_differing_paths.push_back("e");
  EXPECT_EQ(expected_differing_paths, differing_paths);
}

TEST(PrefValueMapTest, SwapTwoMaps) {
  PrefValueMap first_map;
  EXPECT_TRUE(first_map.SetValue("a", Value("test")));
  EXPECT_TRUE(first_map.SetValue("b", Value("test")));
  EXPECT_TRUE(first_map.SetValue("c", Value("test")));

  PrefValueMap second_map;
  EXPECT_TRUE(second_map.SetValue("d", Value("test")));
  EXPECT_TRUE(second_map.SetValue("e", Value("test")));
  EXPECT_TRUE(second_map.SetValue("f", Value("test")));

  first_map.Swap(&second_map);

  EXPECT_TRUE(first_map.GetValue("d", nullptr));
  EXPECT_TRUE(first_map.GetValue("e", nullptr));
  EXPECT_TRUE(first_map.GetValue("f", nullptr));

  EXPECT_TRUE(second_map.GetValue("a", nullptr));
  EXPECT_TRUE(second_map.GetValue("b", nullptr));
  EXPECT_TRUE(second_map.GetValue("c", nullptr));
}

}  // namespace
}  // namespace base
