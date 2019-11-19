// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "components/spellcheck/renderer/spellcheck_provider_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests for Hunspell functionality in SpellcheckingProvider

using base::ASCIIToUTF16;
using base::WideToUTF16;

namespace {

// Tests that the SpellCheckProvider object sends a spellcheck request when a
// user finishes typing a word. Also this test verifies that this object checks
// only a line being edited by the user.
TEST_F(SpellCheckProviderTest, MultiLineText) {
  FakeTextCheckingResult completion;

  // Verify that the SpellCheckProvider class does not spellcheck empty text.
  provider_.ResetResult();
  provider_.RequestTextChecking(
      base::string16(),
      std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_TRUE(provider_.text_.empty());
  EXPECT_EQ(provider_.spelling_service_call_count_, 0U);

  // Verify that the SpellCheckProvider class spellcheck the first word when we
  // stop typing after finishing the first word.
  provider_.ResetResult();
  provider_.RequestTextChecking(
      ASCIIToUTF16("First"),
      std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(ASCIIToUTF16("First"), provider_.text_);
  EXPECT_EQ(provider_.spelling_service_call_count_, 1U);

  // Verify that the SpellCheckProvider class spellcheck the first line when we
  // type a return key, i.e. when we finish typing a line.
  provider_.ResetResult();
  provider_.RequestTextChecking(
      ASCIIToUTF16("First Second\n"),
      std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(ASCIIToUTF16("First Second\n"), provider_.text_);
  EXPECT_EQ(provider_.spelling_service_call_count_, 2U);

  // Verify that the SpellCheckProvider class spellcheck the lines when we
  // finish typing a word "Third" to the second line.
  provider_.ResetResult();
  provider_.RequestTextChecking(
      ASCIIToUTF16("First Second\nThird "),
      std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(ASCIIToUTF16("First Second\nThird "), provider_.text_);
  EXPECT_EQ(provider_.spelling_service_call_count_, 3U);

  // Verify that the SpellCheckProvider class does not send a spellcheck request
  // when a user inserts whitespace characters.
  provider_.ResetResult();
  provider_.RequestTextChecking(
      ASCIIToUTF16("First Second\nThird   "),
      std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_TRUE(provider_.text_.empty());
  EXPECT_EQ(provider_.spelling_service_call_count_, 3U);

  // Verify that the SpellCheckProvider class spellcheck the lines when we type
  // a period.
  provider_.ResetResult();
  provider_.RequestTextChecking(
      ASCIIToUTF16("First Second\nThird   Fourth."),
      std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(ASCIIToUTF16("First Second\nThird   Fourth."), provider_.text_);
  EXPECT_EQ(provider_.spelling_service_call_count_, 4U);
}

// Tests that the SpellCheckProvider class does not send requests to the
// spelling service when not necessary.
TEST_F(SpellCheckProviderTest, CancelUnnecessaryRequests) {
  FakeTextCheckingResult completion;
  provider_.RequestTextChecking(
      ASCIIToUTF16("hello."),
      std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(completion.completion_count_, 1U);
  EXPECT_EQ(completion.cancellation_count_, 0U);
  EXPECT_EQ(provider_.spelling_service_call_count_, 1U);

  // Test that the SpellCheckProvider does not send a request with the same text
  // as above.
  provider_.RequestTextChecking(
      ASCIIToUTF16("hello."),
      std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(completion.completion_count_, 2U);
  EXPECT_EQ(completion.cancellation_count_, 0U);
  EXPECT_EQ(provider_.spelling_service_call_count_, 1U);

  // Test that the SpellCheckProvider class cancels an incoming request that
  // does not include any words.
  provider_.RequestTextChecking(
      ASCIIToUTF16(":-)"),
      std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(completion.completion_count_, 3U);
  EXPECT_EQ(completion.cancellation_count_, 1U);
  EXPECT_EQ(provider_.spelling_service_call_count_, 1U);

  // Test that the SpellCheckProvider class sends a request when it receives a
  // Russian word.
  const wchar_t kRussianWord[] = L"\x0431\x0451\x0434\x0440\x0430";
  provider_.RequestTextChecking(
      WideToUTF16(kRussianWord),
      std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(completion.completion_count_, 4U);
  EXPECT_EQ(completion.cancellation_count_, 1U);
  EXPECT_EQ(provider_.spelling_service_call_count_, 2U);
}

// Tests that the SpellCheckProvider calls didFinishCheckingText() when
// necessary.
TEST_F(SpellCheckProviderTest, CompleteNecessaryRequests) {
  FakeTextCheckingResult completion;

  base::string16 text = ASCIIToUTF16("Icland is an icland ");
  provider_.RequestTextChecking(
      text, std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(0U, completion.cancellation_count_) << "Should finish checking \""
                                                << text << "\"";

  const int kSubstringLength = 18;
  base::string16 substring = text.substr(0, kSubstringLength);
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
