// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_ANDROID_ANDROID_TIME_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_ANDROID_ANDROID_TIME_H_

#include <stdint.h>

#include "base/time/time.h"

namespace history {

// Android's system time is the milliseconds since January 1, 1970 00:00:00 UTC,
// the below 2 methods are used convert between base::Time and the milliseconds
// stored in database.
inline base::Time FromDatabaseTime(int64_t milliseconds) {
  return base::Milliseconds(milliseconds) + base::Time::UnixEpoch();
}

inline int64_t ToDatabaseTime(const base::Time& time) {
  return (time - base::Time::UnixEpoch()).InMilliseconds();
}

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_ANDROID_ANDROID_TIME_H_
