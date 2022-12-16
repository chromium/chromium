// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_CONSTRAINTS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_CONSTRAINTS_H_

#include "base/time/time.h"

namespace content_settings {

// Options to restrict the scope of a content setting. These specify the
// lifetime model of a given setting and how it may become invalidated or
// expired.
// Durable:     Settings persist forever and are bounded only by an expiry date,
//              if set.
// UserSession: Settings will persist no longer than the user session
//              regardless of expiry date, if set.
// NonRestorableUserSession: Same as UserSession, except this session-based
//              setting will be reset when the user session ends regardless
//              the restore setting. These settings will not be restored e.g.
//              when the user selected "continue where you left off" or after
//              a crash or update related restart.
// OneTime:     Settings will persist for the current "tab session", meaning
//              until the last tab from the origin is closed.
enum class SessionModel {
  Durable = 0,
  UserSession = 1,
  NonRestorableUserSession = 2,
  OneTime = 3,
  kMaxValue = OneTime,
};

// Constraints to be applied when setting a content setting.
struct ContentSettingConstraints {
  // Specification of an |expiration| provides an upper bound on the time a
  // setting will remain valid. If 0 is specified for |expiration| no time limit
  // will apply.
  base::Time expiration;
  // Used to specify the lifetime model that should be used.
  SessionModel session_model = SessionModel::Durable;
  // Set to true to keep track of the last visit to the origin of this
  // permission.
  // This is used for the Safety check permission module and unrelated to the
  // "expiration" keyword above.
  bool track_last_visit_for_autoexpiration = false;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_CONSTRAINTS_H_
