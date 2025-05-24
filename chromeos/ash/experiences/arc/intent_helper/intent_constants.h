// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_INTENT_HELPER_INTENT_CONSTANTS_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_INTENT_HELPER_INTENT_CONSTANTS_H_

namespace arc {

// Well-known Android intent actions, e.g. "android.intent.action.VIEW"
// (corresponding to Android's android.content.Intent.ACTION_VIEW constant).
inline constexpr char kIntentActionMain[] = "android.intent.action.MAIN";
inline constexpr char kIntentActionView[] = "android.intent.action.VIEW";
inline constexpr char kIntentActionSend[] = "android.intent.action.SEND";
inline constexpr char kIntentActionSendMultiple[] =
    "android.intent.action.SEND_MULTIPLE";

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_INTENT_HELPER_INTENT_CONSTANTS_H_
