// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"

#include <stdint.h>

#include "base/strings/string_number_conversions.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

class DataReductionProxyPrefsTest : public testing::Test {
 public:
  void SetUp() override {
    RegisterPrefs(local_state_prefs_.registry());
    PrefRegistrySimple* profile_registry = profile_prefs_.registry();
    RegisterPrefs(profile_registry);
  }

  PrefService* local_state_prefs() {
    return &local_state_prefs_;
  }

  PrefService* profile_prefs() {
    return &profile_prefs_;
  }

  // Initializes a list with ten string representations of successive int64_t
  // values, starting with |starting_value|.
  void InitializeList(const char* pref_name,
                      int64_t starting_value,
                      PrefService* pref_service) {
    ListPrefUpdate list(local_state_prefs(), pref_name);
    for (int64_t i = 0; i < 10L; ++i) {
      list->AppendString(base::NumberToString(i + starting_value));
    }
  }

  // Verifies that ten string repreentations of successive int64_t values
  // starting with |starting_value| are found in the |ListValue| with the
  // associated |pref_name|.
  void VerifyList(const char* pref_name,
                  int64_t starting_value,
                  PrefService* pref_service) {
    const base::ListValue* list_value = pref_service->GetList(pref_name);
    for (int64_t i = 0; i < 10L; ++i) {
      std::string string_value;
      int64_t value;
      list_value->GetString(i, &string_value);
      base::StringToInt64(string_value, &value);
      EXPECT_EQ(i + starting_value, value);
    }
  }

 private:
  void RegisterPrefs(PrefRegistrySimple* registry) {
    registry->RegisterInt64Pref(prefs::kHttpReceivedContentLength, 0);
    registry->RegisterInt64Pref(prefs::kHttpOriginalContentLength, 0);

    registry->RegisterListPref(prefs::kDailyHttpOriginalContentLength);
    registry->RegisterListPref(prefs::kDailyHttpReceivedContentLength);
    registry->RegisterInt64Pref(
        prefs::kDailyHttpContentLengthLastUpdateDate, 0L);
  }

  TestingPrefServiceSimple local_state_prefs_;
  TestingPrefServiceSimple profile_prefs_;
};

}  // namespace data_reduction_proxy
