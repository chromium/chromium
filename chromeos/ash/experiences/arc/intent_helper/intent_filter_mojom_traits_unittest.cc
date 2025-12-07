// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/intent_helper/intent_filter_mojom_traits.h"

#include <cstddef>
#include <vector>

#include "chromeos/ash/experiences/arc/intent_helper/intent_constants.h"
#include "chromeos/ash/experiences/arc/intent_helper/intent_filter.h"
#include "chromeos/ash/experiences/arc/mojom/intent_helper.mojom-shared.h"
#include "chromeos/ash/experiences/arc/mojom/intent_helper.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(IntentFilterMojomTraitsTest, RoundTrip) {
  std::vector<arc::IntentFilter::AuthorityEntry> authorities;
  authorities.emplace_back("www.a.com", 0);

  std::vector<arc::IntentFilter::PatternMatcher> patterns;
  patterns.emplace_back("/", arc::PatternType::kPrefix);

  auto input =
      arc::IntentFilter("package_name", "activity_name", "activity_label",
                        {arc::kIntentActionView}, std::move(authorities),
                        std::move(patterns), {"https"}, {"text/plain"});

  arc::IntentFilter output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<arc::mojom::IntentFilter>(
      input, output));

  EXPECT_EQ(input.package_name(), output.package_name());
  EXPECT_EQ(input.activity_name(), output.activity_name());
  EXPECT_EQ(input.activity_label(), output.activity_label());
  EXPECT_EQ(input.actions(), output.actions());
  ASSERT_EQ(input.authorities().size(), output.authorities().size());
  for (size_t i = 0; i < input.authorities().size(); i++) {
    EXPECT_EQ(input.authorities()[i].host(), output.authorities()[i].host());
    EXPECT_EQ(input.authorities()[i].port(), output.authorities()[i].port());
  }
  ASSERT_EQ(input.paths().size(), output.paths().size());
  for (size_t i = 0; i < input.paths().size(); i++) {
    EXPECT_EQ(input.paths()[i].pattern(), output.paths()[i].pattern());
    EXPECT_EQ(input.paths()[i].match_type(), output.paths()[i].match_type());
  }
  EXPECT_EQ(input.schemes(), output.schemes());
  EXPECT_EQ(input.mime_types(), output.mime_types());
}
