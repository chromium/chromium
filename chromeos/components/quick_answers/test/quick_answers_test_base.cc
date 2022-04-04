// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/test/quick_answers_test_base.h"

#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

#include "base/logging.h"

QuickAnswersTestBase::QuickAnswersTestBase() = default;

QuickAnswersTestBase::~QuickAnswersTestBase() = default;

void QuickAnswersTestBase::SetUp() {
  testing::Test::SetUp();

  // Setup test pref service.
  test_pref_service_ = std::make_unique<TestingPrefServiceSimple>();
  // Register profile prefs observed by Quick answers state.
  auto* registry = test_pref_service_->registry();
  quick_answers::prefs::RegisterProfilePrefs(registry);
  registry->RegisterStringPref(language::prefs::kApplicationLocale,
                               std::string());
  registry->RegisterStringPref(language::prefs::kPreferredLanguages,
                               std::string());

  if (!QuickAnswersState::Get()) {
    quick_answers_state_ = std::make_unique<QuickAnswersState>();
  }

  quick_answers_state_->RegisterPrefChanges(test_pref_service_.get());
}

void QuickAnswersTestBase::TearDown() {
  quick_answers_state_.reset();
  test_pref_service_.reset();
  testing::Test::TearDown();
}
