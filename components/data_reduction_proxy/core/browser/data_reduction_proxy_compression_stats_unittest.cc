// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kWriteDelayMinutes = 60;

// Each bucket holds data usage for a 15 minute interval. History is maintained
// for 60 days.
const int kNumExpectedBuckets = 60 * 24 * 60 / 15;

int64_t GetListPrefInt64Value(const base::ListValue& list_update,
                              size_t index) {
  std::string string_value;
  EXPECT_TRUE(list_update.GetString(index, &string_value));

  int64_t value = 0;
  EXPECT_TRUE(base::StringToInt64(string_value, &value));
  return value;
}

class DataUsageLoadVerifier {
 public:
  DataUsageLoadVerifier(
      std::unique_ptr<std::vector<data_reduction_proxy::DataUsageBucket>>
          expected) {
    expected_ = std::move(expected);
  }

  void OnLoadDataUsage(
      std::unique_ptr<std::vector<data_reduction_proxy::DataUsageBucket>>
          actual) {
    EXPECT_EQ(expected_->size(), actual->size());

    // We are iterating through 2 vectors, |actual| and |expected|, so using an
    // index rather than an iterator.
    for (size_t i = 0; i < expected_->size(); ++i) {
      data_reduction_proxy::DataUsageBucket* actual_bucket = &(actual->at(i));
      data_reduction_proxy::DataUsageBucket* expected_bucket =
          &(expected_->at(i));
      EXPECT_EQ(expected_bucket->connection_usage_size(),
                actual_bucket->connection_usage_size());

      for (int j = 0; j < expected_bucket->connection_usage_size(); ++j) {
        data_reduction_proxy::PerConnectionDataUsage actual_connection_usage =
            actual_bucket->connection_usage(j);
        data_reduction_proxy::PerConnectionDataUsage expected_connection_usage =
            expected_bucket->connection_usage(j);

        EXPECT_EQ(expected_connection_usage.site_usage_size(),
                  actual_connection_usage.site_usage_size());

        for (auto expected_site_usage :
             expected_connection_usage.site_usage()) {
          data_reduction_proxy::PerSiteDataUsage actual_site_usage;
          for (auto it = actual_connection_usage.site_usage().begin();
               it != actual_connection_usage.site_usage().end(); ++it) {
            if (it->hostname() == expected_site_usage.hostname()) {
              actual_site_usage = *it;
            }
          }

          EXPECT_EQ(expected_site_usage.data_used(),
                    actual_site_usage.data_used());
          EXPECT_EQ(expected_site_usage.original_size(),
                    actual_site_usage.original_size());
        }
      }
    }
  }

 private:
  std::unique_ptr<std::vector<data_reduction_proxy::DataUsageBucket>> expected_;
};

}  // namespace

namespace data_reduction_proxy {

// The initial last update time used in test. There is no leap second a few
// days around this time used in the test.
// Note: No time zone is specified. Local time will be assumed by
// base::Time::FromString below.
const char kLastUpdateTime[] = "Wed, 18 Sep 2013 03:45:26";

class DataReductionProxyCompressionStatsTest : public testing::Test {
 protected:
  DataReductionProxyCompressionStatsTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {
    EXPECT_TRUE(base::Time::FromString(kLastUpdateTime, &now_));
  }

  void SetUp() override {
    drp_test_context_ = DataReductionProxyTestContext::Builder().Build();

    compression_stats_.reset(new DataReductionProxyCompressionStats(
        data_reduction_proxy_service(), pref_service(), base::TimeDelta()));
  }

  void ResetCompressionStatsWithDelay(const base::TimeDelta& delay) {
    compression_stats_.reset(new DataReductionProxyCompressionStats(
        data_reduction_proxy_service(), pref_service(), delay));
  }

  base::Time FakeNow() const {
    return now_ + now_delta_;
  }

  void SetFakeTimeDeltaInHours(int hours) {
    now_delta_ = base::TimeDelta::FromHours(hours);
  }

  void AddFakeTimeDeltaInHours(int hours) {
    now_delta_ += base::TimeDelta::FromHours(hours);
  }

  void SetUpPrefs() {
    CreatePrefList(prefs::kDailyHttpOriginalContentLength);
    CreatePrefList(prefs::kDailyHttpReceivedContentLength);

    const int64_t kOriginalLength = 150;
    const int64_t kReceivedLength = 100;

    compression_stats_->SetInt64(
        prefs::kHttpOriginalContentLength, kOriginalLength);
    compression_stats_->SetInt64(
        prefs::kHttpReceivedContentLength, kReceivedLength);

    base::ListValue* original_daily_content_length_list =
        compression_stats_->GetList(prefs::kDailyHttpOriginalContentLength);
    base::ListValue* received_daily_content_length_list =
        compression_stats_->GetList(prefs::kDailyHttpReceivedContentLength);

    for (size_t i = 0; i < kNumDaysInHistory; ++i) {
      original_daily_content_length_list->Set(
          i, std::make_unique<base::Value>(base::NumberToString(i)));
    }

    received_daily_content_length_list->Clear();
    for (size_t i = 0; i < kNumDaysInHistory / 2; ++i) {
      received_daily_content_length_list->AppendString(base::NumberToString(i));
    }
  }

  // Create daily pref list of |kNumDaysInHistory| zero values.
  void CreatePrefList(const char* pref) {
    base::ListValue* update = compression_stats_->GetList(pref);
    update->Clear();
    for (size_t i = 0; i < kNumDaysInHistory; ++i) {
      update->Insert(0, std::make_unique<base::Value>(base::Int64ToString(0)));
    }
  }

  // Verify the pref list values in |pref_service_| are equal to those in
  // |simple_pref_service| for |pref|.
  void VerifyPrefListWasWritten(const char* pref) {
    const base::ListValue* delayed_list = compression_stats_->GetList(pref);
    const base::ListValue* written_list = pref_service()->GetList(pref);
    ASSERT_EQ(delayed_list->GetSize(), written_list->GetSize());
    size_t count = delayed_list->GetSize();

    for (size_t i = 0; i < count; ++i) {
      EXPECT_EQ(GetListPrefInt64Value(*delayed_list, i),
                GetListPrefInt64Value(*written_list, i));
    }
  }

  // Verify the pref value in |pref_service_| are equal to that in
  // |simple_pref_service|.
  void VerifyPrefWasWritten(const char* pref) {
    int64_t delayed_pref = compression_stats_->GetInt64(pref);
    int64_t written_pref = pref_service()->GetInt64(pref);
    EXPECT_EQ(delayed_pref, written_pref);
  }

  // Verify the pref values in |dict| are equal to that in |compression_stats_|.
  void VerifyPrefs(base::DictionaryValue* dict) {
    base::string16 dict_pref_string;
    int64_t dict_pref;
    int64_t service_pref;

    dict->GetString("historic_original_content_length", &dict_pref_string);
    base::StringToInt64(dict_pref_string, &dict_pref);
    service_pref =
        compression_stats_->GetInt64(prefs::kHttpOriginalContentLength);
    EXPECT_EQ(service_pref, dict_pref);

    dict->GetString("historic_received_content_length", &dict_pref_string);
    base::StringToInt64(dict_pref_string, &dict_pref);
    service_pref =
        compression_stats_->GetInt64(prefs::kHttpReceivedContentLength);
    EXPECT_EQ(service_pref, dict_pref);
  }

