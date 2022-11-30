// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_MOCK_PREF_CHANGE_CALLBACK_H_
#define COMPONENTS_PREFS_MOCK_PREF_CHANGE_CALLBACK_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/values_equivalent.h"
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

  return base::ValuesEquivalent(value, pref->GetValue());
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
  raw_ptr<PrefService> prefs_;
};

#endif  // COMPONENTS_PREFS_MOCK_PREF_CHANGE_CALLBACK_H_
