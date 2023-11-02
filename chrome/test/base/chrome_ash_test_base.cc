// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_ash_test_base.h"

#include <memory>

#include "content/public/test/browser_task_environment.h"

ChromeAshTestBase::ChromeAshTestBase()
    : ash::AshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
          std::make_unique<content::BrowserTaskEnvironment>())) {}

ChromeAshTestBase::~ChromeAshTestBase() = default;
