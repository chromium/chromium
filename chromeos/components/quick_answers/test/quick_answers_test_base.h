// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_TEST_QUICK_ANSWERS_TEST_BASE_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_TEST_QUICK_ANSWERS_TEST_BASE_H_

#include <memory>

#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/test/fake_quick_answers_state.h"
#include "testing/gtest/include/gtest/gtest.h"

// Helper class for Quick Answers related tests.
class QuickAnswersTestBase : public testing::Test {
 public:
  QuickAnswersTestBase();

  QuickAnswersTestBase(const QuickAnswersTestBase&) = delete;
  QuickAnswersTestBase& operator=(const QuickAnswersTestBase&) = delete;

  ~QuickAnswersTestBase() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  FakeQuickAnswersState* fake_quick_answers_state() {
    CHECK(fake_quick_answers_state_);
    return &fake_quick_answers_state_.value();
  }

 private:
  std::optional<FakeQuickAnswersState> fake_quick_answers_state_;
};

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_TEST_QUICK_ANSWERS_TEST_BASE_H_
