// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings_test_utils.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace {

const char kProxy[] = "proxy";

}  // namespace

namespace data_reduction_proxy {

DataReductionProxySettingsTestBase::DataReductionProxySettingsTestBase() {}

DataReductionProxySettingsTestBase::~DataReductionProxySettingsTestBase() {}

// testing::Test implementation:
void DataReductionProxySettingsTestBase::SetUp() {
  test_context_ =
      DataReductionProxyTestContext::Builder()
          .WithMockConfig()
          .WithMockDataReductionProxyService()
          .SkipSettingsInitialization()
          .Build();

  test_context_->SetDataReductionProxyEnabled(false);
  TestingPrefServiceSimple* pref_service = test_context_->pref_service();
  pref_service->SetInt64(prefs::kDailyHttpContentLengthLastUpdateDate, 0L);
  pref_service->registry()->RegisterDictionaryPref(kProxy);
  pref_service->SetBoolean(prefs::kDataReductionProxyWasEnabledBefore, false);

  ResetSettings(nullptr);

  ListPrefUpdate original_update(test_context_->pref_service(),
                                 prefs::kDailyHttpOriginalContentLength);
  ListPrefUpdate received_update(test_context_->pref_service(),
                                 prefs::kDailyHttpReceivedContentLength);
  for (int64_t i = 0; i < kNumDaysInHistory; i++) {
    original_update->Insert(
        0, std::make_unique<base::Value>(base::NumberToString(2 * i)));
    received_update->Insert(
        0, std::make_unique<base::Value>(base::NumberToString(i)));
  }
  last_update_time_ = base::Time::Now().LocalMidnight();
  pref_service->SetInt64(prefs::kDailyHttpContentLengthLastUpdateDate,
                         last_update_time_.ToInternalValue());
}

template <class C>
void DataReductionProxySettingsTestBase::ResetSettings(base::Clock* clock) {
  MockDataReductionProxySettings<C>* settings =
      new MockDataReductionProxySettings<C>();
  if (settings_) {
    settings->data_reduction_proxy_service_ =
        std::move(settings_->data_reduction_proxy_service_);
  } else {
    settings->data_reduction_proxy_service_ = test_context_->TakeService();
  }
  settings->data_reduction_proxy_service_->SetSettingsForTesting(settings);
  settings->config_ = test_context_->config();
  settings->prefs_ = test_context_->pref_service();
  if (clock)
    settings->clock_ = clock;
  EXPECT_CALL(*settings, GetOriginalProfilePrefs())
      .Times(AnyNumber())
      .WillRepeatedly(Return(test_context_->pref_service()));
  EXPECT_CALL(*settings, GetLocalStatePrefs())
      .Times(AnyNumber())
      .WillRepeatedly(Return(test_context_->pref_service()));
  settings_.reset(settings);
}

// Explicitly generate required instantiations.
template void DataReductionProxySettingsTestBase::ResetSettings<
    DataReductionProxySettings>(base::Clock* clock);

void DataReductionProxySettingsTestBase::ExpectSetProxyPrefs(
    bool expected_enabled,
    bool expected_at_startup) {
  MockDataReductionProxyService* mock_service =
      static_cast<MockDataReductionProxyService*>(
          settings_->data_reduction_proxy_service());
  EXPECT_CALL(*mock_service,
              SetProxyPrefs(expected_enabled, expected_at_startup));
}

void DataReductionProxySettingsTestBase::CheckOnPrefChange(
    bool enabled,
    bool expected_enabled,
    bool managed) {
  ExpectSetProxyPrefs(expected_enabled, false);
  if (managed) {
    test_context_->pref_service()->SetManagedPref(
        prefs::kDataSaverEnabled, std::make_unique<base::Value>(enabled));
  } else {
    test_context_->SetDataReductionProxyEnabled(enabled);
  }
  test_context_->RunUntilIdle();
  // Never expect the proxy to be restricted for pref change tests.
}

void DataReductionProxySettingsTestBase::InitDataReductionProxy(
    bool enabled_at_startup) {
  settings_->InitDataReductionProxySettings(
      test_context_->pref_service(),
      std::move(settings_->data_reduction_proxy_service_));
  settings_->SetCallbackToRegisterSyntheticFieldTrial(base::Bind(
      &DataReductionProxySettingsTestBase::OnSyntheticFieldTrialRegistration,
      base::Unretained(this)));

  test_context_->RunUntilIdle();
}

void DataReductionProxySettingsTestBase::CheckDataReductionProxySyntheticTrial(
    bool enabled) {
  EXPECT_EQ(enabled ? "Enabled" : "Disabled",
      synthetic_field_trials_["SyntheticDataReductionProxySetting"]);
}

bool DataReductionProxySettingsTestBase::OnSyntheticFieldTrialRegistration(
    base::StringPiece trial_name,
    base::StringPiece group_name) {
  synthetic_field_trials_[trial_name.as_string()] = group_name.as_string();
  return true;
}

}  // namespace data_reduction_proxy
