// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/idle/idle_pref_names.h"

namespace enterprise_idle::prefs {
// Number of minutes of inactivity before running actions from
// kIdleTimeoutActions. Controlled via the IdleTimeout policy.
const char kIdleTimeout[] = "idle_timeout";

// Actions to run when the idle timeout is reached. Controller via the
// IdleTimeoutActions policy.
const char kIdleTimeoutActions[] = "idle_timeout_actions";

// If true, show the IdleTimeout bubble when Chrome starts.
const char kIdleTimeoutShowBubbleOnStartup[] =
    "idle_timeout_show_bubble_on_startup";

// The last active time updated based on taps registered in
// `browser_view_controller.cc`
const char kLastActiveTimestamp[] = "idle_timeout_last_active_timestamp";

// The time when the browser was last marked as idle. Used with
// `kLastActiveTimestamp` to calculate the idle time on start-up.
const char kLastIdleTimestamp[] = "idle_timeout_last_idle_timestamp";

// If true and a data clearing action is set, data will only be cleared for the
// duration the user was signed in. The policy will also clear data on signout
// since this data will not be accessible on the next timeout when the user
// signs in again with the policy set.
const char kIdleTimeoutPolicyAppliesToUserOnly[] =
    "idle_timeout_applies_to_user_only";

}  // namespace enterprise_idle::prefs