  // Verify the pref list values are equal to the given values.
  // If the count of values is less than kNumDaysInHistory, zeros are assumed
  // at the beginning.
  void VerifyPrefList(const char* pref,
                      const int64_t* values,
                      size_t count,
                      size_t num_days_in_history) {
    ASSERT_GE(num_days_in_history, count);
    base::ListValue* update = compression_stats_->GetList(pref);
    ASSERT_EQ(num_days_in_history, update->GetSize()) << "Pref: " << pref;

    for (size_t i = 0; i < count; ++i) {
      EXPECT_EQ(values[i],
                GetListPrefInt64Value(*update, num_days_in_history - count + i))
          << pref << "; index=" << (num_days_in_history - count + i);
    }
    for (size_t i = 0; i < num_days_in_history - count; ++i) {
      EXPECT_EQ(0, GetListPrefInt64Value(*update, i)) << "index=" << i;
    }
  }

  // Verify that the pref value is equal to given value.
  void VerifyPrefInt64(const char* pref, const int64_t value) {
    EXPECT_EQ(value, compression_stats_->GetInt64(pref));
  }

  // Verify all daily data saving pref list values.
  void VerifyDailyDataSavingContentLengthPrefLists(
      const int64_t* original_values,
      size_t original_count,
      const int64_t* received_values,
      size_t received_count,
      const int64_t* original_with_data_reduction_proxy_enabled_values,
      size_t original_with_data_reduction_proxy_enabled_count,
      const int64_t* received_with_data_reduction_proxy_enabled_values,
      size_t received_with_data_reduction_proxy_count,
      const int64_t* original_via_data_reduction_proxy_values,
      size_t original_via_data_reduction_proxy_count,
      const int64_t* received_via_data_reduction_proxy_values,
      size_t received_via_data_reduction_proxy_count,
      size_t num_days_in_history) {
    VerifyPrefList(data_reduction_proxy::prefs::kDailyHttpOriginalContentLength,
                   original_values, original_count, num_days_in_history);
    VerifyPrefList(data_reduction_proxy::prefs::kDailyHttpReceivedContentLength,
                   received_values, received_count, num_days_in_history);
    VerifyPrefList(data_reduction_proxy::prefs::
                       kDailyOriginalContentLengthWithDataReductionProxyEnabled,
                   original_with_data_reduction_proxy_enabled_values,
                   original_with_data_reduction_proxy_enabled_count,
                   num_days_in_history);
    VerifyPrefList(data_reduction_proxy::prefs::
                       kDailyContentLengthWithDataReductionProxyEnabled,
                   received_with_data_reduction_proxy_enabled_values,
                   received_with_data_reduction_proxy_count,
                   num_days_in_history);
    VerifyPrefList(data_reduction_proxy::prefs::
                       kDailyOriginalContentLengthViaDataReductionProxy,
                   original_via_data_reduction_proxy_values,
                   original_via_data_reduction_proxy_count,
                   num_days_in_history);
    VerifyPrefList(
        data_reduction_proxy::prefs::kDailyContentLengthViaDataReductionProxy,
        received_via_data_reduction_proxy_values,
        received_via_data_reduction_proxy_count, num_days_in_history);

    VerifyPrefInt64(
        data_reduction_proxy::prefs::kDailyHttpOriginalContentLengthApplication,
        original_values ? original_values[original_count - 1] : 0);
    VerifyPrefInt64(
        data_reduction_proxy::prefs::kDailyHttpReceivedContentLengthApplication,
        received_values ? received_values[received_count - 1] : 0);

    VerifyPrefInt64(
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthWithDataReductionProxyEnabledApplication,
        original_with_data_reduction_proxy_enabled_values
            ? original_with_data_reduction_proxy_enabled_values
                  [original_with_data_reduction_proxy_enabled_count - 1]
            : 0);
    VerifyPrefInt64(
        data_reduction_proxy::prefs::
            kDailyContentLengthWithDataReductionProxyEnabledApplication,
        received_with_data_reduction_proxy_enabled_values
            ? received_with_data_reduction_proxy_enabled_values
                  [received_with_data_reduction_proxy_count - 1]
            : 0);

    VerifyPrefInt64(
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthViaDataReductionProxyApplication,
        original_via_data_reduction_proxy_values
            ? original_via_data_reduction_proxy_values
                  [original_via_data_reduction_proxy_count - 1]
            : 0);
    VerifyPrefInt64(data_reduction_proxy::prefs::
                        kDailyContentLengthViaDataReductionProxyApplication,
                    received_via_data_reduction_proxy_values
                        ? received_via_data_reduction_proxy_values
                              [received_via_data_reduction_proxy_count - 1]
                        : 0);
  }

