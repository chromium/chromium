// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/test/quick_answers_test_base.h"

QuickAnswersTestBase::QuickAnswersTestBase() = default;

QuickAnswersTestBase::~QuickAnswersTestBase() = default;

void QuickAnswersTestBase::SetUp() {
  testing::Test::SetUp();

  if (!QuickAnswersState::Get())
    quick_answers_state_ = std::make_unique<QuickAnswersState>();
}

void QuickAnswersTestBase::TearDown() {
  quick_answers_state_.reset();
  testing::Test::TearDown();
}
