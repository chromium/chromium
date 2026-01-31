// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/renderer/spellcheck_provider_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_text_check_client.h"

namespace {

class SpellCheckProviderMacTest : public SpellCheckProviderTest {};

TEST_F(SpellCheckProviderMacTest, SingleRoundtripSuccess) {
  FakeTextCheckingResult completion;

  provider_.RequestTextChecking(
      u"helo worldd",
      /*spelling_markers=*/
      {spellcheck::SpellingMarker(/*start=*/0, /*end=*/4,
                                  spellcheck::Decoration::SPELLING),
       spellcheck::SpellingMarker(/*start=*/5, /*end=*/11,
                                  spellcheck::Decoration::GRAMMAR)},
      blink::WebTextCheckClient::ShouldForceRefreshTextCheckService::kNo,
      std::make_unique<FakeTextCheckingCompletion>(&completion));
  EXPECT_EQ(completion.completion_count_, 0U);
  EXPECT_EQ(provider_.text_check_requests_.size(), 1U);
  EXPECT_EQ(provider_.pending_text_request_size(), 1U);

  const auto& text = std::get<0>(provider_.text_check_requests_.back());
  const auto& spelling_markers =
      std::get<1>(provider_.text_check_requests_.back());
  auto& callback = std::get<2>(provider_.text_check_requests_.back());
  EXPECT_EQ(text, u"helo worldd");
  EXPECT_EQ(spelling_markers.size(), 2U);
  EXPECT_EQ(spelling_markers[0],
            spellcheck::SpellingMarker(/*start=*/0, /*end=*/4,
                                       spellcheck::Decoration::SPELLING));
  EXPECT_EQ(spelling_markers[1],
            spellcheck::SpellingMarker(/*start=*/5, /*end=*/11,
                                       spellcheck::Decoration::GRAMMAR));
  EXPECT_TRUE(callback);

  std::vector<SpellCheckResult> fake_results;
  std::move(callback).Run(fake_results);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(completion.completion_count_, 1U);
  EXPECT_EQ(provider_.pending_text_request_size(), 0U);

  provider_.text_check_requests_.clear();
}

TEST_F(SpellCheckProviderMacTest, TwoRoundtripSuccess) {
  FakeTextCheckingResult completion1;
  provider_.RequestTextChecking(
      u"hello ", /*spelling_markers=*/{},
      blink::WebTextCheckClient::ShouldForceRefreshTextCheckService::kNo,
      std::make_unique<FakeTextCheckingCompletion>(&completion1));
  FakeTextCheckingResult completion2;
  provider_.RequestTextChecking(
      u"byee ", /*spelling_markers=*/
      {spellcheck::SpellingMarker(/*start=*/0, /*end=*/4,
                                  spellcheck::Decoration::SPELLING)},
      blink::WebTextCheckClient::ShouldForceRefreshTextCheckService::kNo,
      std::make_unique<FakeTextCheckingCompletion>(&completion2));

  EXPECT_EQ(completion1.completion_count_, 0U);
  EXPECT_EQ(completion2.completion_count_, 0U);
  EXPECT_EQ(provider_.text_check_requests_.size(), 2U);
  EXPECT_EQ(provider_.pending_text_request_size(), 2U);

  const auto& text1 = std::get<0>(provider_.text_check_requests_[0]);
  const auto& spelling_markers1 =
      std::get<1>(provider_.text_check_requests_[0]);
  auto& callback1 = std::get<2>(provider_.text_check_requests_[0]);
  EXPECT_EQ(text1, u"hello ");
  EXPECT_EQ(spelling_markers1.size(), 0U);
  EXPECT_TRUE(callback1);

  const auto& text2 = std::get<0>(provider_.text_check_requests_[1]);
  const auto& spelling_markers2 =
      std::get<1>(provider_.text_check_requests_[1]);
  auto& callback2 = std::get<2>(provider_.text_check_requests_[1]);
  EXPECT_EQ(text2, u"byee ");
  EXPECT_EQ(spelling_markers2.size(), 1U);
  EXPECT_EQ(spelling_markers2[0],
            spellcheck::SpellingMarker(/*start=*/0, /*end=*/4,
                                       spellcheck::Decoration::SPELLING));
  EXPECT_TRUE(callback2);

  std::vector<SpellCheckResult> fake_results;

  std::move(callback1).Run(fake_results);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(completion1.completion_count_, 1U);
  EXPECT_EQ(completion2.completion_count_, 0U);
  EXPECT_EQ(provider_.pending_text_request_size(), 1U);

  std::move(callback2).Run(fake_results);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(completion1.completion_count_, 1U);
  EXPECT_EQ(completion2.completion_count_, 1U);
  EXPECT_EQ(provider_.pending_text_request_size(), 0U);

  provider_.text_check_requests_.clear();
}

TEST_F(SpellCheckProviderMacTest, CancelOneIfTwoRoundtripsAreIdentical) {
  FakeTextCheckingResult completion1;
  provider_.RequestTextChecking(
      u"hello ", /*spelling_markers=*/{},
      blink::WebTextCheckClient::ShouldForceRefreshTextCheckService::kNo,
      std::make_unique<FakeTextCheckingCompletion>(&completion1));

  EXPECT_EQ(completion1.completion_count_, 0U);
  EXPECT_EQ(provider_.text_check_requests_.size(), 1U);

  const auto& text1 = std::get<0>(provider_.text_check_requests_[0]);
  const auto& spelling_markers1 =
      std::get<1>(provider_.text_check_requests_[0]);
  auto& callback1 = std::get<2>(provider_.text_check_requests_[0]);
  EXPECT_EQ(text1, u"hello ");
  EXPECT_EQ(spelling_markers1.size(), 0U);
  EXPECT_TRUE(callback1);

  std::vector<SpellCheckResult> fake_results;
  std::move(callback1).Run(fake_results);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(completion1.completion_count_, 1U);

  FakeTextCheckingResult completion2;
  provider_.RequestTextChecking(
      u"hello ", /*spelling_markers=*/{},
      blink::WebTextCheckClient::ShouldForceRefreshTextCheckService::kNo,
      std::make_unique<FakeTextCheckingCompletion>(&completion2));

  // The second identical request will be dropped.
  EXPECT_EQ(provider_.text_check_requests_.size(), 1U);
  EXPECT_EQ(provider_.pending_text_request_size(), 0U);

  provider_.text_check_requests_.clear();
}

TEST_F(SpellCheckProviderMacTest,
       SendAllIdenticalRequestsIfShouldForceRefreshFlagIsEnabled) {
  FakeTextCheckingResult completion1;
  provider_.RequestTextChecking(
      u"hello ", /*spelling_markers=*/{},
      blink::WebTextCheckClient::ShouldForceRefreshTextCheckService::kYes,
      std::make_unique<FakeTextCheckingCompletion>(&completion1));

  EXPECT_EQ(completion1.completion_count_, 0U);
  EXPECT_EQ(provider_.text_check_requests_.size(), 1U);

  const auto& text1 = std::get<0>(provider_.text_check_requests_[0]);
  const auto& spelling_markers1 =
      std::get<1>(provider_.text_check_requests_[0]);
  auto& callback1 = std::get<2>(provider_.text_check_requests_[0]);
  EXPECT_EQ(text1, u"hello ");
  EXPECT_EQ(spelling_markers1.size(), 0U);
  EXPECT_TRUE(callback1);

  std::vector<SpellCheckResult> fake_results;
  std::move(callback1).Run(fake_results);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(completion1.completion_count_, 1U);

  FakeTextCheckingResult completion2;
  provider_.RequestTextChecking(
      u"hello ", /*spelling_markers=*/{},
      blink::WebTextCheckClient::ShouldForceRefreshTextCheckService::kYes,
      std::make_unique<FakeTextCheckingCompletion>(&completion2));

  // The second identical request will be processed.
  EXPECT_EQ(provider_.text_check_requests_.size(), 2U);
  EXPECT_EQ(provider_.pending_text_request_size(), 1U);

  const auto& text2 = std::get<0>(provider_.text_check_requests_[1]);
  const auto& spelling_markers2 =
      std::get<1>(provider_.text_check_requests_[1]);
  auto& callback2 = std::get<2>(provider_.text_check_requests_[1]);
  EXPECT_EQ(text2, u"hello ");
  EXPECT_EQ(spelling_markers2.size(), 0U);
  EXPECT_TRUE(callback2);
  std::move(callback2).Run(fake_results);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(completion2.completion_count_, 1U);

  provider_.text_check_requests_.clear();
}

}  // namespace
