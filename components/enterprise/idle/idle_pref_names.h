// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_IDLE_IDLE_PREF_NAMES_H_
#define COMPONENTS_ENTERPRISE_IDLE_IDLE_PREF_NAMES_H_

namespace enterprise_idle::prefs {
extern const char kIdleTimeout[];
extern const char kIdleTimeoutActions[];
extern const char kIdleTimeoutShowBubbleOnStartup[];

extern const char kLastActiveTimestamp[];
extern const char kLastIdleTimestamp[];
extern const char kIdleTimeoutPolicyAppliesToUserOnly[];
}  // namespace enterprise_idle::prefs

#endif  // COMPONENTS_ENTERPRISE_IDLE_IDLE_PREF_NAMES_H_
