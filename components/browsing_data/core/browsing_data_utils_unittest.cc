// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/browsing_data_utils.h"

#include <string>
#include <vector>

#include "base/bind_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browsing_data/core/counters/autofill_counter.h"
#include "components/browsing_data/core/counters/history_counter.h"
#include "components/browsing_data/core/counters/passwords_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_data {

namespace {

class FakeWebDataService : public autofill::AutofillWebDataService {
 public:
  FakeWebDataService()
      : AutofillWebDataService(base::ThreadTaskRunnerHandle::Get(),
                               base::ThreadTaskRunnerHandle::Get()) {}

 protected:
  ~FakeWebDataService() override {}
};

}  // namespace

class BrowsingDataUtilsTest : public testing::Test {
 public:
  BrowsingDataUtilsTest() {}
  ~BrowsingDataUtilsTest() override {}

  void SetUp() override {
    browsing_data::prefs::RegisterBrowserUserPrefs(prefs_.registry());
  }

  PrefService* prefs() { return &prefs_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
};

// Tests the complex output of the Autofill counter.
TEST_F(BrowsingDataUtilsTest, AutofillCounterResult) {
  AutofillCounter counter(
      scoped_refptr<FakeWebDataService>(new FakeWebDataService()), nullptr);

  // Test all configurations of zero and nonzero partial results for datatypes.
  // Test singular and plural for each datatype.
  const struct TestCase {
    int num_credit_cards;
    int num_addresses;
    int num_suggestions;
    bool sync_enabled;
    std::string expected_output;
  } kTestCases[] = {
      {0, 0, 0, false, "None"},
      {0, 0, 0, true, "None"},
      {1, 0, 0, false, "1 credit card"},
      {0, 5, 0, false, "5 addresses"},
      {0, 0, 1, false, "1 suggestion"},
      {0, 0, 2, false, "2 suggestions"},
      {0, 0, 2, true, "2 suggestions (synced)"},
      {4, 7, 0, false, "4 credit cards, 7 addresses"},
      {4, 7, 0, true, "4 credit cards, 7 addresses (synced)"},
      {3, 0, 9, false, "3 credit cards, 9 other suggestions"},
      {0, 1, 1, false, "1 address, 1 other suggestion"},
      {9, 6, 3, false, "9 credit cards, 6 addresses, 3 others"},
      {4, 2, 1, false, "4 credit cards, 2 addresses, 1 other"},
      {4, 2, 1, true, "4 credit cards, 2 addresses, 1 other (synced)"},
  };

  for (const TestCase& test_case : kTestCases) {
    AutofillCounter::AutofillResult result(
        &counter, test_case.num_suggestions, test_case.num_credit_cards,
        test_case.num_addresses, test_case.sync_enabled);

    SCOPED_TRACE(
        base::StringPrintf("Test params: %d credit card(s), "
                           "%d address(es), %d suggestion(s).",
                           test_case.num_credit_cards, test_case.num_addresses,
                           test_case.num_suggestions));

    base::string16 output = browsing_data::GetCounterTextFromResult(&result);
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
}

// Tests the output of the Passwords counter.
TEST_F(BrowsingDataUtilsTest, PasswordsCounterResult) {
  scoped_refptr<password_manager::TestPasswordStore> store(
      new password_manager::TestPasswordStore());
  PasswordsCounter counter(
      scoped_refptr<password_manager::PasswordStore>(store), nullptr);

  const struct TestCase {
    int num_passwords;
    int is_synced;
    std::vector<std::string> domain_examples;
    std::string expected_output;
  } kTestCases[] = {
      {0, false, {}, "None"},
      {0, true, {}, "None"},
      {1, false, {"domain1.com"}, "1 password (for domain1.com)"},
      {1, true, {"domain1.com"}, "1 password (for domain1.com, synced)"},
      {5,
       false,
       {"domain1.com", "domain2.com", "domain3.com", "domain4.com"},
       "5 passwords (for domain1.com, domain2.com, and 3 more)"},
      {5,
       true,
       {"domain1.com", "domain2.com", "domain3.com", "domain4.com",
        "domain5.com"},
       "5 passwords (for domain1.com, domain2.com, and 3 more, synced)"},
  };

  for (const TestCase& test_case : kTestCases) {
    PasswordsCounter::PasswordsResult result(&counter, test_case.num_passwords,
                                             test_case.is_synced,
                                             test_case.domain_examples);
    SCOPED_TRACE(base::StringPrintf("Test params: %d password(s), %d is_synced",
                                    test_case.num_passwords,
                                    test_case.is_synced));
    base::string16 output = browsing_data::GetCounterTextFromResult(&result);
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
  store->ShutdownOnUIThread();
}

// Tests the output of the History counter.
TEST_F(BrowsingDataUtilsTest, HistoryCounterResult) {
  history::HistoryService history_service;
  HistoryCounter counter(&history_service,
                         HistoryCounter::GetUpdatedWebHistoryServiceCallback(),
                         nullptr);
  counter.Init(prefs(), ClearBrowsingDataTab::ADVANCED, base::DoNothing());

  const struct TestCase {
    int num_history;
    int is_sync_enabled;
    int has_sync_visits;
    std::string expected_output;
  } kTestCases[] = {
      // No sync, no synced visits:
      {0, false, false, "None"},
      {1, false, false, "1 item"},
      {5, false, false, "5 items"},
      // Sync but not synced visits:
      {0, true, false, "None"},
      {1, true, false, "1 item"},
      {5, true, false, "5 items"},
      // Sync and synced visits:
      {0, true, true, "At least 1 item on synced devices"},
      {1, true, true, "1 item (and more on synced devices)"},
      {5, true, true, "5 items (and more on synced devices)"},
  };

  for (const TestCase& test_case : kTestCases) {
    HistoryCounter::HistoryResult result(&counter, test_case.num_history,
                                         test_case.is_sync_enabled,
                                         test_case.has_sync_visits);
    SCOPED_TRACE(
        base::StringPrintf("Test params: %d history, %d has_synced_visits",
                           test_case.num_history, test_case.has_sync_visits));
    base::string16 output = browsing_data::GetCounterTextFromResult(&result);
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
}

}  // namespace browsing_data
