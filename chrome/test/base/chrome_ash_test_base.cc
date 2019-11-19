// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_ash_test_base.h"

ChromeAshTestBase::ChromeAshTestBase()
    : ash::AshTestBase(ash::AshTestBase::SubclassManagesTaskEnvironment()) {}

ChromeAshTestBase::~ChromeAshTestBase() = default;