  // Verify daily data saving pref for request types.
  void VerifyDailyRequestTypeContentLengthPrefLists(
      const int64_t* original_values,
      size_t original_count,
      const int64_t* received_values,
      size_t received_count,
      const int64_t* original_with_data_reduction_proxy_enabled_values,
      size_t original_with_data_reduction_proxy_enabled_count,
      const int64_t* received_with_data_reduction_proxy_enabled_values,
      size_t received_with_data_reduction_proxy_count,
      const int64_t* https_with_data_reduction_proxy_enabled_values,
      size_t https_with_data_reduction_proxy_enabled_count,
      const int64_t* short_bypass_with_data_reduction_proxy_enabled_values,
      size_t short_bypass_with_data_reduction_proxy_enabled_count,
      const int64_t* long_bypass_with_data_reduction_proxy_enabled_values,
      size_t long_bypass_with_data_reduction_proxy_enabled_count,
      const int64_t* unknown_with_data_reduction_proxy_enabled_values,
      size_t unknown_with_data_reduction_proxy_enabled_count) {
    VerifyPrefList(data_reduction_proxy::prefs::kDailyHttpOriginalContentLength,
                   original_values, original_count, kNumDaysInHistory);
    VerifyPrefList(data_reduction_proxy::prefs::kDailyHttpReceivedContentLength,
                   received_values, received_count, kNumDaysInHistory);
    VerifyPrefList(data_reduction_proxy::prefs::
                       kDailyOriginalContentLengthWithDataReductionProxyEnabled,
                   original_with_data_reduction_proxy_enabled_values,
                   original_with_data_reduction_proxy_enabled_count,
                   kNumDaysInHistory);
    VerifyPrefList(data_reduction_proxy::prefs::
                       kDailyContentLengthWithDataReductionProxyEnabled,
                   received_with_data_reduction_proxy_enabled_values,
                   received_with_data_reduction_proxy_count, kNumDaysInHistory);
    VerifyPrefList(data_reduction_proxy::prefs::
                       kDailyContentLengthHttpsWithDataReductionProxyEnabled,
                   https_with_data_reduction_proxy_enabled_values,
                   https_with_data_reduction_proxy_enabled_count,
                   kNumDaysInHistory);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyContentLengthShortBypassWithDataReductionProxyEnabled,
        short_bypass_with_data_reduction_proxy_enabled_values,
        short_bypass_with_data_reduction_proxy_enabled_count,
        kNumDaysInHistory);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyContentLengthLongBypassWithDataReductionProxyEnabled,
        long_bypass_with_data_reduction_proxy_enabled_values,
        long_bypass_with_data_reduction_proxy_enabled_count, kNumDaysInHistory);
    VerifyPrefList(data_reduction_proxy::prefs::
                       kDailyContentLengthUnknownWithDataReductionProxyEnabled,
                   unknown_with_data_reduction_proxy_enabled_values,
                   unknown_with_data_reduction_proxy_enabled_count,
                   kNumDaysInHistory);
  }

  int64_t GetInt64(const char* pref_path) {
    return compression_stats_->GetInt64(pref_path);
  }

  void SetInt64(const char* pref_path, int64_t pref_value) {
    compression_stats_->SetInt64(pref_path, pref_value);
  }

  std::string NormalizeHostname(const std::string& hostname) {
    return DataReductionProxyCompressionStats::NormalizeHostname(hostname);
  }

  void RecordContentLengthPrefs(int64_t received_content_length,
                                int64_t original_content_length,
                                bool with_data_reduction_proxy_enabled,
                                DataReductionProxyRequestType request_type,
                                const std::string& mime_type,
                                base::Time now) {
    compression_stats_->RecordRequestSizePrefs(
        received_content_length, original_content_length,
        with_data_reduction_proxy_enabled, request_type, mime_type, now);
  }

  void RecordContentLengthPrefs(int64_t received_content_length,
                                int64_t original_content_length,
                                bool with_data_reduction_proxy_enabled,
                                DataReductionProxyRequestType request_type,
                                base::Time now) {
    RecordContentLengthPrefs(received_content_length, original_content_length,
                             with_data_reduction_proxy_enabled, request_type,
                             "application/octet-stream", now);
  }

  void RecordDataUsage(const std::string& data_usage_host,
                       int64_t data_used,
                       int64_t original_size,
                       const base::Time& time) {
    compression_stats_->RecordDataUseByHost(data_usage_host, data_used,
                                            original_size, time);
  }

  void GetHistoricalDataUsage(
      const HistoricalDataUsageCallback& onLoadDataUsage,
      const base::Time& now) {
    compression_stats_->GetHistoricalDataUsageImpl(onLoadDataUsage, now);
  }

  void LoadHistoricalDataUsage(
      const HistoricalDataUsageCallback& onLoadDataUsage) {
    compression_stats_->service_->LoadHistoricalDataUsage(onLoadDataUsage);
  }

  void DeleteHistoricalDataUsage() {
    compression_stats_->DeleteHistoricalDataUsage();
  }

  void ClearDataSavingStatistics() {
    compression_stats_->ClearDataSavingStatistics(
        DataReductionProxySavingsClearedReason::
            USER_ACTION_DELETE_BROWSING_HISTORY);
  }

  void DeleteBrowsingHistory(const base::Time& start, const base::Time& end) {
    compression_stats_->DeleteBrowsingHistory(start, end);
  }

  void EnableDataUsageReporting() {
    pref_service()->SetBoolean(prefs::kDataUsageReportingEnabled, true);
  }

  void DisableDataUsageReporting() {
    pref_service()->SetBoolean(prefs::kDataUsageReportingEnabled, false);
  }

  DataReductionProxyCompressionStats* compression_stats() {
    return compression_stats_.get();
  }

  void ForceWritePrefs() { compression_stats_->WritePrefs(); }

  bool IsDelayedWriteTimerRunning() const {
    return compression_stats_->pref_writer_timer_.IsRunning();
  }

  TestingPrefServiceSimple* pref_service() {
    return drp_test_context_->pref_service();
  }

  DataReductionProxyService* data_reduction_proxy_service() {
    return drp_test_context_->data_reduction_proxy_service();
  }

  bool IsDataReductionProxyEnabled() {
    return drp_test_context_->IsDataReductionProxyEnabled();
  }

  void InitializeWeeklyAggregateDataUse(const base::Time& now) {
    compression_stats_->InitializeWeeklyAggregateDataUse(now);
  }

  void RecordWeeklyAggregateDataUse(
      const base::Time& now,
      int32_t received_kb,
      bool is_user_request,
      data_use_measurement::DataUseUserData::DataUseContentType content_type,
      int32_t service_hash_code) {
    compression_stats_->RecordWeeklyAggregateDataUse(
        now, received_kb, is_user_request, content_type, service_hash_code);
  }

  void VerifyDictionaryPref(const std::string& pref,
                            int key,
                            int expected_value) const {
    const base::DictionaryValue* dict =
        compression_stats_->pref_service_->GetDictionary(pref);
    EXPECT_EQ(expected_value != 0, dict->HasKey(base::IntToString(key)));
    if (expected_value) {
      EXPECT_EQ(expected_value,
                dict->FindKey(base::IntToString(key))->GetInt());
    }
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context_;
  std::unique_ptr<DataReductionProxyCompressionStats> compression_stats_;
  base::Time now_;
  base::TimeDelta now_delta_;
};

TEST_F(DataReductionProxyCompressionStatsTest, WritePrefsDirect) {
  SetUpPrefs();

  VerifyPrefWasWritten(prefs::kHttpOriginalContentLength);
  VerifyPrefWasWritten(prefs::kHttpReceivedContentLength);
  VerifyPrefListWasWritten(prefs::kDailyHttpOriginalContentLength);
  VerifyPrefListWasWritten(prefs::kDailyHttpReceivedContentLength);
}

TEST_F(DataReductionProxyCompressionStatsTest, WritePrefsDelayed) {
  ResetCompressionStatsWithDelay(
      base::TimeDelta::FromMinutes(kWriteDelayMinutes));

  EXPECT_EQ(0, pref_service()->GetInt64(prefs::kHttpOriginalContentLength));
  EXPECT_EQ(0, pref_service()->GetInt64(prefs::kHttpReceivedContentLength));
  EXPECT_FALSE(IsDelayedWriteTimerRunning());

  SetUpPrefs();
  EXPECT_TRUE(IsDelayedWriteTimerRunning());
  ForceWritePrefs();

  VerifyPrefWasWritten(prefs::kHttpOriginalContentLength);
  VerifyPrefWasWritten(prefs::kHttpReceivedContentLength);
  VerifyPrefListWasWritten(prefs::kDailyHttpOriginalContentLength);
  VerifyPrefListWasWritten(prefs::kDailyHttpReceivedContentLength);
}

