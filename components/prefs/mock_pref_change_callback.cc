// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/mock_pref_change_callback.h"

#include "base/functional/bind.h"

MockPrefChangeCallback::MockPrefChangeCallback(PrefService* prefs)
    : prefs_(prefs) {
}

MockPrefChangeCallback::~MockPrefChangeCallback() = default;

PrefChangeRegistrar::NamedChangeCallback MockPrefChangeCallback::GetCallback() {
  return base::BindRepeating(&MockPrefChangeCallback::OnPreferenceChanged,
                             base::Unretained(this));
}

void MockPrefChangeCallback::Expect(const std::string& pref_name,
                                    const base::Value* value) {
  EXPECT_CALL(*this, OnPreferenceChanged(pref_name))
      .With(PrefValueMatches(prefs_.get(), pref_name, value));
}
