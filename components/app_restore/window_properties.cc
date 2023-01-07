// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/window_properties.h"

#include "components/app_restore/window_info.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(APP_RESTORE), int32_t*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(APP_RESTORE),
                                       app_restore::WindowInfo*)

namespace app_restore {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(int32_t, kActivationIndexKey, nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kAppIdKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kAppTypeBrowser, false)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kBrowserAppNameKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kGhostWindowSessionIdKey, 0)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kLacrosWindowId, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kLaunchedFromAppRestoreKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kParentToHiddenContainerKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kRealArcTaskWindow, true)
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kRestoreWindowIdKey, 0)
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kWindowIdKey, 0)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(WindowInfo, kWindowInfoKey, nullptr)

}  // namespace app_restore