TEST_F(DataReductionProxyCompressionStatsTest, StatsRestoredOnOnRestart) {
  base::ListValue list_value;
  list_value.Insert(0,
                    std::make_unique<base::Value>(base::Int64ToString(1234)));
  pref_service()->Set(prefs::kDailyHttpOriginalContentLength, list_value);

  ResetCompressionStatsWithDelay(
      base::TimeDelta::FromMinutes(kWriteDelayMinutes));

  const base::ListValue* value = pref_service()->GetList(
      prefs::kDailyHttpOriginalContentLength);
  std::string string_value;
  value->GetString(0, &string_value);
  EXPECT_EQ("1234", string_value);
}

TEST_F(DataReductionProxyCompressionStatsTest, TotalLengths) {
  const int64_t kOriginalLength = 200;
  const int64_t kReceivedLength = 100;

  compression_stats()->RecordDataUseWithMimeType(
      kReceivedLength, kOriginalLength, IsDataReductionProxyEnabled(),
      UNKNOWN_TYPE, std::string(), true,
      data_use_measurement::DataUseUserData::OTHER, 0);

  EXPECT_EQ(kReceivedLength,
            GetInt64(data_reduction_proxy::prefs::kHttpReceivedContentLength));
  EXPECT_FALSE(IsDataReductionProxyEnabled());
  EXPECT_EQ(kOriginalLength,
            GetInt64(data_reduction_proxy::prefs::kHttpOriginalContentLength));

  // Record the same numbers again, and total lengths should be doubled.
  compression_stats()->RecordDataUseWithMimeType(
      kReceivedLength, kOriginalLength, IsDataReductionProxyEnabled(),
      UNKNOWN_TYPE, std::string(), true,
      data_use_measurement::DataUseUserData::OTHER, 0);

  EXPECT_EQ(kReceivedLength * 2,
            GetInt64(data_reduction_proxy::prefs::kHttpReceivedContentLength));
  EXPECT_FALSE(IsDataReductionProxyEnabled());
  EXPECT_EQ(kOriginalLength * 2,
            GetInt64(data_reduction_proxy::prefs::kHttpOriginalContentLength));
}

TEST_F(DataReductionProxyCompressionStatsTest, OneResponse) {
  const int64_t kOriginalLength = 200;
  const int64_t kReceivedLength = 100;
  int64_t original[] = {kOriginalLength};
  int64_t received[] = {kReceivedLength};

  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());

  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1, original, 1, received, 1, original, 1, received,
      1, kNumDaysInHistory);
}

TEST_F(DataReductionProxyCompressionStatsTest, MultipleResponses) {
  const int64_t kOriginalLength = 150;
  const int64_t kReceivedLength = 100;
  int64_t original[] = {kOriginalLength};
  int64_t received[] = {kReceivedLength};
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, false, UNKNOWN_TYPE, FakeNow());
  VerifyDailyDataSavingContentLengthPrefLists(original, 1, received, 1, nullptr,
                                              0, nullptr, 0, nullptr, 0,
                                              nullptr, 0, kNumDaysInHistory);

  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, UNKNOWN_TYPE, FakeNow());
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  int64_t original_proxy_enabled[] = {kOriginalLength};
  int64_t received_proxy_enabled[] = {kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1, original_proxy_enabled, 1,
      received_proxy_enabled, 1, nullptr, 0, nullptr, 0, kNumDaysInHistory);

  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  original_proxy_enabled[0] += kOriginalLength;
  received_proxy_enabled[0] += kReceivedLength;
  int64_t original_via_proxy[] = {kOriginalLength};
  int64_t received_via_proxy[] = {kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1, original_proxy_enabled, 1,
      received_proxy_enabled, 1, original_via_proxy, 1, received_via_proxy, 1,
      kNumDaysInHistory);

  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, UNKNOWN_TYPE, FakeNow());
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  original_proxy_enabled[0] += kOriginalLength;
  received_proxy_enabled[0] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1, original_proxy_enabled, 1,
      received_proxy_enabled, 1, original_via_proxy, 1, received_via_proxy, 1,
      kNumDaysInHistory);

  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, false, UNKNOWN_TYPE, FakeNow());
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1, original_proxy_enabled, 1,
      received_proxy_enabled, 1, original_via_proxy, 1, received_via_proxy, 1,
      kNumDaysInHistory);
}

TEST_F(DataReductionProxyCompressionStatsTest, RequestType) {
  const int64_t kContentLength = 200;
  int64_t received[] = {0};
  int64_t https_received[] = {0};
  int64_t total_received[] = {0};
  int64_t proxy_enabled_received[] = {0};

  RecordContentLengthPrefs(
      kContentLength, kContentLength, true, HTTPS, FakeNow());
  total_received[0] += kContentLength;
  proxy_enabled_received[0] += kContentLength;
  https_received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 0,  // short bypass
      received, 0,  // long bypass
      received, 0);  // unknown

  // Data reduction proxy is not enabled.
  RecordContentLengthPrefs(
      kContentLength, kContentLength, false, HTTPS, FakeNow());
  total_received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 0,  // short bypass
      received, 0,  // long bypass
      received, 0);  // unknown

  RecordContentLengthPrefs(
      kContentLength, kContentLength, true, HTTPS, FakeNow());
  total_received[0] += kContentLength;
  proxy_enabled_received[0] += kContentLength;
  https_received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 0,  // short bypass
      received, 0,  // long bypass
      received, 0);  // unknown

  RecordContentLengthPrefs(
      kContentLength, kContentLength, true, SHORT_BYPASS, FakeNow());
  total_received[0] += kContentLength;
  proxy_enabled_received[0] += kContentLength;
  received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 1,  // short bypass
      received, 0,  // long bypass
      received, 0);  // unknown

  RecordContentLengthPrefs(
      kContentLength, kContentLength, true, LONG_BYPASS, FakeNow());
  total_received[0] += kContentLength;
  proxy_enabled_received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,  // total
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 1,  // short bypass
      received, 1,  // long bypass
      received, 0);  // unknown

  RecordContentLengthPrefs(
      kContentLength, kContentLength, true, UNKNOWN_TYPE, FakeNow());
  total_received[0] += kContentLength;
  proxy_enabled_received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 1,  // short bypass
      received, 1,  // long bypass
      received, 1);  // unknown
}

