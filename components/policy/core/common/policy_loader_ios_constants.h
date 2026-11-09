// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_IOS_CONSTANTS_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_IOS_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Key in NSUserDefaults that contains the managed app configuration.
extern NSString* const kPolicyLoaderIOSConfigurationKey;

// Key in the managed app configuration whose value is the device name
// configured by the MDM administrator. This entry is consumed directly by
// `policy::GetIOSManagedDeviceName()` and is not a policy, so it is ignored by
// `PolicyLoaderIOS`.
extern NSString* const kPolicyLoaderIOSDeviceNameKey;

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_IOS_CONSTANTS_H_
