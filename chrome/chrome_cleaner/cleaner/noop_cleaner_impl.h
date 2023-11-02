// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_CLEANER_NOOP_CLEANER_IMPL_H_
#define CHROME_CHROME_CLEANER_CLEANER_NOOP_CLEANER_IMPL_H_

#include <vector>

#include "chrome/chrome_cleaner/cleaner/cleaner.h"

namespace chrome_cleaner {

// A cleaner that does nothing. This should only be used with engines that do
// not support any removable UwS, so it asserts that cleaning-related functions
// are not called.
class NoopCleanerImpl : public Cleaner {
 public:
  NoopCleanerImpl();
  ~NoopCleanerImpl() override;

  // Cleaner.
  void Start(const std::vector<UwSId>& uws_ids,
             DoneCallback done_callback) override;
  void StartPostReboot(const std::vector<UwSId>& uws_ids,
                       DoneCallback done_callback) override;
  void Stop() override;
  bool IsCompletelyDone() const override;
  bool CanClean(const std::vector<UwSId>& uws_ids) override;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_CLEANER_NOOP_CLEANER_IMPL_H_
