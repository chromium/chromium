// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/addresses/android/supported_countries_cache.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/ui/addresses/android/dropdown_key_value_android.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::ElementsAreArray;
using ::testing::Return;

using MockBuilder = base::MockRepeatingCallback<std::vector<
    DropdownKeyValueAndroid>(std::string_view)>;

// Produces a distinct one-entry list per `key` so tests can tell cached values
// apart without caring what the key represents.
std::vector<DropdownKeyValueAndroid> MakeEntries(std::string_view key) {
  std::vector<DropdownKeyValueAndroid> result;
  result.emplace_back(base::StrCat({"code_", key}), u"name");
  return result;
}

// Repeated lookups for the same locale must reuse the cached list. `Times(1)`
// on the builder is the actual assertion: a second GetForLocale() for the
// same locale must not rebuild.
TEST(SupportedCountriesCacheTest, ReusesCachedResultForRepeatedLocale) {
  MockBuilder builder;
  const std::vector<DropdownKeyValueAndroid> expected = MakeEntries("en-US");
  EXPECT_CALL(builder, Run("en-US"))
      .Times(1)
      .WillOnce(Return(expected));

  SupportedCountriesCache cache(builder.Get());
  EXPECT_THAT(cache.GetForLocale("en-US"), ElementsAreArray(expected));
  EXPECT_THAT(cache.GetForLocale("en-US"), ElementsAreArray(expected));
}

// Distinct locales are cached independently. Each `Times(1)` asserts that its
// locale's list is built exactly once even when the two are interleaved.
TEST(SupportedCountriesCacheTest, CachesEachLocaleSeparately) {
  MockBuilder builder;
  EXPECT_CALL(builder, Run("en-US"))
      .Times(1)
      .WillOnce(Return(MakeEntries("en-US")));
  EXPECT_CALL(builder, Run("fr-FR"))
      .Times(1)
      .WillOnce(Return(MakeEntries("fr-FR")));

  SupportedCountriesCache cache(builder.Get());
  cache.GetForLocale("en-US");
  cache.GetForLocale("fr-FR");
  cache.GetForLocale("en-US");
  cache.GetForLocale("fr-FR");
}

// An empty result must be cached as well; otherwise a locale with no
// resolvable countries would rebuild the empty vector on every call.
// `Times(1)` enforces that the empty result is not treated as a cache miss.
TEST(SupportedCountriesCacheTest, CachesEmptyResult) {
  MockBuilder builder;
  EXPECT_CALL(builder, Run("xx"))
      .Times(1)
      .WillOnce(Return(std::vector<DropdownKeyValueAndroid>{}));

  SupportedCountriesCache cache(builder.Get());
  EXPECT_TRUE(cache.GetForLocale("xx").empty());
  EXPECT_TRUE(cache.GetForLocale("xx").empty());
}

}  // namespace
}  // namespace autofill
