// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_IDLE_ACTION_TYPE_H_
#define COMPONENTS_ENTERPRISE_IDLE_ACTION_TYPE_H_

#include <string>

#include "base/values.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"

namespace enterprise_idle {

// Action types supported by IdleTimeoutActions.
//
// Actions run in order, based on their numerical value. Lower values run first.
// Keep this enum sorted by priority.
enum class ActionType {
#if !BUILDFLAG(IS_IOS)
  kShowDialog = 0,  // Not an IdleTimeoutAction value. Added as a side-effect.
#endif              // !BUILDFLAG(IS_IOS)
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  kCloseBrowsers = 1,
  kShowProfilePicker = 2,
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  kClearBrowsingHistory = 3,
  kClearCookiesAndOtherSiteData,
  kClearCachedImagesAndFiles,
  kClearPasswordSignin,
  kClearAutofill,
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  kClearDownloadHistory,
  kClearHostedAppData,
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#if BUILDFLAG(IS_IOS)
  kCloseTabs,
  kSignOut,
#else
  kClearSiteSettings,
  kReloadPages,
  kShowBubble,  // Not an IdleTimeoutAction value. Added as a side-effect.
#endif  // BUILDFLAG(IS_IOS)
};

// Returns the idle timeout action type for an action string.
std::optional<ActionType> NameToActionType(const std::string& name);

// Returns the name of the browsing data type that should be cleared given
// `clear_*` action name.
std::string GetActionBrowsingDataTypeName(const std::string& action);

// Returns the ActionTypes based on the IdleTimeoutActions pref value.
std::vector<ActionType> GetActionTypesFromPrefs(PrefService* prefs);

}  // namespace enterprise_idle

#endif  // COMPONENTS_ENTERPRISE_IDLE_ACTION_TYPE_H_
