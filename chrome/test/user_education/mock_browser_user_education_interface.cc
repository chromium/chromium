// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/user_education/mock_browser_user_education_interface.h"

#include "chrome/browser/ui/user_education/browser_user_education_interface.h"

MockBrowserUserEducationInterface::MockBrowserUserEducationInterface(
    BrowserWindowInterface* browser)
    : BrowserUserEducationInterface(browser) {}

MockBrowserUserEducationInterface::~MockBrowserUserEducationInterface() =
    default;
