// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROME_ASH_TEST_BASE_H_
#define CHROME_TEST_BASE_CHROME_ASH_TEST_BASE_H_

#include "ash/test/ash_test_base.h"

// AshTestBase used in Chrome.
// TODO(crbug.com/40798802): Chrome should not have tests subclassing
// AshTestBase.
class ChromeAshTestBase : public ash::AshTestBase {
 public:
  ChromeAshTestBase();
  ChromeAshTestBase(
      std::unique_ptr<base::test::TaskEnvironment> task_environment)
      : AshTestBase(std::move(task_environment)) {}

  ChromeAshTestBase(const ChromeAshTestBase&) = delete;
  ChromeAshTestBase& operator=(const ChromeAshTestBase&) = delete;
  ~ChromeAshTestBase() override;
};

#endif  // CHROME_TEST_BASE_CHROME_ASH_TEST_BASE_H_
