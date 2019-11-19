// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_CLOCK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_CLOCK_H_

namespace base {
class Clock;
class Time;
}

namespace offline_pages {

// Returns the clock to be used for obtaining the current time. This function
// can be called from any threads.
const base::Clock* OfflineClock();

// Allows tests to override the clock returned by |OfflineClock()|. For safety,
// use |TestScopedOfflineClock| instead if possible.
void SetOfflineClockForTesting(const base::Clock* clock);

// Returns the current time given by |OfflineClock|. This used as a shortcut
// for calls to |OfflineClock()->Now()|
base::Time OfflineTimeNow();

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_CLOCK_H_
