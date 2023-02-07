// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/renderer/spellcheck_provider_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests for Hunspell functionality in SpellcheckingProvider

using base::ASCIIToUTF16;
using base::WideToUTF16;

namespace {

void CheckSpellingServiceCallCount(size_t actual, size_t expected) {
  // On Windows, if the native spell checker integration is enabled,
  // CallSpellingService() is not used, so the fake provider's |text_| is never
  // assigned. Don't assert the text in that case.
#if !BUILDFLAG(IS_WIN)
  EXPECT_EQ(actual, expected);
#endif  // !BUILDFLAG(IS_WIN)
}

void CheckProviderText(std::u16string expected, std::u16string actual) {
  // On Windows, if the native spell checker integration is enabled,
  // CallSpellingService() is not used, so the fake provider's |text_| is never
  // assigned. Don't assert the text in that case.
#if !BUILDFLAG(IS_WIN)
  EXPECT_EQ(actual, expected);
#endif  // !BUILDFLAG(IS_WIN)
}

// Tests that the SpellCheckProvider object sends a spellcheck request when a
// user finishes typing a word. Also this test verifies that this object checks
// only a line being edited by the user.
TEST_F(SpellCheckProviderTest, MultiLineText) {
  FakeTextCheckingResult completion;

  // Verify that the SpellCheckProvider class does not spellcheck empty text.
  provider_.ResetResult();
  provider_.RequestTextChecking(
      std::u16string(),
      std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(completion.completion_count_, 1U);
  EXPECT_TRUE(provider_.text_.empty());
  CheckSpellingServiceCallCount(provider_.spelling_service_call_count_, 0U);

  // Verify that the SpellCheckProvider class spellcheck the first word when we
  // stop typing after finishing the first word.
  provider_.ResetResult();
  provider_.RequestTextChecking(
      u"First", std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(completion.completion_count_, 2U);
  CheckProviderText(u"First", provider_.text_);
  CheckSpellingServiceCallCount(provider_.spelling_service_call_count_, 1U);

  // Verify that the SpellCheckProvider class spellcheck the first line when we
  // type a return key, i.e. when we finish typing a line.
  provider_.ResetResult();
  provider_.RequestTextChecking(
      u"First Second\n",
      std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(completion.completion_count_, 3U);
  CheckProviderText(u"First Second\n", provider_.text_);
  CheckSpellingServiceCallCount(provider_.spelling_service_call_count_, 2U);

  // Verify that the SpellCheckProvider class spellcheck the lines when we
  // finish typing a word "Third" to the second line.
  provider_.ResetResult();
  provider_.RequestTextChecking(
      u"First Second\nThird ",
      std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(completion.completion_count_, 4U);
  CheckProviderText(u"First Second\nThird ", provider_.text_);
  CheckSpellingServiceCallCount(provider_.spelling_service_call_count_, 3U);

  // Verify that the SpellCheckProvider class does not send a spellcheck request
  // when a user inserts whitespace characters.
  provider_.ResetResult();
  provider_.RequestTextChecking(
      u"First Second\nThird   ",
      std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(completion.completion_count_, 5U);
  EXPECT_TRUE(provider_.text_.empty());
  CheckSpellingServiceCallCount(provider_.spelling_service_call_count_, 3U);

  // Verify that the SpellCheckProvider class spellcheck the lines when we type
  // a period.
  provider_.ResetResult();
  provider_.RequestTextChecking(
      u"First Second\nThird   Fourth.",
      std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(completion.completion_count_, 6U);
  CheckProviderText(u"First Second\nThird   Fourth.", provider_.text_);
  CheckSpellingServiceCallCount(provider_.spelling_service_call_count_, 4U);
}

// Tests that the SpellCheckProvider class does not send requests to the
// spelling service when not necessary.
TEST_F(SpellCheckProviderTest, CancelUnnecessaryRequests) {
  FakeTextCheckingResult completion;
  provider_.RequestTextChecking(
      u"hello.", std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(completion.completion_count_, 1U);
  EXPECT_EQ(completion.cancellation_count_, 0U);
  CheckSpellingServiceCallCount(provider_.spelling_service_call_count_, 1U);

  // Test that the SpellCheckProvider does not send a request with the same text
  // as above.
  provider_.RequestTextChecking(
      u"hello.", std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(completion.completion_count_, 2U);
  EXPECT_EQ(completion.cancellation_count_, 0U);
  CheckSpellingServiceCallCount(provider_.spelling_service_call_count_, 1U);

  // Test that the SpellCheckProvider class cancels an incoming request that
  // does not include any words.
  provider_.RequestTextChecking(
      u":-)", std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(completion.completion_count_, 3U);
  EXPECT_EQ(completion.cancellation_count_, 1U);
  CheckSpellingServiceCallCount(provider_.spelling_service_call_count_, 1U);

  // Test that the SpellCheckProvider class sends a request when it receives a
  // Russian word.
  const char16_t kRussianWord[] = u"\x0431\x0451\x0434\x0440\x0430";
  provider_.RequestTextChecking(
      kRussianWord, std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(completion.completion_count_, 4U);
  EXPECT_EQ(completion.cancellation_count_, 1U);
  CheckSpellingServiceCallCount(provider_.spelling_service_call_count_, 2U);
}

// Tests that the SpellCheckProvider calls didFinishCheckingText() when
// necessary.
TEST_F(SpellCheckProviderTest, CompleteNecessaryRequests) {
  FakeTextCheckingResult completion;

  std::u16string text = u"Icland is an icland ";
  provider_.RequestTextChecking(
      text, std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(0U, completion.cancellation_count_) << "Should finish checking \""
                                                << text << "\"";

  const int kSubstringLength = 18;
  std::u16string substring = text.substr(0, kSubstringLength);
  provider_.RequestTextChecking(
      substring, std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(0U, completion.cancellation_count_) << "Should finish checking \""
                                                << substring << "\"";

  provider_.RequestTextChecking(
      text, std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(0U, completion.cancellation_count_) << "Should finish checking \""
                                                << text << "\"";
}

}  // namespace
