// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_ash_test_base.h"

#include <memory>
#include <utility>

#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/browser_task_environment.h"

ChromeAshTestBase::ChromeAshTestBase()
    : ChromeAshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
          std::make_unique<content::BrowserTaskEnvironment>())) {}

ChromeAshTestBase::ChromeAshTestBase(
    std::unique_ptr<base::test::TaskEnvironment> task_environment)
    : AshTestBase(std::move(task_environment),
                  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()) {}

ChromeAshTestBase::~ChromeAshTestBase() = default;
