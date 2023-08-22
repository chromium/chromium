// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_PREFERENCES_MOCK_MAC_H_
#define COMPONENTS_POLICY_CORE_COMMON_PREFERENCES_MOCK_MAC_H_

#include "base/apple/scoped_cftyperef.h"
#include "components/policy/core/common/preferences_mac.h"
#include "components/policy/policy_export.h"

// Mock preferences wrapper for testing code that interacts with CFPreferences.
class POLICY_EXPORT MockPreferences : public MacPreferences {
 public:
  MockPreferences();
  ~MockPreferences() override;

  // MacPreferences
  Boolean AppSynchronize(CFStringRef application_id) override;
  CFPropertyListRef CopyAppValue(CFStringRef key,
                                 CFStringRef application_id) override;
  Boolean AppValueIsForced(CFStringRef key,
                           CFStringRef application_id) override;
  Boolean IsManagedPolicyAvailableForMachineScope(CFStringRef key) override;

  // Adds a preference item with the given info to the test set.
  void AddTestItem(CFStringRef key,
                   CFPropertyListRef value,
                   bool is_forced,
                   bool is_machine);

 private:
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> values_;
  base::apple::ScopedCFTypeRef<CFMutableSetRef> forced_;
  base::apple::ScopedCFTypeRef<CFMutableSetRef> machine_;
};

#endif  // COMPONENTS_POLICY_CORE_COMMON_PREFERENCES_MOCK_MAC_H_
