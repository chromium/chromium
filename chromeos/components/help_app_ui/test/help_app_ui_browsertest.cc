// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/help_app_ui/test/help_app_ui_browsertest.h"

#include "base/files/file_path.h"
#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/components/web_applications/test/sandboxed_web_ui_test_base.h"

constexpr base::FilePath::CharType kHelpAppGuestTestApi[] = FILE_PATH_LITERAL(
    "chromeos/components/help_app_ui/test/guest_query_receiver.js");

// Test cases that run in the guest context.
constexpr base::FilePath::CharType kGuestTestCases[] = FILE_PATH_LITERAL(
    "chromeos/components/help_app_ui/test/help_app_guest_ui_browsertest.js");

HelpAppUiBrowserTest::HelpAppUiBrowserTest()
    : SandboxedWebUiAppTestBase(chromeos::kChromeUIHelpAppURL,
                                chromeos::kChromeUIHelpAppUntrustedURL,
                                {base::FilePath(kHelpAppGuestTestApi),
                                 base::FilePath(kGuestTestCases)}) {}

HelpAppUiBrowserTest::~HelpAppUiBrowserTest() = default;
