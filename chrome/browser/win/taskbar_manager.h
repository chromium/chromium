// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_TASKBAR_MANAGER_H_
#define CHROME_BROWSER_WIN_TASKBAR_MANAGER_H_

#include <string>

#include "base/functional/callback_forward.h"

namespace browser_util {

using PinResultCallback = base::OnceCallback<void(bool)>;

// Functions to pin an icon for a Chrome window to the Windows taskbar, and to
// check if Chrome should offer to pin. These functions do most of their work on
// a background thread, but have to finish the work on the UI thread.
// The result callback will be called from the thread that called them.

// `callback` is called with true if pinning is supported, and the app is not
// currently pinned to the taskbar, false otherwise. There must be a shortcut
// with `app_user_model_id` in the start menu for pinning to be supported.
void ShouldOfferToPin(const std::wstring& app_user_model_id,
                      PinResultCallback callback);

// Pins a Chrome window to the taskbar. `app_user_model_id` is the AUMI for
// the window to pin. The user should have requested this window be pinned,
// per the Microsoft limited feature access request form.
// There must be a shortcut in the Start Menu folder with the same AUMI.
// It uses the Windows TaskbarManager method `RequestPinCurrentAppAsync`, which
// will confirm that the user wishes to pin the window to the taskbar.
void PinAppToTaskbar(const std::wstring& app_user_model_id,
                     PinResultCallback callback);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. These values are recorded both for
// determining if Chrome can be pinned to the taskbar, and if an attempt was
// made to pin Chrome to the taskbar. Public for testing.
enum class PinResultMetric {
  // Shortcut was pinned, or we successfully determined if we should pin.
  kSuccess = 0,

  // `PinLimitedAccessFeatureAvailable` returned false.
  kFeatureNotAvailable = 1,

  // There are a number of COM calls that can fail. This is unexpected.
  kCOMError = 2,

  // Error calling ITaskbarManager method.
  kTaskbarManagerError = 3,

  // Error posting async results.
  kPostAsyncResultsFailed = 4,

  // `get_IsPinningAllowed` false. This could be because there is no shortcut to
  // the app in the start menu, or some other Windows criteria for app pinning
  // has not been met.
  kPinningNotAllowed = 5,

  // Chrome already pinned to taskbar.
  kAlreadyPinned = 6,

  // Successfully called RequestPinCurrentAppAsync and the request failed.
  // This could be because the user rejected the Windows confirmation, or some
  // other internal failure.
  kPinCurrentAppFailed = 7,
  kMaxValue = kPinCurrentAppFailed,
};

}  // namespace browser_util

#endif  // CHROME_BROWSER_WIN_TASKBAR_MANAGER_H_
