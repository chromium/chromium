// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/test/quick_answers_test_base.h"

QuickAnswersTestBase::QuickAnswersTestBase() = default;

QuickAnswersTestBase::~QuickAnswersTestBase() = default;

void QuickAnswersTestBase::SetUp() {
  testing::Test::SetUp();

  CHECK(!QuickAnswersState::Get());

  fake_quick_answers_state_.emplace();

  // Default values of intent eligibilities are true.
  fake_quick_answers_state_->SetDefinitionEligible(true);
  fake_quick_answers_state_->SetTranslationEligible(true);
  fake_quick_answers_state_->SetUnitConversionEligible(true);
}

void QuickAnswersTestBase::TearDown() {
  fake_quick_answers_state_.reset();
  testing::Test::TearDown();
}
