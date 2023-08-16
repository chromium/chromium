// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_SCOPED_BUNDLE_SWIZZLER_MAC_H_
#define CHROME_TEST_BASE_SCOPED_BUNDLE_SWIZZLER_MAC_H_

#include <memory>

namespace base::apple {
class ScopedObjCClassSwizzler;
}  // namespace base::apple

// Within a given scope, swizzles the implementation of +[NSBundle mainBundle]
// to return a partial mock of the original bundle. This partial mock has a
// custom bundle identifier.
// Since this class swizzles a class method, it doesn't make sense to have more
// than one instance of this class in existence at the same time.
// The primary purpose of this class is to stub out the behavior of NSBundle for
// AppKit, which makes assumptions about the functionality of +[NSBundle
// mainBundle]. Code in Chrome should not require the use of this method, since
// the NSBundle is always accessed through bundle_locations.h, and that NSBundle
// can be directly stubbed.
class ScopedBundleSwizzlerMac {
 public:
  ScopedBundleSwizzlerMac();
  ScopedBundleSwizzlerMac(const ScopedBundleSwizzlerMac&) = delete;
  ScopedBundleSwizzlerMac& operator=(const ScopedBundleSwizzlerMac&) = delete;
  ~ScopedBundleSwizzlerMac();

 private:
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler> class_swizzler_;
};

#endif  // CHROME_TEST_BASE_SCOPED_BUNDLE_SWIZZLER_MAC_H_
