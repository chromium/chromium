// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_SCANNER_SCANNER_H_
#define CHROME_CHROME_CLEANER_SCANNER_SCANNER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

// This class defines the scanning interface and is used as a base for the
// scanners for different engines.
class Scanner {
 public:
  // The type of callback to be called when an UwS is detected.
  typedef base::RepeatingCallback<void(UwSId found_uws)> FoundUwSCallback;

  // The type of callback that is called when the scan completes.
  // |status| should contain RESULT_CODE_SUCCESS if no failures were encountered
  // or error status code otherwise.
  typedef base::OnceCallback<void(ResultCode status,
                                  const std::vector<UwSId>& found_pups)>
      DoneCallback;

  virtual ~Scanner() {}

  // Start scanning for UwS. When an UwS is detected and if |Stop| has not been
  // called, |found_uws_callback| is called. If the scan completes before
  // |Stop| is called, then |done_callback| is called. Returns true if startup
  // succeeded. If the startup fails, |done_callback| will be called with the
  // failure code.
  virtual bool Start(const FoundUwSCallback& found_uws_callback,
                     const DoneCallback done_callback) = 0;

  // Interrupts the current scanning. It's a noop when scanning has not started
  // or is already done, or has already been stopped.
  virtual void Stop() = 0;

  // When calling |Stop|, some tasks may still be running, so make sure to call
  // |IsCompletelyDone| and allow the main UI to pump messages to let the task
  // tracker mark all tasks as done before clearing data passed to the scanner.
  virtual bool IsCompletelyDone() const = 0;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_SCANNER_SCANNER_H_
