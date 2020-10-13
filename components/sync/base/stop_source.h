// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_STOP_SOURCE_H_
#define COMPONENTS_SYNC_BASE_STOP_SOURCE_H_

namespace syncer {

// Enumerate the main sources that can turn off sync. This enum is used to
// back a UMA histogram and should be treated as append-only.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.sync
enum StopSource {
  // Deprecated: PROFILE_DESTRUCTION = 0,
  SIGN_OUT = 1,              // The user signed out of Chrome.
  BIRTHDAY_ERROR = 2,        // A dashboard stop-and-clear on the server.
  CHROME_SYNC_SETTINGS = 3,  // The on/off switch in settings for mobile Chrome.
  ANDROID_CHROME_SYNC = 4,   // Android's sync setting for Chrome.
  ANDROID_MASTER_SYNC = 5,   // Android's master sync setting.
  STOP_SOURCE_LIMIT = 6,
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_STOP_SOURCE_H_
