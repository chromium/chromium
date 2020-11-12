// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_CONSTRAINTS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_CONSTRAINTS_H_

#include "base/time/time.h"

namespace content_settings {

// Options to restrict the scope of a content setting. These specify the
// lifetime model of a given setting and how it may become invalidated or
// expired.
// Durable:     Settings persist forever and are bounded only by an expiry date,
//              if set.
// UserSession: Settings will persist no longer than the user session
//              regardless of expiry date, if set.
enum class SessionModel {
  Durable = 0,
  UserSession = 1,
  kMaxValue = UserSession,
};

// Constraints to be applied when setting a content setting.
struct ContentSettingConstraints {
  // Specification of an |expiration| provides an upper bound on the time a
  // setting will remain valid. If 0 is specified for |expiration| no time limit
  // will apply.
  base::Time expiration;
  // Used to specify the lifetime model that should be used.
  SessionModel session_model = SessionModel::Durable;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_CONSTRAINTS_H_