TEST_F(DataReductionProxyCompressionStatsTest, ForwardOneDay) {
  const int64_t kOriginalLength = 200;
  const int64_t kReceivedLength = 100;

  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());

  // Forward one day.
  SetFakeTimeDeltaInHours(24);

  // Proxy not enabled. Not via proxy.
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, false, UNKNOWN_TYPE, FakeNow());

  int64_t original[] = {kOriginalLength, kOriginalLength};
  int64_t received[] = {kReceivedLength, kReceivedLength};
  int64_t original_with_data_reduction_proxy_enabled[] = {kOriginalLength, 0};
  int64_t received_with_data_reduction_proxy_enabled[] = {kReceivedLength, 0};
  int64_t original_via_data_reduction_proxy[] = {kOriginalLength, 0};
  int64_t received_via_data_reduction_proxy[] = {kReceivedLength, 0};
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2, received, 2, original_with_data_reduction_proxy_enabled, 2,
      received_with_data_reduction_proxy_enabled, 2,
      original_via_data_reduction_proxy, 2, received_via_data_reduction_proxy,
      2, kNumDaysInHistory);

  // Proxy enabled. Not via proxy.
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, UNKNOWN_TYPE, FakeNow());
  original[1] += kOriginalLength;
  received[1] += kReceivedLength;
  original_with_data_reduction_proxy_enabled[1] += kOriginalLength;
  received_with_data_reduction_proxy_enabled[1] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2, received, 2, original_with_data_reduction_proxy_enabled, 2,
      received_with_data_reduction_proxy_enabled, 2,
      original_via_data_reduction_proxy, 2, received_via_data_reduction_proxy,
      2, kNumDaysInHistory);

  // Proxy enabled and via proxy.
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  original[1] += kOriginalLength;
  received[1] += kReceivedLength;
  original_with_data_reduction_proxy_enabled[1] += kOriginalLength;
  received_with_data_reduction_proxy_enabled[1] += kReceivedLength;
  original_via_data_reduction_proxy[1] += kOriginalLength;
  received_via_data_reduction_proxy[1] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2, received, 2, original_with_data_reduction_proxy_enabled, 2,
      received_with_data_reduction_proxy_enabled, 2,
      original_via_data_reduction_proxy, 2, received_via_data_reduction_proxy,
      2, kNumDaysInHistory);

  // Proxy enabled and via proxy, with content length greater than max int32_t.
  const int64_t kBigOriginalLength = 0x300000000LL;  // 12G.
  const int64_t kBigReceivedLength = 0x200000000LL;  // 8G.
  RecordContentLengthPrefs(kBigReceivedLength, kBigOriginalLength, true,
                           VIA_DATA_REDUCTION_PROXY, FakeNow());
  original[1] += kBigOriginalLength;
  received[1] += kBigReceivedLength;
  original_with_data_reduction_proxy_enabled[1] += kBigOriginalLength;
  received_with_data_reduction_proxy_enabled[1] += kBigReceivedLength;
  original_via_data_reduction_proxy[1] += kBigOriginalLength;
  received_via_data_reduction_proxy[1] += kBigReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2, received, 2, original_with_data_reduction_proxy_enabled, 2,
      received_with_data_reduction_proxy_enabled, 2,
      original_via_data_reduction_proxy, 2, received_via_data_reduction_proxy,
      2, kNumDaysInHistory);
}

TEST_F(DataReductionProxyCompressionStatsTest, PartialDayTimeChange) {
  const int64_t kOriginalLength = 200;
  const int64_t kReceivedLength = 100;
  int64_t original[] = {0, kOriginalLength};
  int64_t received[] = {0, kReceivedLength};

  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2, received, 2, original, 2, received, 2, original, 2, received,
      2, kNumDaysInHistory);

  // Forward 10 hours, stay in the same day.
  // See kLastUpdateTime: "Now" in test is 03:45am.
  SetFakeTimeDeltaInHours(10);
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  original[1] += kOriginalLength;
  received[1] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2, received, 2, original, 2, received, 2, original, 2, received,
      2, kNumDaysInHistory);

  // Forward 11 more hours, comes to tomorrow.
  AddFakeTimeDeltaInHours(11);
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  int64_t original2[] = {kOriginalLength * 2, kOriginalLength};
  int64_t received2[] = {kReceivedLength * 2, kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original2, 2, received2, 2, original2, 2, received2, 2, original2, 2,
      received2, 2, kNumDaysInHistory);
}

TEST_F(DataReductionProxyCompressionStatsTest, BackwardAndForwardOneDay) {
  base::HistogramTester histogram_tester;
  const int64_t kOriginalLength = 200;
  const int64_t kReceivedLength = 100;
  int64_t original[] = {kOriginalLength};
  int64_t received[] = {kReceivedLength};

  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());

  // Backward one day, expect no count.
  SetFakeTimeDeltaInHours(-24);
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1, original, 1, received, 1, original, 1, received,
      1, kNumDaysInHistory);
  histogram_tester.ExpectTotalCount("DataReductionProxy.SavingsCleared.Reason",
                                    0);

  // Then forward one day, expect no count.
  AddFakeTimeDeltaInHours(24);
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  int64_t original2[] = {kOriginalLength * 2, kOriginalLength};
  int64_t received2[] = {kReceivedLength * 2, kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original2, 2, received2, 2, original2, 2, received2, 2, original2, 2,
      received2, 2, kNumDaysInHistory);
  histogram_tester.ExpectTotalCount("DataReductionProxy.SavingsCleared.Reason",
                                    0);
}

TEST_F(DataReductionProxyCompressionStatsTest, BackwardTwoDays) {
  base::HistogramTester histogram_tester;
  const int64_t kOriginalLength = 200;
  const int64_t kReceivedLength = 100;
  int64_t original[] = {kOriginalLength};
  int64_t received[] = {kReceivedLength};

  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());

  // Backward two days, expect SYSTEM_CLOCK_MOVED_BACK.
  SetFakeTimeDeltaInHours(-2 * 24);
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1, original, 1, received, 1, original, 1, received,
      1, kNumDaysInHistory);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.SavingsCleared.Reason",
      DataReductionProxySavingsClearedReason::SYSTEM_CLOCK_MOVED_BACK, 1);
  VerifyPrefInt64(prefs::kDataReductionProxySavingsClearedNegativeSystemClock,
                  FakeNow().ToInternalValue());

  // Backward another two days, expect SYSTEM_CLOCK_MOVED_BACK.
  SetFakeTimeDeltaInHours(-4 * 24);
  RecordContentLengthPrefs(kReceivedLength, kOriginalLength, true,
                           VIA_DATA_REDUCTION_PROXY, FakeNow());
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.SavingsCleared.Reason",
      DataReductionProxySavingsClearedReason::SYSTEM_CLOCK_MOVED_BACK, 2);

  // Forward 2 days, expect no change.
  AddFakeTimeDeltaInHours(2 * 24);
  RecordContentLengthPrefs(kReceivedLength, kOriginalLength, true,
                           VIA_DATA_REDUCTION_PROXY, FakeNow());
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.SavingsCleared.Reason",
      DataReductionProxySavingsClearedReason::SYSTEM_CLOCK_MOVED_BACK, 2);
}

TEST_F(DataReductionProxyCompressionStatsTest, NormalizeHostname) {
  EXPECT_EQ("www.foo.com", NormalizeHostname("http://www.foo.com"));
  EXPECT_EQ("foo.com", NormalizeHostname("https://foo.com"));
  EXPECT_EQ("bar.co.uk", NormalizeHostname("http://bar.co.uk"));
  EXPECT_EQ("http.www.co.in", NormalizeHostname("http://http.www.co.in"));
}

