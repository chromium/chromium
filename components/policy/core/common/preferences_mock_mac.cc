// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/preferences_mock_mac.h"

MockPreferences::MockPreferences() {
  values_.reset(CFDictionaryCreateMutable(kCFAllocatorDefault,
                                          0,
                                          &kCFTypeDictionaryKeyCallBacks,
                                          &kCFTypeDictionaryValueCallBacks));
  forced_.reset(CFSetCreateMutable(kCFAllocatorDefault,
                                   0,
                                   &kCFTypeSetCallBacks));
  machine_.reset(
      CFSetCreateMutable(kCFAllocatorDefault, 0, &kCFTypeSetCallBacks));
}

MockPreferences::~MockPreferences() {
}

Boolean MockPreferences::AppSynchronize(CFStringRef application_id) {
  return true;
}

CFPropertyListRef MockPreferences::CopyAppValue(CFStringRef key,
                                                CFStringRef application_id) {
  CFPropertyListRef value;
  Boolean found = CFDictionaryGetValueIfPresent(values_.get(), key, &value);
  if (!found || !value)
    return NULL;
  CFRetain(value);
  return value;
}

Boolean MockPreferences::AppValueIsForced(CFStringRef key,
                                          CFStringRef application_id) {
  return CFSetContainsValue(forced_.get(), key);
}

Boolean MockPreferences::IsManagedPolicyAvailableForMachineScope(
    CFStringRef key) {
  return CFSetContainsValue(machine_.get(), key);
}

void MockPreferences::AddTestItem(CFStringRef key,
                                  CFPropertyListRef value,
                                  bool is_forced,
                                  bool is_machine) {
  CFDictionarySetValue(values_.get(), key, value);
  if (is_forced)
    CFSetAddValue(forced_.get(), key);
  if (is_machine)
    CFSetAddValue(machine_.get(), key);
}
