// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEFAULT_PINNED_APPS_DEFAULT_PINNED_APPS_H_
#define CHROMEOS_ASH_COMPONENTS_DEFAULT_PINNED_APPS_DEFAULT_PINNED_APPS_H_

#include <vector>

#include "base/component_export.h"
#include "content/public/browser/browser_context.h"

using StaticAppId = const char*;

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEFAULT_PINNED_APPS)
std::vector<StaticAppId> GetDefaultPinnedAppsForFormFactor(
    content::BrowserContext* browser_context);

#endif  // CHROMEOS_ASH_COMPONENTS_DEFAULT_PINNED_APPS_DEFAULT_PINNED_APPS_H_