TEST_F(DataReductionProxyCompressionStatsTest, RecordUma) {
  const int64_t kOriginalLength = 15000;
  const int64_t kReceivedLength = 10000;
  base::HistogramTester tester;

  RecordContentLengthPrefs(kReceivedLength, kOriginalLength, true,
                           VIA_DATA_REDUCTION_PROXY, FakeNow());

  // Forward one day.
  SetFakeTimeDeltaInHours(24);

  // Proxy not enabled. Not via proxy.
  RecordContentLengthPrefs(kReceivedLength, kOriginalLength, false,
                           UNKNOWN_TYPE, FakeNow());

  // 15000 falls into the 12 KB bucket
  tester.ExpectUniqueSample("Net.DailyOriginalContentLength", 12, 1);
  tester.ExpectUniqueSample("Net.DailyOriginalContentLength_Application", 12,
                            1);
  tester.ExpectUniqueSample(
      "Net.DailyOriginalContentLength_DataReductionProxyEnabled", 12, 1);
  tester.ExpectUniqueSample(
      "Net.DailyOriginalContentLength_DataReductionProxyEnabled_Application",
      12, 1);
  tester.ExpectUniqueSample(
      "Net.DailyOriginalContentLength_ViaDataReductionProxy", 12, 1);
  tester.ExpectUniqueSample(
      "Net.DailyOriginalContentLength_ViaDataReductionProxy_Application", 12,
      1);

  // 10000 falls into the 9 KB bucket
  tester.ExpectUniqueSample("Net.DailyContentLength", 9, 1);
  tester.ExpectUniqueSample("Net.DailyReceivedContentLength_Application", 9, 1);
  tester.ExpectUniqueSample("Net.DailyContentLength_DataReductionProxyEnabled",
                            9, 1);
  tester.ExpectUniqueSample(
      "Net.DailyContentLength_DataReductionProxyEnabled_Application", 9, 1);
  tester.ExpectUniqueSample("Net.DailyContentLength_ViaDataReductionProxy", 9,
                            1);
  tester.ExpectUniqueSample(
      "Net.DailyContentLength_ViaDataReductionProxy_Application", 9, 1);

  // floor((15000 - 10000) * 100) = 33.
  tester.ExpectUniqueSample("Net.DailyContentSavingPercent", 33, 1);
  tester.ExpectUniqueSample(
      "Net.DailyContentSavingPercent_DataReductionProxyEnabled", 33, 1);
  tester.ExpectUniqueSample(
      "Net.DailyContentSavingPercent_ViaDataReductionProxy", 33, 1);

  tester.ExpectUniqueSample("Net.DailyContentPercent_DataReductionProxyEnabled",
                            100, 1);
  tester.ExpectUniqueSample("Net.DailyContentPercent_ViaDataReductionProxy",
                            100, 1);
  tester.ExpectUniqueSample(
      "Net.DailyContentPercent_DataReductionProxyEnabled_Unknown", 0, 1);
}

TEST_F(DataReductionProxyCompressionStatsTest, RecordDataUsageSingleSite) {
  EnableDataUsageReporting();
  base::RunLoop().RunUntilIdle();

  base::Time now = base::Time::Now();
  RecordDataUsage("https://www.foo.com", 1000, 1250, now);

  auto expected_data_usage =
      std::make_unique<std::vector<data_reduction_proxy::DataUsageBucket>>(
          kNumExpectedBuckets);
  data_reduction_proxy::PerConnectionDataUsage* connection_usage =
      expected_data_usage->at(kNumExpectedBuckets - 1).add_connection_usage();
  data_reduction_proxy::PerSiteDataUsage* site_usage =
      connection_usage->add_site_usage();
  site_usage->set_hostname("www.foo.com");
  site_usage->set_data_used(1000);
  site_usage->set_original_size(1250);

  DataUsageLoadVerifier verifier(std::move(expected_data_usage));

  GetHistoricalDataUsage(base::Bind(&DataUsageLoadVerifier::OnLoadDataUsage,
                                    base::Unretained(&verifier)),
                         now);
  base::RunLoop().RunUntilIdle();
}

TEST_F(DataReductionProxyCompressionStatsTest, DisableDataUsageRecording) {
  EnableDataUsageReporting();
  base::RunLoop().RunUntilIdle();

  base::Time now = base::Time::Now();
  RecordDataUsage("https://www.foo.com", 1000, 1250, now);

  DisableDataUsageReporting();
  base::RunLoop().RunUntilIdle();

#if !defined(OS_ANDROID)
  // Data usage on disk must be deleted.
  auto expected_data_usage1 =
      std::make_unique<std::vector<data_reduction_proxy::DataUsageBucket>>(
          kNumExpectedBuckets);
  DataUsageLoadVerifier verifier1(std::move(expected_data_usage1));
  LoadHistoricalDataUsage(base::Bind(&DataUsageLoadVerifier::OnLoadDataUsage,
                                     base::Unretained(&verifier1)));

  // Public API must return an empty array.
  auto expected_data_usage2 =
      std::make_unique<std::vector<data_reduction_proxy::DataUsageBucket>>();
  DataUsageLoadVerifier verifier2(std::move(expected_data_usage2));
  GetHistoricalDataUsage(base::Bind(&DataUsageLoadVerifier::OnLoadDataUsage,
                                    base::Unretained(&verifier2)),
                         now);
#else
  // For Android don't delete data usage.
  auto expected_data_usage =
      std::make_unique<std::vector<data_reduction_proxy::DataUsageBucket>>(
          kNumExpectedBuckets);
  data_reduction_proxy::PerConnectionDataUsage* connection_usage =
      expected_data_usage->at(kNumExpectedBuckets - 1).add_connection_usage();
  data_reduction_proxy::PerSiteDataUsage* site_usage =
      connection_usage->add_site_usage();
  site_usage->set_hostname("www.foo.com");
  site_usage->set_data_used(1000);
  site_usage->set_original_size(1250);

  DataUsageLoadVerifier verifier(std::move(expected_data_usage));

  GetHistoricalDataUsage(base::Bind(&DataUsageLoadVerifier::OnLoadDataUsage,
                                    base::Unretained(&verifier)),
                         now);
#endif

  base::RunLoop().RunUntilIdle();
}

TEST_F(DataReductionProxyCompressionStatsTest, RecordDataUsageMultipleSites) {
  EnableDataUsageReporting();
  base::RunLoop().RunUntilIdle();

  base::Time now = base::Time::Now();
  RecordDataUsage("https://www.foo.com", 1000, 1250, now);
  RecordDataUsage("https://bar.com", 1001, 1251, now);
  RecordDataUsage("http://foobar.com", 1002, 1252, now);

  auto expected_data_usage =
      std::make_unique<std::vector<data_reduction_proxy::DataUsageBucket>>(
          kNumExpectedBuckets);
  data_reduction_proxy::PerConnectionDataUsage* connection_usage =
      expected_data_usage->at(kNumExpectedBuckets - 1).add_connection_usage();
  data_reduction_proxy::PerSiteDataUsage* site_usage =
      connection_usage->add_site_usage();
  site_usage->set_hostname("www.foo.com");
  site_usage->set_data_used(1000);
  site_usage->set_original_size(1250);

  site_usage = connection_usage->add_site_usage();
  site_usage->set_hostname("bar.com");
  site_usage->set_data_used(1001);
  site_usage->set_original_size(1251);

  site_usage = connection_usage->add_site_usage();
  site_usage->set_hostname("foobar.com");
  site_usage->set_data_used(1002);
  site_usage->set_original_size(1252);

  DataUsageLoadVerifier verifier(std::move(expected_data_usage));

  GetHistoricalDataUsage(base::Bind(&DataUsageLoadVerifier::OnLoadDataUsage,
                                    base::Unretained(&verifier)),
                         now);
  base::RunLoop().RunUntilIdle();
}

