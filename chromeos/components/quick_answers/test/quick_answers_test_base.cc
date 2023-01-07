// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/test/quick_answers_test_base.h"

QuickAnswersTestBase::QuickAnswersTestBase() = default;

QuickAnswersTestBase::~QuickAnswersTestBase() = default;

void QuickAnswersTestBase::SetUp() {
  testing::Test::SetUp();

  DCHECK(!QuickAnswersState::Get());

  fake_quick_answers_state_ = std::make_unique<FakeQuickAnswersState>();
}

void QuickAnswersTestBase::TearDown() {
  fake_quick_answers_state_.reset();
  testing::Test::TearDown();
}
