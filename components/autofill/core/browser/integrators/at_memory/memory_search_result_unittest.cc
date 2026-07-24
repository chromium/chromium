// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {


TEST(MemorySearchResultTest, ObfuscatesIban) {
  std::u16string raw_iban = u"DE91 1000 0000 0123 4567 89";
  std::u16string obfuscated_value = u"DE\u2006\u2022\u20226789";
  MemorySearchResult result(MemoryDataType::kIban, u"IBAN", obfuscated_value);
  result.is_obfuscated = true;

  EXPECT_EQ(result.type, MemoryDataType::kIban);
  EXPECT_TRUE(result.is_obfuscated);

  EXPECT_EQ(result.value, obfuscated_value);
}

TEST(MemorySearchResultTest, DoesNotObfuscateNonSpiiValue) {
  std::u16string value = u"Some other value";
  MemorySearchResult result(MemoryDataType::kNameFull, u"Name", value);

  EXPECT_EQ(result.type, MemoryDataType::kNameFull);
  EXPECT_FALSE(result.is_obfuscated);
  EXPECT_EQ(result.value, value);
}

}  // namespace autofill
