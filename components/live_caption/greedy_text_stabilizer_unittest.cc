// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/greedy_text_stabilizer.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace captions {

class GreedyTextStabilizerTest : public testing::Test {
 public:
  GreedyTextStabilizerTest() = default;
  ~GreedyTextStabilizerTest() override = default;
};

// Ensures that invalid min token frequency gets set to 0.
TEST_F(GreedyTextStabilizerTest, NegativeMinTokenFrequencySetToZero) {
  GreedyTextStabilizer stabilizer(-10);

  // When min_token_frequency is 0, the output is set to the input.
  EXPECT_EQ(stabilizer.UpdateText("Hello world"), "Hello world");
}

// Tests that empty sentences are handled correctly.
TEST_F(GreedyTextStabilizerTest, EmptySentencesHaveZeroStableTokens) {
  std::vector<std::string> partials = {"", "", ""};
  GreedyTextStabilizer stabilizer(1);
  for (const auto& partial : partials) {
    stabilizer.UpdateText(partial);

    // stable_count should be 0 for all results.
    EXPECT_EQ(stabilizer.GetStableTokenCount(), 0);
  }
}

// Tests that complete sentences are handled correctly.
TEST_F(GreedyTextStabilizerTest, CompleteSentencesOutputFullText) {
  GreedyTextStabilizer stabilizer(2);
  // With min_token_frequency=2 the output will be empty for the first partial.
  EXPECT_EQ(stabilizer.UpdateText("Hello world", /*is_final=*/false), "");
  EXPECT_EQ(stabilizer.GetStableTokenCount(), 0);

  // Since the tokens changed, the stabilization will still be empty.
  EXPECT_EQ(stabilizer.UpdateText("Once upon a time", /*is_final=*/false), "");
  EXPECT_EQ(stabilizer.GetStableTokenCount(), 0);

  // But when the sentence is complete, the output should be equal to the input.
  EXPECT_EQ(stabilizer.UpdateText("I don't care if Monday's blue",
                                  /*is_final=*/true),
            "I don't care if Monday's blue");
  // But the number of stable tokens should still be 0.
  EXPECT_EQ(stabilizer.GetStableTokenCount(), 0);
}

// Use the same phrase and only change the punctuation.
// The punctuation will change if it is seen more often than others (the mode).
TEST_F(GreedyTextStabilizerTest, PunctuationFlickerIsStabilized) {
  const int min_token_frequency = 2;
  GreedyTextStabilizer stabilizer(min_token_frequency);

  // Partials with flickering punctuation and first letter.
  const std::vector<std::string> partials = {
      "He retired in 2008",  "He retired in 2008.", "He retired in 2008.",
      "he retired in 2008,", "he retired in 2008",  "He retired in 2008.",
      "he retired in 2008",  "He retired in 2008,", "he retired in 2008.",
      "He retired in 2008,", "he retired in 2008"};

  // Here, the first letter and punctuation are stabilized.
  // No output is written at first because the tokens must be seen twice when
  // min_token_frequency = 2.
  const std::vector<std::string> stable_partials = {"",
                                                    "He retired in 2008",
                                                    "He retired in 2008.",
                                                    "He retired in 2008.",
                                                    "He retired in 2008.",
                                                    "He retired in 2008.",
                                                    "He retired in 2008.",
                                                    "He retired in 2008.",
                                                    "He retired in 2008.",
                                                    "He retired in 2008.",
                                                    "He retired in 2008."};

  EXPECT_EQ(partials.size(), stable_partials.size());

  for (unsigned long i = 0; i < partials.size(); ++i) {
    EXPECT_EQ(stabilizer.UpdateText(partials[i]), stable_partials[i]);
  }
}

// Tests the behavior for varying values of the min_token_frequency.
TEST_F(GreedyTextStabilizerTest, MultipleFormsOfFlickerAreStabilized) {
  // Here the capitalization of the first token flickers, the punctuation
  // changes from nothing, to periods, to commas, and some tokens change.
  // These should be stabilized by the algorithm.
  const std::vector<std::string> partials = {
      "After work",
      "After working,",  // The second token got longer.
      "After work,",
      "after work, he",  // Here the "A" was changed to "a".
      "after work. he",  // Here the "," was changed to ".".
      "After work, he wood",
      "After work. he would run.",  // We changed to a "." here.
      "After work, he would run.",
      "After work, he would run",  // The "." was removed here.
      "After work, he would run. It is",
  };

  // When min frequency is 1, tokens are accepted as soon as they are seen.
  // But, by nature of the algorithm, the result at a location can still
  // change if a different token gets more votes over time.
  const std::vector<std::string> stable_partials_1 = {
      "After work",
      "After working,",  // The second token is allowed because the frequency=1.
      "After work,",
      "After work,",          // Here the "A" remains but nothing is added.
      "After work,",          // Here the "," remains, but nothing is added.
      "After work, he wood",  // We allowed "wood" here.
      "After work, he wood",  // The punctuation changed, so keep old prefix.
      "After work, he would run.",
      "After work, he would run.",  // The last entry with more tokens is used.
      "After work, he would run. It is",
  };

  // When min frequency is 2, tokens are accepted after being seen at least
  // twice. But, by nature of the algorithm, the result at a location can still
  // change if a different token gets more votes over time.
  const std::vector<std::string> stable_partials_2 = {
      "",        // We haven't seen the tokens often enough to output anything.
      "After ",  // The second token is ignored because it changed.
      "After work,",
      "After work,",
      "After work,",  // We keep the previous prefix to retain the length.
      "After work, he",
      "After work, he",  // We keep the previous prefix to retain the length.
      "After work, he would run.",
      "After work, he would run.",  // We keep the previous prefix.
      "After work, he would run.",
  };

  EXPECT_EQ(partials.size(), stable_partials_1.size());
  EXPECT_EQ(partials.size(), stable_partials_2.size());
  GreedyTextStabilizer stabilizer(0);
  for (auto partial : partials) {
    EXPECT_EQ(stabilizer.UpdateText(partial), partial);
  }

  GreedyTextStabilizer stabilizer_1(1);
  for (unsigned long i = 0; i < partials.size(); ++i) {
    EXPECT_EQ(stabilizer_1.UpdateText(partials[i]), stable_partials_1[i]);
  }

  GreedyTextStabilizer stabilizer_2(2);
  for (unsigned long i = 0; i < partials.size(); ++i) {
    EXPECT_EQ(stabilizer_2.UpdateText(partials[i]), stable_partials_2[i]);
  }
}

