// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/quick_answers/test/chrome_quick_answers_test_base.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "chrome/browser/ui/ash/quick_answers/quick_answers_controller_impl.h"

ChromeQuickAnswersTestBase::ChromeQuickAnswersTestBase() = default;

ChromeQuickAnswersTestBase::~ChromeQuickAnswersTestBase() = default;

void ChromeQuickAnswersTestBase::SetUp() {
  ChromeAshTestBase::SetUp();

  if (!ash::QuickAnswersController::Get())
    quick_answers_controller_ =
        std::make_unique<ash::QuickAnswersControllerImpl>();

  ash::QuickAnswersState::Get()->RegisterPrefChanges(
      ash::Shell::Get()->session_controller()->GetPrimaryUserPrefService());
}

void ChromeQuickAnswersTestBase::TearDown() {
  quick_answers_controller_.reset();

  ChromeAshTestBase::TearDown();
}
