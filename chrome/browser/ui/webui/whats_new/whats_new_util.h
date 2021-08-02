// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_UTIL_H_

class PrefService;

namespace whats_new {
extern const char kChromeWhatsNewURL[];
extern const char kChromeWhatsNewURLShort[];

// Allows tests to force What's New to stay open even if loading remote content
// doesn't succeed, since this does not work in tests.
extern bool g_force_enable_for_tests;

bool ShouldShowForState(PrefService* local_state);
void SetLastVersion(PrefService* local_state);
}  // namespace whats_new

#endif  // CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_UTIL_H_
