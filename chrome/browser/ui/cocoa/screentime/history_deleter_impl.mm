// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/screentime/history_deleter_impl.h"

#import <ScreenTime/ScreenTime.h>

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace screentime {

HistoryDeleterImpl::~HistoryDeleterImpl() = default;

std::unique_ptr<HistoryDeleterImpl> HistoryDeleterImpl::Create() {
  if (@available(macOS 12.1, *))
    return base::WrapUnique(new HistoryDeleterImpl);
  return nullptr;
}

void HistoryDeleterImpl::DeleteAllHistory() {
  if (@available(macOS 12.1, *)) {
    [platform_deleter_ deleteAllHistory];
  } else {
    NOTIMPLEMENTED();
  }
}

void HistoryDeleterImpl::DeleteHistoryDuringInterval(
    const TimeInterval& interval) {
  if (@available(macOS 12.1, *)) {
    NSDateInterval* nsinterval =
        [[NSDateInterval alloc] initWithStartDate:interval.first.ToNSDate()
                                          endDate:interval.second.ToNSDate()];
    [platform_deleter_ deleteHistoryDuringInterval:nsinterval];
  } else {
    NOTIMPLEMENTED();
  }
}

void HistoryDeleterImpl::DeleteHistoryForURL(const GURL& url) {
  if (@available(macOS 12.1, *)) {
    [platform_deleter_ deleteHistoryForURL:net::NSURLWithGURL(url)];
  } else {
    NOTIMPLEMENTED();
  }
}

HistoryDeleterImpl::HistoryDeleterImpl() {
  if (@available(macOS 12.1, *)) {
    NSError* error = nil;
    NSString* bundle_id = base::SysUTF8ToNSString(base::mac::BaseBundleID());
    platform_deleter_ = [[STWebHistory alloc] initWithBundleIdentifier:bundle_id
                                                                 error:&error];
    DCHECK(!error);
  } else {
    NOTIMPLEMENTED();
  }
}

}  // namespace screentime