TEST_F(DataReductionProxyCompressionStatsTest,
       RecordDataUsageConsecutiveBuckets) {
  EnableDataUsageReporting();
  base::RunLoop().RunUntilIdle();

  base::Time now = base::Time::Now();
  base::Time fifteen_mins_ago = now - TimeDelta::FromMinutes(15);

  RecordDataUsage("https://www.foo.com", 1000, 1250, fifteen_mins_ago);

  RecordDataUsage("https://bar.com", 1001, 1251, now);

  auto expected_data_usage =
      std::make_unique<std::vector<data_reduction_proxy::DataUsageBucket>>(
          kNumExpectedBuckets);
  data_reduction_proxy::PerConnectionDataUsage* connection_usage =
      expected_data_usage->at(kNumExpectedBuckets - 2).add_connection_usage();
  data_reduction_proxy::PerSiteDataUsage* site_usage =
      connection_usage->add_site_usage();
  site_usage->set_hostname("www.foo.com");
  site_usage->set_data_used(1000);
  site_usage->set_original_size(1250);

  connection_usage =
      expected_data_usage->at(kNumExpectedBuckets - 1).add_connection_usage();
  site_usage = connection_usage->add_site_usage();
  site_usage->set_hostname("bar.com");
  site_usage->set_data_used(1001);
  site_usage->set_original_size(1251);

  DataUsageLoadVerifier verifier(std::move(expected_data_usage));

  GetHistoricalDataUsage(base::Bind(&DataUsageLoadVerifier::OnLoadDataUsage,
                                    base::Unretained(&verifier)),
                         now);
  base::RunLoop().RunUntilIdle();
}

// Test that the last entry in data usage bucket vector is for the current
// interval even when current interval does not have any data usage.
TEST_F(DataReductionProxyCompressionStatsTest,
       RecordDataUsageEmptyCurrentInterval) {
  EnableDataUsageReporting();
  base::RunLoop().RunUntilIdle();

  base::Time now = base::Time::Now();
  base::Time fifteen_mins_ago = now - TimeDelta::FromMinutes(15);

  RecordDataUsage("https://www.foo.com", 1000, 1250, fifteen_mins_ago);

  auto expected_data_usage =
      base::WrapUnique(new std::vector<data_reduction_proxy::DataUsageBucket>(
          kNumExpectedBuckets));
  data_reduction_proxy::PerConnectionDataUsage* connection_usage =
      expected_data_usage->at(kNumExpectedBuckets - 2).add_connection_usage();
  data_reduction_proxy::PerSiteDataUsage* site_usage =
      connection_usage->add_site_usage();
  site_usage->set_hostname("www.foo.com");
  site_usage->set_data_used(1000);
  site_usage->set_original_size(1250);

  DataUsageLoadVerifier verifier(std::move(expected_data_usage));

  GetHistoricalDataUsage(base::Bind(&DataUsageLoadVerifier::OnLoadDataUsage,
                                    base::Unretained(&verifier)),
                         now);
  base::RunLoop().RunUntilIdle();
}

TEST_F(DataReductionProxyCompressionStatsTest, DeleteHistoricalDataUsage) {
  EnableDataUsageReporting();
  base::RunLoop().RunUntilIdle();

  base::Time now = base::Time::Now();
  base::Time fifteen_mins_ago = now - TimeDelta::FromMinutes(15);
  // Fake record to be from 15 minutes ago so that it is flushed to storage.
  RecordDataUsage("https://www.bar.com", 900, 1100, fifteen_mins_ago);

  RecordDataUsage("https://www.foo.com", 1000, 1250, now);

  DeleteHistoricalDataUsage();
  base::RunLoop().RunUntilIdle();

  auto expected_data_usage =
      std::make_unique<std::vector<data_reduction_proxy::DataUsageBucket>>(
          kNumExpectedBuckets);
  DataUsageLoadVerifier verifier(std::move(expected_data_usage));

  GetHistoricalDataUsage(base::Bind(&DataUsageLoadVerifier::OnLoadDataUsage,
                                    base::Unretained(&verifier)),
                         now);
  base::RunLoop().RunUntilIdle();
}

