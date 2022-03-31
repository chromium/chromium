// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_helper_client_mac.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/updater/mac/privileged_helper/service_protocol.h"

namespace {
const int kPrivilegedHelperConnectionFailed = -10000;
}

BrowserUpdaterHelperClientMac::BrowserUpdaterHelperClientMac()
    : main_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  xpc_connection_.reset([[NSXPCConnection alloc]
      initWithMachServiceName:base::SysUTF8ToNSString(kPrivilegedHelperName)
                      options:NSXPCConnectionPrivileged]);

  xpc_connection_.get().remoteObjectInterface = [NSXPCInterface
      interfaceWithProtocol:@protocol(PrivilegedHelperServiceProtocol)];

  xpc_connection_.get().interruptionHandler = ^{
    LOG(WARNING)
        << "PrivilegedHelperServiceProtocolImpl: XPC connection interrupted.";
  };

  xpc_connection_.get().invalidationHandler = ^{
    LOG(WARNING)
        << "PrivilegedHelperServiceProtocolImpl: XPC connection invalidated.";
  };

  [xpc_connection_ resume];
}

BrowserUpdaterHelperClientMac::~BrowserUpdaterHelperClientMac() {
  [xpc_connection_ invalidate];
  xpc_connection_.reset();
}

void BrowserUpdaterHelperClientMac::SetupSystemUpdater(
    base::OnceCallback<void(int)> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  __block base::OnceCallback<void(int)> block_callback = std::move(result);

  auto reply = ^(int error) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(block_callback), error));
  };

  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC Connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(kPrivilegedHelperConnectionFailed);
  };

  [[xpc_connection_ remoteObjectProxyWithErrorHandler:errorHandler]
      setupSystemUpdaterWithBrowserPath:base::mac::FilePathToNSString(
                                            base::mac::OuterBundlePath())
                                  reply:reply];
}
