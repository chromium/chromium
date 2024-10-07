// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/browsing_data_utils.h"

#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browsing_data/core/counters/autofill_counter.h"
#include "components/browsing_data/core/counters/history_counter.h"
#include "components/browsing_data/core/counters/passwords_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_data {

namespace {

class FakeWebDataService : public autofill::AutofillWebDataService {
 public:
  FakeWebDataService()
      : AutofillWebDataService(
            base::MakeRefCounted<WebDatabaseService>(
                base::FilePath(),
                base::SingleThreadTaskRunner::GetCurrentDefault(),
                base::SingleThreadTaskRunner::GetCurrentDefault()),
            base::SingleThreadTaskRunner::GetCurrentDefault()) {}

 protected:
  ~FakeWebDataService() override = default;
};

}  // namespace

class BrowsingDataUtilsTest : public testing::Test {
 public:
  ~BrowsingDataUtilsTest() override = default;

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
  autofill::TestPersonalDataManager test_personal_data_manager;
  AutofillCounter counter(&test_personal_data_manager,
                          base::MakeRefCounted<FakeWebDataService>(), nullptr,
                          nullptr);

  // Test all configurations of zero and nonzero partial results for datatypes.
  // Test singular and plural for each datatype.
  const struct TestCase {
    int num_credit_cards;
    int num_addresses;
    int num_suggestions;
    int num_user_annotation_entries;
    bool sync_enabled;
    std::string expected_output;
  } kTestCases[] = {
      {0, 0, 0, 0, false, "None"},
      {0, 0, 0, 0, true, "None"},
      {1, 0, 0, 0, false, "1 credit card"},
      {0, 5, 0, 0, false, "5 addresses"},
      {0, 0, 1, 0, false, "1 suggestion"},
      {0, 0, 2, 0, false, "2 suggestions"},
      {0, 0, 2, 0, true, "2 suggestions (synced)"},
      {4, 7, 0, 0, false, "4 credit cards, 7 addresses"},
      {4, 7, 0, 0, true, "4 credit cards, 7 addresses (synced)"},
      {3, 0, 9, 0, false, "3 credit cards, 9 other suggestions"},
      {0, 1, 1, 0, false, "1 address, 1 other suggestion"},
      {9, 6, 3, 0, false, "9 credit cards, 6 addresses, 3 others"},
      {4, 2, 1, 0, false, "4 credit cards, 2 addresses, 1 other"},
      {4, 2, 1, 0, true, "4 credit cards, 2 addresses, 1 other (synced)"},
      {1, 0, 0, 1, false, "1 credit card; 1 suggestion (device only)"},
      {1, 1, 0, 2, true,
       "1 credit card, 1 address (synced); 2 suggestions (device only)"},
  };

  for (const TestCase& test_case : kTestCases) {
    AutofillCounter::AutofillResult result(
        &counter, test_case.num_suggestions, test_case.num_credit_cards,
        test_case.num_addresses, test_case.num_user_annotation_entries,
        test_case.sync_enabled);

    SCOPED_TRACE(
        base::StringPrintf("Test params: %d credit card(s), "
                           "%d address(es), %d suggestion(s).",
                           test_case.num_credit_cards, test_case.num_addresses,
                           test_case.num_suggestions));

    std::u16string output = browsing_data::GetCounterTextFromResult(&result);
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
}

// Tests the output of the Passwords counter.
TEST_F(BrowsingDataUtilsTest, PasswordsCounterResult) {
  auto store = base::MakeRefCounted<password_manager::TestPasswordStore>();
  store->Init(prefs(), /*affiliated_match_helper=*/nullptr);
  PasswordsCounter counter(
      /*profile_store=*/scoped_refptr<password_manager::PasswordStoreInterface>(
          store),
      /*account_store=*/nullptr,
      /*pref_service=*/nullptr,
      /*sync_service=*/nullptr);

  // Use a separate struct for input to make test cases easier to read after
  // auto formatting.
  struct TestInput {
    int num_passwords;
    int num_account_passwords;
    int is_synced;
    std::vector<std::string> domain_examples;
    std::vector<std::string> account_domain_examples;
  };
  const struct TestCase {
    TestInput input;
    std::string expected_output;
  } kTestCases[] = {
      {{0, 0, false, {}, {}}, "None"},
      {{0, 0, true, {}, {}}, "None"},
      {{1, 0, false, {"a.com"}, {}}, "1 password (for a.com)"},
      {{1, 0, true, {"a.com"}, {}}, "1 password (for a.com, synced)"},
      {{5, 0, false, {"a.com", "b.com", "c.com", "d.com"}, {}},
       "5 passwords (for a.com, b.com, and 3 more)"},
      {{2, 0, false, {"a.com", "b.com"}, {}}, "2 passwords (for a.com, b.com)"},
      {{5, 0, true, {"a.com", "b.com", "c.com", "d.com", "e.com"}, {}},
       "5 passwords (for a.com, b.com, and 3 more, synced)"},
      {{0, 1, false, {}, {"a.com"}}, "1 password in your account (for a.com)"},
      {{0, 2, false, {}, {"a.com", "b.com"}},
       "2 passwords in your account (for a.com, b.com)"},
      {{0, 3, false, {}, {"a.com", "b.com", "c.com"}},
       "3 passwords in your account (for a.com, b.com, and 1 more)"},
      {{2, 1, false, {"a.com", "b.com"}, {"c.com"}},
       "2 passwords (for a.com, b.com); 1 password in your account (for "
       "c.com)"},
  };

  for (const TestCase& test_case : kTestCases) {
    auto& input = test_case.input;
    PasswordsCounter::PasswordsResult result(
        &counter, input.num_passwords, input.num_account_passwords,
        input.is_synced, input.domain_examples, input.account_domain_examples);
    SCOPED_TRACE(base::StringPrintf(
        "Test params: %d password(s), %d account password(s), %d is_synced",
        input.num_passwords, input.num_account_passwords, input.is_synced));
    std::u16string output = browsing_data::GetCounterTextFromResult(&result);
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
    std::u16string output = browsing_data::GetCounterTextFromResult(&result);
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
}

}  // namespace browsing_data
