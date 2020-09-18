// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MULTIDEVICE_STUB_MULTIDEVICE_UTIL_H_
#define CHROMEOS_COMPONENTS_MULTIDEVICE_STUB_MULTIDEVICE_UTIL_H_

#include "chromeos/components/multidevice/remote_device.h"

namespace chromeos {

namespace multidevice {

// Returns a fake host phone with all host features enabled. Can be used as a
// stub remote device to fake out multidevice features.
RemoteDevice CreateStubHostPhone();

// Returns a fake client computer with the client features enabled according to
// their corresponding flags. Can be used as a stub remote device to fake out
// multidevice features.
RemoteDevice CreateStubClientComputer();

// Returns if we should use stubbed implementations of multidevice features,
// i.e. when we are running a Linux CrOS build that doesn't support them.
bool ShouldUseMultideviceStubs();

}  // namespace multidevice

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MULTIDEVICE_STUB_MULTIDEVICE_UTIL_H_
