// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_TYPES_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_TYPES_H_

#include "base/functional/callback_forward.h"
#include "base/types/strong_alias.h"

class Browser;

// Type of the callbacks that are called to be notified that the switch to a
// given step by `ProfileManagementFlowController` is completed. `success` is
// is set to false if some sort of error is detected, and `true` otherwise.
// This type is intended for documentation purposes, there is no plan to treat
// it like an opaque type.
using StepSwitchFinishedCallback = base::OnceCallback<void(bool success)>;

// Callback executed when the flow finishes, after the host was cleared and
// we opened a browser for the newly set up profile.
// This callback should not rely on profile management flow instances, as we
// assume that they are deleted when the host is cleared.
// The provided `Browser*` may be `nullptr` if the operation failed.
using PostHostClearedCallback =
    base::StrongAlias<class PostHostClearedCallbackTag,
                      base::OnceCallback<void(Browser*)>>;

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_TYPES_H_
