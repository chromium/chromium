// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/ruleset_version.h"

#include <string>

#include "components/prefs/testing_pref_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

class IndexedRulesetVersionTest : public testing::Test {
 public:
  TestingPrefServiceSimple* prefs() { return &pref_service_; }

 private:
  TestingPrefServiceSimple pref_service_;
};

// Test that the filter_tag can be specified to identify the type of filter
// and pref names.
TEST_F(IndexedRulesetVersionTest, ArbitraryFilterTag) {
  IndexedRulesetVersion::RegisterPrefs(prefs()->registry(), "test_tag");

  IndexedRulesetVersion version_1("test", 27, "test_tag");
  version_1.checksum = 345;
  version_1.SaveToPrefs(prefs());

  // Check that prefs with "test_tag" names have the expected values.
  EXPECT_EQ(prefs()->GetString("test_tag.ruleset_version.content"), "test");
  EXPECT_EQ(prefs()->GetInteger("test_tag.ruleset_version.format"), 27);
  EXPECT_EQ(prefs()->GetInteger("test_tag.ruleset_version.checksum"), 345);

  IndexedRulesetVersion version_2("test_tag");
  version_2.ReadFromPrefs(prefs());

  // Check that a new version object can read the correct pref names.
  EXPECT_EQ(version_2.content_version, "test");
  EXPECT_EQ(version_2.format_version, 27);
  EXPECT_EQ(version_2.checksum, 345);
}

}  // namespace subresource_filter
