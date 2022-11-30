// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/screentime/history_deleter_impl.h"

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "net/base/mac/url_conversions.h"

//#import <ScreenTime/ScreenTime.h>

namespace screentime {

HistoryDeleterImpl::~HistoryDeleterImpl() = default;

std::unique_ptr<HistoryDeleterImpl> HistoryDeleterImpl::Create() {
  if (@available(macOS 12.1, *))
    return base::WrapUnique(new HistoryDeleterImpl);
  return nullptr;
}

void HistoryDeleterImpl::DeleteAllHistory() {
<<<<<<< HEAD
  if (@available(macOS 11.0, *)) {
    //[platform_deleter_ deleteAllHistory];
||||||| 80c960997e61f
  if (@available(macOS 11.0, *)) {
    [platform_deleter_ deleteAllHistory];
=======
  if (@available(macOS 12.1, *)) {
    [platform_deleter_ deleteAllHistory];
>>>>>>> 27d3765d341b09369006d030f83f582a29eb57ae
  } else {
    NOTIMPLEMENTED();
  }
}

void HistoryDeleterImpl::DeleteHistoryDuringInterval(
    const TimeInterval& interval) {
  if (@available(macOS 12.1, *)) {
    base::scoped_nsobject<NSDateInterval> nsinterval([[NSDateInterval alloc]
        initWithStartDate:interval.first.ToNSDate()
                  endDate:interval.second.ToNSDate()]);
    //[platform_deleter_ deleteHistoryDuringInterval:nsinterval.get()];
  } else {
    NOTIMPLEMENTED();
  }
}

void HistoryDeleterImpl::DeleteHistoryForURL(const GURL& url) {
<<<<<<< HEAD
  if (@available(macOS 11.0, *)) {
    //[platform_deleter_ deleteHistoryForURL:net::NSURLWithGURL(url)];
||||||| 80c960997e61f
  if (@available(macOS 11.0, *)) {
    [platform_deleter_ deleteHistoryForURL:net::NSURLWithGURL(url)];
=======
  if (@available(macOS 12.1, *)) {
    [platform_deleter_ deleteHistoryForURL:net::NSURLWithGURL(url)];
>>>>>>> 27d3765d341b09369006d030f83f582a29eb57ae
  } else {
    NOTIMPLEMENTED();
  }
}

HistoryDeleterImpl::HistoryDeleterImpl() {
<<<<<<< HEAD
  /*
  if (@available(macOS 11.0, *)) {
||||||| 80c960997e61f
  if (@available(macOS 11.0, *)) {
=======
  if (@available(macOS 12.1, *)) {
>>>>>>> 27d3765d341b09369006d030f83f582a29eb57ae
    NSError* error = nil;
    NSString* bundle_id = base::SysUTF8ToNSString(base::mac::BaseBundleID());
    platform_deleter_.reset(
        [[STWebHistory alloc] initWithBundleIdentifier:bundle_id error:&error]);
    DCHECK(!error);
  } else {
    NOTIMPLEMENTED();
  }
  */
}

}  // namespace screentime
