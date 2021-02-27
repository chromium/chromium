// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_MOCK_PREF_CHANGE_CALLBACK_H_
#define COMPONENTS_PREFS_MOCK_PREF_CHANGE_CALLBACK_H_

#include <string>

#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Pointee;
using testing::Property;
using testing::Truly;

// Matcher that checks whether the current value of the preference named
// |pref_name| in |prefs| matches |value|. If |value| is NULL, the matcher
// checks that the value is not set.
MATCHER_P3(PrefValueMatches, prefs, pref_name, value, "") {
  const PrefService::Preference* pref = prefs->FindPreference(pref_name);
  if (!pref)
    return false;

  const base::Value* actual_value = pref->GetValue();
  if (!actual_value)
    return value == NULL;
  if (!value)
    return actual_value == NULL;
  return value->Equals(actual_value);
}

// A mock for testing preference notifications and easy setup of expectations.
class MockPrefChangeCallback {
 public:
  explicit MockPrefChangeCallback(PrefService* prefs);
  virtual ~MockPrefChangeCallback();

  PrefChangeRegistrar::NamedChangeCallback GetCallback();

  MOCK_METHOD1(OnPreferenceChanged, void(const std::string&));

  void Expect(const std::string& pref_name,
              const base::Value* value);

 private:
  PrefService* prefs_;
};

#endif  // COMPONENTS_PREFS_MOCK_PREF_CHANGE_CALLBACK_H_
