// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_IDLE_ACTION_TYPE_H_
#define COMPONENTS_ENTERPRISE_IDLE_ACTION_TYPE_H_

#include <string>

#include "base/values.h"
#include "build/build_config.h"

namespace enterprise_idle {

// Action types supported by IdleTimeoutActions.
//
// Actions run in order, based on their numerical value. Lower values run first.
// Keep this enum sorted by priority.
enum class ActionType {
  kShowDialog = 0,  // Not an IdleTimeoutAction value. Added as a side-effect.
#if !BUILDFLAG(IS_ANDROID)
  kCloseBrowsers = 1,
  kShowProfilePicker = 2,
#endif  // !BUILDFLAG(IS_ANDROID)
  kClearBrowsingHistory = 3,
  kClearDownloadHistory,
  kClearCookiesAndOtherSiteData,
  kClearCachedImagesAndFiles,
  kClearPasswordSignin,
  kClearAutofill,
  kClearSiteSettings,
  kClearHostedAppData,
  kReloadPages,
  kShowBubble,  // Not an IdleTimeoutAction value. Added as a side-effect.
};

// Checks if the action type does not require sync types to be disabled.
bool AllowsSyncEnabled(const std::string& name);

// Returns the idle timeout action type for an action string.
absl::optional<ActionType> NameToActionType(const std::string& name);

// Returns the name of the browsing data type that should be cleared given
// `clear_*` action name.
std::string GetActionBrowsingDataTypeName(const std::string& action);

}  // namespace enterprise_idle

#endif  // COMPONENTS_ENTERPRISE_IDLE_ACTION_TYPE_H_
