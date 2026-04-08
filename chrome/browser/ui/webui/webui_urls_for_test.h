// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_URLS_FOR_TEST_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_URLS_FOR_TEST_H_

#include <string_view>
#include "base/containers/span.h"

// Returns a list of chrome:// URLs to test for:
//  1) TrustedTypes violations (see NoTrustedTypesViolation test).
//  2) Presence of TrustedTypes checks (see TrustedTypesEnabled test).
base::span<const std::string_view> GetChromeUrlsForTest();

// Returns a list of chrome:// URLs that fail sanity check tests, i.e. at least
// one of the following is true:
// (1) Have a console error when loaded.
// (2) Have a network (e.g. resource fetch) error when loaded.
// (3) Don't use TrustedTypes.
base::span<const std::string_view> GetUntestedChromeUrlsForTest();

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_URLS_FOR_TEST_H_
