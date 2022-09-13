// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_TEST_UTILS_H_
#define COMPONENTS_PREFS_PREF_TEST_UTILS_H_

#include <string>

class PrefService;
namespace base {
class Value;
}

// Spins a RunLoop until the preference at |path| has value |value|.
void WaitForPrefValue(PrefService* pref_service,
                      const std::string& path,
                      const base::Value& value);

#endif  // COMPONENTS_PREFS_PREF_TEST_UTILS_H_
