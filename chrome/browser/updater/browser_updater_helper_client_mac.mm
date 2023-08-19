// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_helper_client_mac.h"

#import <Foundation/Foundation.h>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/updater/mac/privileged_helper/service_protocol.h"

namespace {
const int kPrivilegedHelperConnectionFailed = -10000;
}

BrowserUpdaterHelperClientMac::BrowserUpdaterHelperClientMac() {
  xpc_connection_ = [[NSXPCConnection alloc]
      initWithMachServiceName:base::SysUTF8ToNSString(kPrivilegedHelperName)
                      options:NSXPCConnectionPrivileged];

  xpc_connection_.remoteObjectInterface = [NSXPCInterface
      interfaceWithProtocol:@protocol(PrivilegedHelperServiceProtocol)];

  xpc_connection_.interruptionHandler = ^{
    LOG(WARNING)
        << "PrivilegedHelperServiceProtocolImpl: XPC connection interrupted.";
  };

  xpc_connection_.invalidationHandler = ^{
    LOG(WARNING)
        << "PrivilegedHelperServiceProtocolImpl: XPC connection invalidated.";
  };

  [xpc_connection_ resume];
}

BrowserUpdaterHelperClientMac::~BrowserUpdaterHelperClientMac() {
  [xpc_connection_ invalidate];
  xpc_connection_ = nil;
}

void BrowserUpdaterHelperClientMac::SetupSystemUpdater(
    base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  __block base::OnceCallback<void(int)> block_callback =
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&BrowserUpdaterHelperClientMac::SetupSystemUpdaterDone,
                         base::WrapRefCounted(this), std::move(callback)));

  auto reply = ^(int error) {
    std::move(block_callback).Run(error);
  };

  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC Connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(kPrivilegedHelperConnectionFailed);
  };

  [[xpc_connection_ remoteObjectProxyWithErrorHandler:errorHandler]
      setupSystemUpdaterWithBrowserPath:base::apple::FilePathToNSString(
                                            base::apple::OuterBundlePath())
                                  reply:reply];
}

void BrowserUpdaterHelperClientMac::SetupSystemUpdaterDone(
    base::OnceCallback<void(int)> callback,
    int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(result);
}
