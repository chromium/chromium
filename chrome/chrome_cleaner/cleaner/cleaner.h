// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_CLEANER_CLEANER_H_
#define CHROME_CHROME_CLEANER_CLEANER_CLEANER_H_

#include <vector>

#include "base/functional/callback.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

// This class defines the cleaning interface that is used for all engines.
class Cleaner {
 public:
  typedef base::OnceCallback<void(ResultCode status)> DoneCallback;

  virtual ~Cleaner() = default;

  // Start cleaning UwS specified in |uws_ids|. If the cleanup completes before
  // |Stop| is called, |done_callback| must be called to report cleanup status.
  virtual void Start(const std::vector<UwSId>& uws_ids,
                     DoneCallback done_callback) = 0;

  // Start post-reboot cleanup of UwS specified in |uws_ids|. |done_callback|
  // must be called to report cleanup status.
  virtual void StartPostReboot(const std::vector<UwSId>& uws_ids,
                               DoneCallback done_callback) = 0;

  // Interrupts the current cleaning. It's a noop when cleaning hasn't started
  // yet, is already done, or has already been stopped.
  virtual void Stop() = 0;

  // When calling |Stop|, some tasks may still be running, so make sure to call
  // |IsCompletelyDone| and allow the main message loop to pump messages until
  // all tasks are done before clearing data passed to the cleaner.
  virtual bool IsCompletelyDone() const = 0;

  // Indicate whether the cleaner can remove UwS specified in |uws_ids|.
  virtual bool CanClean(const std::vector<UwSId>& uws_ids) = 0;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_CLEANER_CLEANER_H_
