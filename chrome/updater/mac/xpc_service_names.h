// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_XPC_SERVICE_NAMES_H_
#define CHROME_UPDATER_MAC_XPC_SERVICE_NAMES_H_

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"

namespace updater {

extern const char kUpdateServiceInternalLaunchdName[];
extern const char kUpdateServiceLaunchdName[];

base::ScopedCFTypeRef<CFStringRef> CopyUpdateServiceLaunchdName();
base::ScopedCFTypeRef<CFStringRef> CopyWakeLaunchdName();
base::ScopedCFTypeRef<CFStringRef> CopyUpdateServiceInternalLaunchdName();
base::scoped_nsobject<NSString> GetUpdateServiceLaunchdLabel();
base::scoped_nsobject<NSString> GetWakeLaunchdLabel();
base::scoped_nsobject<NSString> GetUpdateServiceInternalLaunchdLabel();
base::scoped_nsobject<NSString> GetUpdateServiceMachName();
base::scoped_nsobject<NSString> GetUpdateServiceInternalMachName();

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_XPC_SERVICE_NAMES_H_
