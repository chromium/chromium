// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SCREENTIME_HISTORY_DELETER_H_
#define CHROME_BROWSER_UI_COCOA_SCREENTIME_HISTORY_DELETER_H_

#include <utility>

#include "base/time/time.h"

class GURL;

namespace screentime {

// The HistoryDeleter interface wraps the actual implementation of deleting
// items from the system ScreenTime history store, so the interface here exactly
// mirrors the ScreenTime STWebHistory interface:
//   https://developer.apple.com/documentation/screentime/stwebhistory
class HistoryDeleter {
 public:
  using TimeInterval = std::pair<base::Time, base::Time>;

  virtual ~HistoryDeleter() = default;

  virtual void DeleteAllHistory() = 0;
  virtual void DeleteHistoryDuringInterval(const TimeInterval& interval) = 0;
  virtual void DeleteHistoryForURL(const GURL& url) = 0;
};

}  // namespace screentime

#endif  // CHROME_BROWSER_UI_COCOA_SCREENTIME_HISTORY_DELETER_H_
