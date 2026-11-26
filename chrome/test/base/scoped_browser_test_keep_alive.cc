// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/scoped_browser_test_keep_alive.h"

#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "components/keep_alive_registry/keep_alive_types.h"

ScopedBrowserTestKeepAlive::ScopedBrowserTestKeepAlive(Profile& profile)
    : keep_alive_(KeepAliveOrigin::BROWSER_TEST,
                  KeepAliveRestartOption::DISABLED),
      profile_keep_alive_(&profile, ProfileKeepAliveOrigin::kBrowserWindow) {}

ScopedBrowserTestKeepAlive::~ScopedBrowserTestKeepAlive() = default;
