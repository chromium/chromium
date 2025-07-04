// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_opened_tabs_counter_service.h"

IsolatedWebAppsOpenedTabsCounterService::
    IsolatedWebAppsOpenedTabsCounterService(Profile* profile)
    : profile_(*profile) {}

IsolatedWebAppsOpenedTabsCounterService::
    ~IsolatedWebAppsOpenedTabsCounterService() = default;