// Test that greedy text stabilization works for long text sequences.
TEST_F(GreedyTextStabilizerTest, FlickerInLongFormTextIsStabilized) {
  const std::vector<std::string> partials = {
      "soy",
      "yo no",
      "no soy vidente",
      "no soy vidente divisores de cajones con dicho",
      "no soy vidente divisores de cajones con dicho es",
      "no soy divisores de cajones videntes con decir es el",
      "no soy vidente divisores de cajones con dicho",
      "no soy un divisor de cajones videntes al decir su defensa yo",
      "no soy vidente divisores de cajones con decir su defensa i",
      "no soy un divisor de cajones videntes al decir su defensa yo "
      "simplemente",
      "no soy divisor de cajones videntes con decir su defensa simplemente",
      "no soy un divisor de cajones videntes al decir su defensa yo "
      "simplemente",
      "no soy un divisor de cajones videntes al decir su defensa simplemente "
      "hago conjeturas",
      "no soy un divisor de cajones videntes al decir su defensa simplemente "
      "hago conjeturas en el sótano",
      "no soy un divisor de cajones videntes al decir su defensa simplemente "
      "hago conjeturas en los países del sótano",
      "no soy un divisor de cajones videntes al decir su defensa simplemente "
      "hago conjeturas en los países del sótano que",
      "no soy un divisor de cajones videntes al decir su defensa simplemente "
      "hago conjeturas de los países del sótano que yo"};

  const std::vector<std::string> stable_partials = {
      "",
      "",
      "",
      "no soy vidente",
      "no soy vidente divisores de cajones con dicho",
      "no soy vidente divisores de cajones con dicho",
      "no soy vidente divisores de cajones con dicho",
      "no soy vidente divisores de cajones con dicho",
      "no soy vidente divisores de cajones con dicho",
      "no soy vidente divisores de cajones con dicho",
      "no soy vidente divisores de cajones con dicho",
      "no soy vidente divisores de cajones con dicho",
      "no soy vidente divisores de cajones con dicho",
      "no soy vidente divisores de cajones con dicho",
      "no soy un divisor de cajones videntes al decir su defensa simplemente "
      "hago conjeturas en ",
      "no soy un divisor de cajones videntes al decir su defensa simplemente "
      "hago conjeturas en los países del sótano",
      "no soy un divisor de cajones videntes al decir su defensa simplemente "
      "hago conjeturas en los países del sótano"};

  EXPECT_EQ(partials.size(), stable_partials.size());

  GreedyTextStabilizer stabilizer(2);
  for (unsigned long i = 0; i < partials.size(); ++i) {
    EXPECT_EQ(stabilizer.UpdateText(partials[i]), stable_partials[i]);
  }
}

TEST_F(GreedyTextStabilizerTest, KoreanTokensAreStabilized) {
  const int min_token_frequency = 1;
  GreedyTextStabilizer stabilizer(min_token_frequency);

  const std::vector<std::string> partials = {
      "나는",
      "난 아니야",
      "난 선견자가 아니야",
      "내가 말하는 서랍 서랍 분배기가 아니야",
      "나는 서랍 서랍 분배기가 아니야",
      "나는 서랍 서랍 분배기가 아니야",
      "내가 말하는 서랍 서랍 분배기가 아니야",
      "나는 그의 방어를 말하는 씨 서랍 서랍이 아닙니다 나는",
      "나는 그의 방어를 말하는 시어 서랍 칸막이가 아니야",
      "나는 그의 방어를 말하는 씨 서랍 서랍이 아닙니다 나는 단순히",
      "나는 단순히 자신의 방어를 말하는 시어 서랍 디바이더가 아닙니다",
      "나는 그의 방어를 말하는 씨 서랍 서랍이 아닙니다 나는 단순히",
      "나는 그의 방어를 말하는 씨 서랍 서랍이 아닙니다 나는 단순히 추측을한다"};

  // Stable partials when min_token_frequency=1.
  const std::vector<std::string> stable_partials = {
      "나는",
      "난 아니야",
      "난 선견자가 아니야",
      "난 선견자가 아니야",
      "나는 서랍 서랍 분배기가 아니야",
      "나는 서랍 서랍 분배기가 아니야",
      "나는 서랍 서랍 분배기가 아니야",
      "나는 서랍 서랍 분배기가 아니야",
      "나는 서랍 서랍 분배기가 아니야",
      "나는 서랍 서랍 분배기가 아니야",
      "나는 서랍 서랍 분배기가 아니야",
      "나는 그의 방어를 말하는 씨 서랍 서랍이 아닙니다 나는 단순히",
      "나는 그의 방어를 말하는 씨 서랍 서랍이 아닙니다 나는 단순히 추측을한다"};

  EXPECT_EQ(partials.size(), stable_partials.size());

  for (unsigned long i = 0; i < partials.size(); ++i) {
    EXPECT_EQ(stabilizer.UpdateText(partials[i]), stable_partials[i]);
  }
}

}  // namespace captions
