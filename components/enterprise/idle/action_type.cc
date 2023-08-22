
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/idle/action_type.h"

#include <cstring>
#include <regex>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"

#include "base/json/values_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"

namespace enterprise_idle {

namespace {
#if !BUILDFLAG(IS_ANDROID)
const char kCloseBrowsersActionName[] = "close_browsers";
const char kShowProfilePickerActionName[] = "show_profile_picker";
#endif  // !BUILDFLAG(IS_ANDROID)
const char kClearBrowsingHistoryActionName[] = "clear_browsing_history";
const char kClearDownloadHistoryActionName[] = "clear_download_history";
const char kClearCookiesAndOtherSiteDataActionName[] =
    "clear_cookies_and_other_site_data";
const char kClearCachedImagesAndFilesActionName[] =
    "clear_cached_images_and_files";
const char kClearPasswordSigninActionName[] = "clear_password_signin";
const char kClearAutofillActionName[] = "clear_autofill";
const char kClearSiteSettingsActionName[] = "clear_site_settings";
const char kClearHostedAppDataActionName[] = "clear_hosted_app_data";
const char kReloadPagesActionName[] = "reload_pages";
}  // namespace

#if !BUILDFLAG(IS_ANDROID)
bool AllowsSyncEnabled(const std::string& name) {
  static const char* kActionsAllowedWithSync[] = {
      kCloseBrowsersActionName,
      kShowProfilePickerActionName,
      kClearDownloadHistoryActionName,
      kClearCookiesAndOtherSiteDataActionName,
      kClearCachedImagesAndFilesActionName,
      kReloadPagesActionName,
      kClearHostedAppDataActionName};
  return base::ranges::any_of(
      base::make_span(kActionsAllowedWithSync),
      [&name](const char* allowed_action) { return allowed_action == name; });
}
#endif  //! BUILDFLAG(IS_ANDROID)

absl::optional<ActionType> NameToActionType(const std::string& name) {
#if !BUILDFLAG(IS_ANDROID)
  if (name == kCloseBrowsersActionName) {
    return ActionType::kCloseBrowsers;
  }
  if (name == kShowProfilePickerActionName) {
    return ActionType::kShowProfilePicker;
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  if (name == kClearBrowsingHistoryActionName) {
    return ActionType::kClearBrowsingHistory;
  }
  if (name == kClearDownloadHistoryActionName) {
    return ActionType::kClearDownloadHistory;
  }
  if (name == kClearCookiesAndOtherSiteDataActionName) {
    return ActionType::kClearCookiesAndOtherSiteData;
  }
  if (name == kClearCachedImagesAndFilesActionName) {
    return ActionType::kClearCachedImagesAndFiles;
  }
  if (name == kClearPasswordSigninActionName) {
    return ActionType::kClearPasswordSignin;
  }
  if (name == kClearAutofillActionName) {
    return ActionType::kClearAutofill;
  }
  if (name == kClearSiteSettingsActionName) {
    return ActionType::kClearSiteSettings;
  }
  if (name == kClearHostedAppDataActionName) {
    return ActionType::kClearHostedAppData;
  }
  if (name == kReloadPagesActionName) {
    return ActionType::kReloadPages;
  }
  return absl::nullopt;
}

std::string GetActionBrowsingDataTypeName(const std::string& action) {
  // Get the data type to be cleared if the action is to clear browsig data.
  const char kPrefix[] = "clear_";
  if (!base::StartsWith(action, kPrefix, base::CompareCase::SENSITIVE)) {
    return std::string();
  }
  return action.substr(std::strlen(kPrefix));
}

}  // namespace enterprise_idle
