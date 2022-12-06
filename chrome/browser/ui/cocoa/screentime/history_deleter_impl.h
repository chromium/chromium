// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SCREENTIME_HISTORY_DELETER_IMPL_H_
#define CHROME_BROWSER_UI_COCOA_SCREENTIME_HISTORY_DELETER_IMPL_H_

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/screentime/history_deleter.h"

@class STWebHistory;

namespace screentime {

// Implementation of HistoryDeleter that mutates the actual system history
// store.
class HistoryDeleterImpl : public HistoryDeleter {
 public:
  ~HistoryDeleterImpl() override;

  // The constructor is private so that the actual construction of this object
  // can be guarded by availability checks inside this class rather than in
  // callers. This method may return nullptr if called on a system where
  // ScreenTime is not available!
  static std::unique_ptr<HistoryDeleterImpl> Create();

  void DeleteAllHistory() override;
  void DeleteHistoryDuringInterval(const TimeInterval& interval) override;
  void DeleteHistoryForURL(const GURL& url) override;

 private:
  HistoryDeleterImpl();

  base::scoped_nsobject<STWebHistory> platform_deleter_;
};

}  // namespace screentime

#endif  // CHROME_BROWSER_UI_COCOA_SCREENTIME_HISTORY_DELETER_IMPL_H_
