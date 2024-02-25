// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HANDOFF_PREF_NAMES_IOS_H_
#define COMPONENTS_HANDOFF_PREF_NAMES_IOS_H_

namespace prefs {

// Whether Chrome should attempt to hand off the current URL to other Apple
// devices that share an iCloud account.
inline constexpr char kIosHandoffToOtherDevices[] =
    "ios.handoff_to_other_devices";

}  // namespace prefs

#endif  // COMPONENTS_HANDOFF_PREF_NAMES_IOS_H_
