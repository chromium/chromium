// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/crash_key.h"

#include <dispatch/dispatch.h>

#include "base/strings/sys_string_conversions.h"
#include "components/crash/core/common/crash_key_base_support.h"
#import "third_party/breakpad/breakpad/src/client/ios/Breakpad.h"
#import "third_party/breakpad/breakpad/src/client/ios/BreakpadController.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// The iOS Breakpad implementation internally uses a LongStringDictionary,
// which performs the same chunking done by crash_key_breakpad.cc. This class
// implementation therefore just wraps the iOS Breakpad interface.

namespace crash_reporter {
namespace internal {

namespace {

// Accessing the BreakpadRef is done on an async queue, so serialize the
// access to the current thread, as the CrashKeyString API is sync. This
// matches //ios/chrome/browser/crash_report/breakpad_helper.mm.
// When getting a value, wait until the value is received.
// Note: This will block the current thread.
void WithBreakpadRefSync(void (^block)(BreakpadRef ref)) {
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

  [[BreakpadController sharedInstance] withBreakpadRef:^(BreakpadRef ref) {
    block(ref);
    dispatch_semaphore_signal(semaphore);
  }];
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
}

// When setting a value, avoid to block the current thread.
void WithBreakpadRefAsync(void (^block)(BreakpadRef ref)) {
  [[BreakpadController sharedInstance] withBreakpadRef:^(BreakpadRef ref) {
    block(ref);
  }];
}

}  // namespace

void CrashKeyStringImpl::Set(base::StringPiece value) {
  NSString* key = base::SysUTF8ToNSString(name_);
  NSString* value_ns = base::SysUTF8ToNSString(value.as_string());

  WithBreakpadRefAsync(^(BreakpadRef ref) {
    BreakpadAddUploadParameter(ref, key, value_ns);
  });
}

void CrashKeyStringImpl::Clear() {
  NSString* key = base::SysUTF8ToNSString(name_);

  WithBreakpadRefAsync(^(BreakpadRef ref) {
    BreakpadRemoveUploadParameter(ref, key);
  });
}

bool CrashKeyStringImpl::is_set() const {
  __block bool is_set = false;
  NSString* key = base::SysUTF8ToNSString(
      std::string(BREAKPAD_SERVER_PARAMETER_PREFIX) + name_);

  WithBreakpadRefSync(^(BreakpadRef ref) {
    is_set = BreakpadKeyValue(ref, key) != nil;
  });

  return is_set;
}

}  // namespace internal

void InitializeCrashKeys() {
  InitializeCrashKeyBaseSupport();
}

std::string GetCrashKeyValue(const std::string& key_name) {
  __block NSString* value;
  NSString* key = base::SysUTF8ToNSString(
      std::string(BREAKPAD_SERVER_PARAMETER_PREFIX) + key_name);

  internal::WithBreakpadRefSync(^(BreakpadRef ref) {
    value = BreakpadKeyValue(ref, key);
  });

  return base::SysNSStringToUTF8(value);
}

void InitializeCrashKeysForTesting() {
  [[BreakpadController sharedInstance] updateConfiguration:@{
    @BREAKPAD_URL : @"http://breakpad.test"
  }];
  [[BreakpadController sharedInstance] start:YES];
  InitializeCrashKeys();
}

void ResetCrashKeysForTesting() {
  [[BreakpadController sharedInstance] stop];
}

}  // namespace crash_reporter
