// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_TEST_UTILS_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/message_loop/message_pump_type.h"
#include "base/strings/string_piece.h"
#include "base/task/single_thread_task_executor.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrefService;

namespace data_reduction_proxy {

class DataReductionProxyTestContext;

template <class C>
class MockDataReductionProxySettings : public C {
 public:
  MockDataReductionProxySettings<C>() : C(false) {}
  MOCK_CONST_METHOD0(GetOriginalProfilePrefs, PrefService*());
  MOCK_METHOD0(GetLocalStatePrefs, PrefService*());
  MOCK_CONST_METHOD1(RecordStartupState, void(ProxyStartupState state));
};

class DataReductionProxySettingsTestBase : public testing::Test {
 public:
  static void AddTestProxyToCommandLine();

  DataReductionProxySettingsTestBase();
  ~DataReductionProxySettingsTestBase() override;

  void AddProxyToCommandLine();

  void SetUp() override;

  template <class C>
  void ResetSettings(base::Clock* clock);
  virtual void ResetSettings(base::Clock* clock) = 0;

  void ExpectSetProxyPrefs(bool expected_enabled,
                           bool expected_at_startup);

  void CheckMaybeActivateDataReductionProxy(bool initially_enabled,
                                            bool request_succeeded,
                                            bool expected_enabled,
                                            bool expected_restricted,
                                            bool expected_fallback_restricted);
  void CheckOnPrefChange(bool enabled, bool expected_enabled, bool managed);
  void InitWithStatisticsPrefs();
  void InitDataReductionProxy(bool enabled_at_startup);
  void CheckDataReductionProxySyntheticTrial(bool enabled);
  bool OnSyntheticFieldTrialRegistration(base::StringPiece trial_name,
                                         base::StringPiece group_name);

 protected:
  base::SingleThreadTaskExecutor io_task_executor_{base::MessagePumpType::IO};
  std::unique_ptr<DataReductionProxyTestContext> test_context_;
  std::unique_ptr<DataReductionProxySettings> settings_;
  base::Time last_update_time_;
  std::map<std::string, std::string> synthetic_field_trials_;
};

// Test implementations should be subclasses of an instantiation of this
// class parameterized for whatever DataReductionProxySettings class
// is being tested.
template <class C>
class ConcreteDataReductionProxySettingsTest
    : public DataReductionProxySettingsTestBase {
 public:
  typedef MockDataReductionProxySettings<C> MockSettings;
  void ResetSettings(base::Clock* clock) override {
    return DataReductionProxySettingsTestBase::ResetSettings<C>(clock);
  }
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_TEST_UTILS_H_
