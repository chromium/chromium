// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATER_BROWSER_UPDATER_HELPER_CLIENT_MAC_H_
#define CHROME_BROWSER_UPDATER_BROWSER_UPDATER_HELPER_CLIENT_MAC_H_

#import <Foundation/Foundation.h>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/updater/mac/privileged_helper/service_protocol.h"

// Client that will create a connection between the browser and the privileged
// helper for the Chromium updater. Helps with setting up the system-level
// updater during promotion.
class BrowserUpdaterHelperClientMac
    : public base::RefCountedThreadSafe<BrowserUpdaterHelperClientMac> {
 public:
  BrowserUpdaterHelperClientMac();

  // Calls on the privileged helper to set up the system-level updater. Upon
  // setup completion, an integer return code will be sent back in a callback.
  // `callback` is called on the same sequence. A ref to the
  // BrowserUpdaterHelperClientMac is held throughout the operation.
  void SetupSystemUpdater(base::OnceCallback<void(int)> callback);

 protected:
  friend class base::RefCountedThreadSafe<BrowserUpdaterHelperClientMac>;
  virtual ~BrowserUpdaterHelperClientMac();

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  NSXPCConnection* __strong xpc_connection_;

  void SetupSystemUpdaterDone(base::OnceCallback<void(int)> callback,
                              int result);
};

#endif  // CHROME_BROWSER_UPDATER_BROWSER_UPDATER_HELPER_CLIENT_MAC_H_
