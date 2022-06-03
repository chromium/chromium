// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_UMA_UTIL_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_UMA_UTIL_H_

#include "components/content_settings/core/common/content_settings.h"

namespace content_settings {

void LogWebSiteSettingsPermissionChange(ContentSettingsType type,
                                        ContentSetting setting);
}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_UMA_UTIL_H_
