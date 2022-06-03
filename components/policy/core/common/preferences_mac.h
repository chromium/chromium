// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_PREFERENCES_MAC_H_
#define COMPONENTS_POLICY_CORE_COMMON_PREFERENCES_MAC_H_

#include <CoreFoundation/CoreFoundation.h>

#include "components/policy/policy_export.h"

// Wraps a small part of the CFPreferences API surface in a very thin layer, to
// allow it to be mocked out for testing.

// See CFPreferences documentation for function documentation, as these call
// through directly to their CFPreferences equivalents (Foo ->
// CFPreferencesFoo).
class POLICY_EXPORT MacPreferences {
 public:
  MacPreferences() {}
  MacPreferences(const MacPreferences&) = delete;
  MacPreferences& operator=(const MacPreferences&) = delete;
  virtual ~MacPreferences() {}

  virtual Boolean AppSynchronize(CFStringRef applicationID);

  virtual CFPropertyListRef CopyAppValue(CFStringRef key,
                                         CFStringRef applicationID);

  virtual Boolean AppValueIsForced(CFStringRef key, CFStringRef applicationID);
};

#endif  // COMPONENTS_POLICY_CORE_COMMON_PREFERENCES_MAC_H_
