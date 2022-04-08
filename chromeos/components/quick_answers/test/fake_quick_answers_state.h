// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_TEST_FAKE_QUICK_ANSWERS_STATE_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_TEST_FAKE_QUICK_ANSWERS_STATE_H_

#include <memory>

#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"

class FakeQuickAnswersState : public QuickAnswersState {
 public:
  FakeQuickAnswersState();

  FakeQuickAnswersState(const FakeQuickAnswersState&) = delete;
  FakeQuickAnswersState& operator=(const FakeQuickAnswersState&) = delete;

  ~FakeQuickAnswersState() override;

  void set_application_locale(const std::string& locale) {
    resolved_application_locale_ = locale;
  }
  void set_preferred_languages(const std::string& preferred_languages) {
    preferred_languages_ = preferred_languages;
  }
};

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_TEST_FAKE_QUICK_ANSWERS_STATE_H_
