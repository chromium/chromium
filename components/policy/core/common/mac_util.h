// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_MAC_UTIL_H_
#define COMPONENTS_POLICY_CORE_COMMON_MAC_UTIL_H_

#include <CoreFoundation/CoreFoundation.h>

#include <memory>

#include "components/policy/policy_export.h"

// This file contains utilities shared by both Mac OS X and iOS.

namespace base {
class Value;
}

namespace policy {

// Converts a CFPropertyListRef to the equivalent base::Value. CFDictionary
// entries whose key is not a CFStringRef are ignored.
// Returns NULL if an invalid CFType was found, such as CFDate or CFData.
// NSDictionary is toll-free bridged to CFDictionaryRef, which is a
// CFPropertyListRef, so it can also be passed directly here. Same for the
// other NS* classes that map to CF* properties.
POLICY_EXPORT std::unique_ptr<base::Value> PropertyToValue(
    CFPropertyListRef property);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_MAC_UTIL_H_
