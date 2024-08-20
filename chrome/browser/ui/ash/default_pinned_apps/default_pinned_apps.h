// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DEFAULT_PINNED_APPS_DEFAULT_PINNED_APPS_H_
#define CHROME_BROWSER_UI_ASH_DEFAULT_PINNED_APPS_DEFAULT_PINNED_APPS_H_

#include <vector>

#include "content/public/browser/browser_context.h"

using StaticAppId = const char*;

std::vector<StaticAppId> GetDefaultPinnedAppsForFormFactor(
    content::BrowserContext* browser_context);

#endif  // CHROME_BROWSER_UI_ASH_DEFAULT_PINNED_APPS_DEFAULT_PINNED_APPS_H_
