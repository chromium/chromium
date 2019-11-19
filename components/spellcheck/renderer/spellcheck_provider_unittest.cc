// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spellcheck_provider_test.h"

#include "base/strings/utf_string_conversions.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_text_checking_result.h"

namespace {

class SpellCheckProviderCacheTest : public SpellCheckProviderTest {
 protected:
  void UpdateCustomDictionary() {
    SpellCheck* spellcheck = provider_.spellcheck();
    EXPECT_NE(spellcheck, nullptr);
    // Skip adding friend class - use public CustomDictionaryChanged from
    // |spellcheck::mojom::SpellChecker|
    static_cast<spellcheck::mojom::SpellChecker*>(spellcheck)
        ->CustomDictionaryChanged({}, {});
  }
};

TEST_F(SpellCheckProviderCacheTest, SubstringWithoutMisspellings) {
  FakeTextCheckingResult result;
  FakeTextCheckingCompletion completion(&result);

  blink::WebVector<blink::WebTextCheckingResult> last_results;
  provider_.SetLastResults(base::ASCIIToUTF16("This is a test"), last_results);
  EXPECT_TRUE(provider_.SatisfyRequestFromCache(base::ASCIIToUTF16("This is a"),
                                                &completion));
  EXPECT_EQ(result.completion_count_, 1U);
}

TEST_F(SpellCheckProviderCacheTest, SubstringWithMisspellings) {
  FakeTextCheckingResult result;
  FakeTextCheckingCompletion completion(&result);

  blink::WebVector<blink::WebTextCheckingResult> last_results;
  std::vector<blink::WebTextCheckingResult> results;
  results.push_back(
      blink::WebTextCheckingResult(blink::kWebTextDecorationTypeSpelling, 5, 3,
                                   std::vector<blink::WebString>({"isq"})));
  last_results.Assign(results);
  provider_.SetLastResults(base::ASCIIToUTF16("This isq a test"), last_results);
  EXPECT_TRUE(provider_.SatisfyRequestFromCache(
      base::ASCIIToUTF16("This isq a"), &completion));
  EXPECT_EQ(result.completion_count_, 1U);
}

TEST_F(SpellCheckProviderCacheTest, ShorterTextNotSubstring) {
  FakeTextCheckingResult result;
  FakeTextCheckingCompletion completion(&result);

  blink::WebVector<blink::WebTextCheckingResult> last_results;
  provider_.SetLastResults(base::ASCIIToUTF16("This is a test"), last_results);
  EXPECT_FALSE(provider_.SatisfyRequestFromCache(
      base::ASCIIToUTF16("That is a"), &completion));
  EXPECT_EQ(result.completion_count_, 0U);
}

TEST_F(SpellCheckProviderCacheTest, ResetCacheOnCustomDictionaryUpdate) {
  FakeTextCheckingResult result;
  FakeTextCheckingCompletion completion(&result);

  blink::WebVector<blink::WebTextCheckingResult> last_results;
  provider_.SetLastResults(base::ASCIIToUTF16("This is a test"), last_results);

  UpdateCustomDictionary();

  EXPECT_FALSE(provider_.SatisfyRequestFromCache(
      base::ASCIIToUTF16("This is a"), &completion));
  EXPECT_EQ(result.completion_count_, 0U);
}

}  // namespace
