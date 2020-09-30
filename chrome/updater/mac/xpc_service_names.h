// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_XPC_SERVICE_NAMES_H_
#define CHROME_UPDATER_MAC_XPC_SERVICE_NAMES_H_

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"

namespace updater {

extern const char kControlLaunchdName[];

base::ScopedCFTypeRef<CFStringRef> CopyServiceLaunchdName();
base::ScopedCFTypeRef<CFStringRef> CopyWakeLaunchdName();
base::ScopedCFTypeRef<CFStringRef> CopyControlLaunchdName();
base::scoped_nsobject<NSString> GetServiceLaunchdLabel();
base::scoped_nsobject<NSString> GetWakeLaunchdLabel();
base::scoped_nsobject<NSString> GetControlLaunchdLabel();
base::scoped_nsobject<NSString> GetServiceMachName();
base::scoped_nsobject<NSString> GetVersionedServiceMachName();

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_XPC_SERVICE_NAMES_H_
