// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

TEST(MemorySearchResultTest, ObfuscatesIban) {
  std::u16string raw_iban = u"DE91 1000 0000 0123 4567 89";
  std::u16string obfuscated_value = u"DE\u2006\u2022\u20226789";
  MemorySearchResult result(EntryType::kIban, u"IBAN", obfuscated_value);
  result.is_obfuscated = true;

  EXPECT_EQ(result.type, EntryType::kIban);
  EXPECT_TRUE(result.is_obfuscated);

  EXPECT_EQ(result.value, obfuscated_value);
}

TEST(MemorySearchResultTest, DoesNotObfuscateNonSpiiValue) {
  std::u16string value = u"Some other value";
  MemorySearchResult result(EntryType::kNameFull, u"Name", value);

  EXPECT_EQ(result.type, EntryType::kNameFull);
  EXPECT_FALSE(result.is_obfuscated);
  EXPECT_EQ(result.value, value);
}

}  // namespace accessibility_annotator