TEST_F(DataReductionProxyCompressionStatsTest, DeleteBrowsingHistory) {
  EnableDataUsageReporting();
  base::RunLoop().RunUntilIdle();

  base::Time now = base::Time::Now();
  base::Time fifteen_mins_ago = now - TimeDelta::FromMinutes(15);

  // Fake record to be from 15 minutes ago so that it is flushed to storage.
  RecordDataUsage("https://www.bar.com", 900, 1100, fifteen_mins_ago);

  // This data usage will be in kept in memory.
  RecordDataUsage("https://www.foo.com", 1000, 1250, now);

  // This should only delete in-memory usage.
  DeleteBrowsingHistory(now, now);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(compression_stats()->DataUsageMapForTesting().empty());

  auto expected_data_usage =
      std::make_unique<std::vector<data_reduction_proxy::DataUsageBucket>>(
          kNumExpectedBuckets);
  data_reduction_proxy::PerConnectionDataUsage* connection_usage =
      expected_data_usage->at(kNumExpectedBuckets - 1).add_connection_usage();
  data_reduction_proxy::PerSiteDataUsage* site_usage =
      connection_usage->add_site_usage();
  site_usage->set_hostname("www.bar.com");
  site_usage->set_data_used(900);
  site_usage->set_original_size(1100);
  DataUsageLoadVerifier verifier1(std::move(expected_data_usage));

  LoadHistoricalDataUsage(base::Bind(&DataUsageLoadVerifier::OnLoadDataUsage,
                                     base::Unretained(&verifier1)));
  base::RunLoop().RunUntilIdle();

  // This should delete in-storage usage as well.
  DeleteBrowsingHistory(fifteen_mins_ago, now);
  base::RunLoop().RunUntilIdle();

  expected_data_usage =
      std::make_unique<std::vector<data_reduction_proxy::DataUsageBucket>>(
          kNumExpectedBuckets);
  DataUsageLoadVerifier verifier2(std::move(expected_data_usage));
  LoadHistoricalDataUsage(base::Bind(&DataUsageLoadVerifier::OnLoadDataUsage,
                                     base::Unretained(&verifier2)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(DataReductionProxyCompressionStatsTest, ClearDataSavingStatistics) {
  EnableDataUsageReporting();
  base::RunLoop().RunUntilIdle();

  base::Time now = base::Time::Now();
  base::Time fifteen_mins_ago = now - TimeDelta::FromMinutes(15);
  // Fake record to be from 15 minutes ago so that it is flushed to storage.
  RecordDataUsage("https://www.bar.com", 900, 1100, fifteen_mins_ago);

  RecordDataUsage("https://www.foo.com", 1000, 1250, now);

  const int64_t kOriginalLength = 200;
  const int64_t kReceivedLength = 100;
  int64_t original[] = {kOriginalLength};
  int64_t received[] = {kReceivedLength};

  RecordContentLengthPrefs(kReceivedLength, kOriginalLength, true,
                           VIA_DATA_REDUCTION_PROXY, FakeNow());

  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1, original, 1, received, 1, original, 1, received,
      1, kNumDaysInHistory);

  ClearDataSavingStatistics();
  base::RunLoop().RunUntilIdle();

  auto expected_data_usage =
      std::make_unique<std::vector<data_reduction_proxy::DataUsageBucket>>(
          kNumExpectedBuckets);
  DataUsageLoadVerifier verifier(std::move(expected_data_usage));

  GetHistoricalDataUsage(base::Bind(&DataUsageLoadVerifier::OnLoadDataUsage,
                                    base::Unretained(&verifier)),
                         now);
  base::RunLoop().RunUntilIdle();

  VerifyDailyDataSavingContentLengthPrefLists(nullptr, 0, nullptr, 0, nullptr,
                                              0, nullptr, 0, nullptr, 0,
                                              nullptr, 0, 0);
}

TEST_F(DataReductionProxyCompressionStatsTest, WeeklyAggregateDataUse) {
  const int32_t kDataUseKB = 100;
  base::HistogramTester histogram_tester;

  InitializeWeeklyAggregateDataUse(base::Time::Now());
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ThisWeekAggregateKB.Services.Downstream.Background",
      0);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ThisWeekAggregateKB.Services.Downstream.Foreground",
      0);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.LastWeekAggregateKB.Services.Downstream.Background",
      0);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.LastWeekAggregateKB.Services.Downstream.Foreground",
      0);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ThisWeekAggregateKB.UserTraffic.Downstream."
      "ContentType",
      0);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.LastWeekAggregateKB.UserTraffic.Downstream."
      "ContentType",
      0);

  RecordWeeklyAggregateDataUse(
      base::Time::Now(), kDataUseKB, true,
      data_use_measurement::DataUseUserData::MAIN_FRAME_HTML, 0);
  VerifyDictionaryPref(prefs::kThisWeekUserTrafficContentTypeDownstreamKB,
                       data_use_measurement::DataUseUserData::MAIN_FRAME_HTML,
                       kDataUseKB);

  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ThisWeekAggregateKB.UserTraffic.Downstream."
      "ContentType",
      0);

  InitializeWeeklyAggregateDataUse(base::Time::Now());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ThisWeekAggregateKB.UserTraffic.Downstream."
      "ContentType",
      data_use_measurement::DataUseUserData::MAIN_FRAME_HTML, kDataUseKB);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.LastWeekAggregateKB.UserTraffic.Downstream."
      "ContentType",
      data_use_measurement::DataUseUserData::MAIN_FRAME_HTML, 0);
}

TEST_F(DataReductionProxyCompressionStatsTest, AggregateDataUseForwardWeeks) {
  const int32_t kMainFrameKB = 100;
  const int32_t kNonMainFrameKB = 101;
  base::HistogramTester histogram_tester;

  base::Time fake_time_now = base::Time::Now();

  InitializeWeeklyAggregateDataUse(fake_time_now);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ThisWeekAggregateKB.UserTraffic.Downstream."
      "ContentType",
      0);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.LastWeekAggregateKB.UserTraffic.Downstream."
      "ContentType",
      0);

  RecordWeeklyAggregateDataUse(
      fake_time_now, kMainFrameKB, true,
      data_use_measurement::DataUseUserData::MAIN_FRAME_HTML, 0);
  VerifyDictionaryPref(prefs::kThisWeekUserTrafficContentTypeDownstreamKB,
                       data_use_measurement::DataUseUserData::MAIN_FRAME_HTML,
                       kMainFrameKB);
  RecordWeeklyAggregateDataUse(
      fake_time_now, kNonMainFrameKB, true,
      data_use_measurement::DataUseUserData::NON_MAIN_FRAME_HTML, 0);
  VerifyDictionaryPref(
      prefs::kThisWeekUserTrafficContentTypeDownstreamKB,
      data_use_measurement::DataUseUserData::NON_MAIN_FRAME_HTML,
      kNonMainFrameKB);

  // Fast forward 7 days, and verify that the last week histograms are recorded.
  fake_time_now += base::TimeDelta::FromDays(7);
  InitializeWeeklyAggregateDataUse(fake_time_now);
  VerifyDictionaryPref(prefs::kLastWeekUserTrafficContentTypeDownstreamKB,
                       data_use_measurement::DataUseUserData::MAIN_FRAME_HTML,
                       kMainFrameKB);
  VerifyDictionaryPref(
      prefs::kLastWeekUserTrafficContentTypeDownstreamKB,
      data_use_measurement::DataUseUserData::NON_MAIN_FRAME_HTML,
      kNonMainFrameKB);

  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.LastWeekAggregateKB.UserTraffic.Downstream."
      "ContentType",
      data_use_measurement::DataUseUserData::MAIN_FRAME_HTML, kMainFrameKB);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.LastWeekAggregateKB.UserTraffic.Downstream."
      "ContentType",
      data_use_measurement::DataUseUserData::NON_MAIN_FRAME_HTML,
      kNonMainFrameKB);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ThisWeekAggregateKB.UserTraffic.Downstream."
      "ContentType",
      0);

  // Subsequent data use should be recorded to the current week prefs.
  RecordWeeklyAggregateDataUse(
      fake_time_now, kMainFrameKB, true,
      data_use_measurement::DataUseUserData::MAIN_FRAME_HTML, 0);
  VerifyDictionaryPref(prefs::kThisWeekUserTrafficContentTypeDownstreamKB,
                       data_use_measurement::DataUseUserData::MAIN_FRAME_HTML,
                       kMainFrameKB);

  // Fast forward by more than two weeks, and the prefs will be cleared.
  fake_time_now += base::TimeDelta::FromDays(15);
  InitializeWeeklyAggregateDataUse(fake_time_now);
  VerifyDictionaryPref(prefs::kLastWeekUserTrafficContentTypeDownstreamKB,
                       data_use_measurement::DataUseUserData::MAIN_FRAME_HTML,
                       0);
  VerifyDictionaryPref(
      prefs::kLastWeekUserTrafficContentTypeDownstreamKB,
      data_use_measurement::DataUseUserData::NON_MAIN_FRAME_HTML, 0);
  VerifyDictionaryPref(prefs::kThisWeekUserTrafficContentTypeDownstreamKB,
                       data_use_measurement::DataUseUserData::MAIN_FRAME_HTML,
                       0);
  VerifyDictionaryPref(
      prefs::kThisWeekUserTrafficContentTypeDownstreamKB,
      data_use_measurement::DataUseUserData::NON_MAIN_FRAME_HTML, 0);
}

}  // namespace data_reduction_proxy
