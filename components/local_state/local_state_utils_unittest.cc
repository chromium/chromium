// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/local_state/local_state_utils.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(LocalStateUtilsTest, FilterPrefs) {
  std::vector<std::string> prefixes = {"foo", "bar", "baz"};

  std::vector<std::string> invalid_pref_paths = {"fo", "ar", "afoo"};
  std::vector<std::string> valid_pref_paths = {"foo", "foom", "bar.stuff"};

  std::vector<std::string> all_pref_paths = invalid_pref_paths;
  all_pref_paths.insert(all_pref_paths.end(), valid_pref_paths.begin(),
                        valid_pref_paths.end());

  base::Value prefs(base::Value::Type::DICTIONARY);
  for (const std::string& path : all_pref_paths)
    prefs.SetStringPath(path, path + "_value");

  internal::FilterPrefs(prefixes, prefs);

  for (const std::string& invalid_path : invalid_pref_paths)
    EXPECT_FALSE(prefs.FindStringPath(invalid_path));

  for (const std::string& valid_path : valid_pref_paths) {
    const std::string* result = prefs.FindStringPath(valid_path);
    ASSERT_TRUE(result);
    EXPECT_EQ(valid_path + "_value", *result);
  }
}
