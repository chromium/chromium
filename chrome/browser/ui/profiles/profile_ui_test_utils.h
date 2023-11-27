// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILES_PROFILE_UI_TEST_UTILS_H_
#define CHROME_BROWSER_UI_PROFILES_PROFILE_UI_TEST_UTILS_H_

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#endif

class ManagedUserProfileNoticeHandler;

// This file contains helper functions for testing profile UIs, in particular,
// the profile picker.
namespace profiles::testing {

// Wait until the widget of the picker gets created and the initialization of
// the picker is thus finished (and notably `ProfilePicker::GetViewForTesting()`
// is not null).
void WaitForPickerWidgetCreated();

// Waits until the profile picker's current `WebContents` stops loading and if
// `expected_url` is not empty, checks that it's the currently displaying URL.
//
// Unlike `contents::WaitForLoadStop()`, this also works if the profile picker's
// current `WebContents` changes throughout the waiting as it is observing
// whichever `WebContents` is displayed by the `WebView`.
void WaitForPickerLoadStop(const GURL& expected_url = GURL());

// Returns when the profile picker's `WebView` displays `url`, blocking to wait
// for navigations if needed.
//
// This also works if the profile picker's current `WebContents` changes
// throughout the waiting as it is observing whichever `WebContents` is
// displayed by the `WebView`.
void WaitForPickerUrl(const GURL& url);

// Waits until the picker gets closed.
void WaitForPickerClosed();

// Checks that the profile picker is currently displaying a managed user
// notice screen of type `expected_type` and returns the handler
// associated with it.
ManagedUserProfileNoticeHandler* ExpectPickerManagedUserNoticeScreenType(
    ManagedUserProfileNoticeUI::ScreenType expected_type);

// Checks that the profile picker is currently displaying a notice screen of
// type `expected_type` and performs the user action represented by `choice` on
// that screen.
void ExpectPickerManagedUserNoticeScreenTypeAndProceed(
    ManagedUserProfileNoticeUI::ScreenType expected_type,
    signin::SigninChoice choice);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void CompleteLacrosFirstRun(
    LoginUIService::SyncConfirmationUIClosedResult result);
#endif

}  // namespace profiles::testing

#endif  // CHROME_BROWSER_UI_PROFILES_PROFILE_UI_TEST_UTILS_H_
