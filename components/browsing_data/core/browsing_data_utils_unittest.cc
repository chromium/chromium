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
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browsing_data/core/counters/autofill_counter.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/counters/history_counter.h"
#include "components/browsing_data/core/counters/passwords_counter.h"
#include "components/browsing_data/core/features.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/features.h"
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

// A counter that is only used to produce results for the Skills data type.
class FakeSkillsCounter : public BrowsingDataCounter {
 public:
  const char* GetPrefName() const override { return prefs::kDeleteSkills; }

 private:
  void Count() override {}
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

#if BUILDFLAG(IS_CHROMEOS)
// Tests the complex output of the Autofill counter.
TEST_F(BrowsingDataUtilsTest,
       AutofillCounterResultWithReplaceSyncPromosWithSignInPromosDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      syncer::kReplaceSyncPromosWithSignInPromos);

  autofill::TestPersonalDataManager test_personal_data_manager;
  AutofillCounter counter(&test_personal_data_manager,
                          base::MakeRefCounted<FakeWebDataService>(),
                          /*entity_data_manager=*/nullptr,
                          /*entity_suppression_manager=*/nullptr, nullptr);

  // Test cases are grouped by the shape of the resulting string. Each group
  // covers singular and plural counts, the different data types that map onto
  // the "Other data" / "and more" bucket, and the synced variant.
  const struct TestCase {
    int num_credit_cards;
    int num_addresses;
    int num_suggestions;
    int num_entities;
    int num_suppressions;
    bool sync_enabled;
    std::string expected_output;
  } kTestCases[] = {
      // "None"
      {0, 0, 0, 0, 0, false, "None"},
      {0, 0, 0, 0, 0, true, "None"},
      // "N payment methods"
      {1, 0, 0, 0, 0, false, "1 payment method"},
      {4, 0, 0, 0, 0, false, "4 payment methods"},
      {4, 0, 0, 0, 0, true, "4 payment methods (synced)"},
      // "N addresses"
      {0, 1, 0, 0, 0, false, "1 address"},
      {0, 5, 0, 0, 0, false, "5 addresses"},
      {0, 5, 0, 0, 0, true, "5 addresses (synced)"},
      // "N payment methods, N addresses"
      {1, 1, 0, 0, 0, false, "1 payment method, 1 address"},
      {4, 7, 0, 0, 0, false, "4 payment methods, 7 addresses"},
      {4, 7, 0, 0, 0, true, "4 payment methods, 7 addresses (synced)"},
      // "Other data": autocomplete suggestions, Autofill AI entities and entity
      // suppressions are not counted individually.
      {0, 0, 1, 0, 0, false, "Other data"},
      {0, 0, 0, 1, 0, false, "Other data"},
      {0, 0, 0, 0, 1, false, "Other data"},
      {0, 0, 2, 1, 3, false, "Other data"},
      {0, 0, 2, 1, 3, true, "Other data (synced)"},
      // "N payment methods, and more"
      {1, 0, 1, 0, 0, false, "1 payment method, and more"},
      {3, 0, 0, 2, 0, false, "3 payment methods, and more"},
      {3, 0, 0, 0, 1, false, "3 payment methods, and more"},
      {3, 0, 2, 1, 3, true, "3 payment methods, and more (synced)"},
      // "N addresses, and more"
      {0, 1, 1, 0, 0, false, "1 address, and more"},
      {0, 5, 0, 2, 0, false, "5 addresses, and more"},
      {0, 5, 0, 0, 1, false, "5 addresses, and more"},
      {0, 5, 2, 1, 3, true, "5 addresses, and more (synced)"},
      // "N payment methods, N addresses, and more"
      {1, 1, 1, 0, 0, false, "1 payment method, 1 address, and more"},
      {9, 6, 0, 5, 0, false, "9 payment methods, 6 addresses, and more"},
      {9, 6, 0, 0, 4, false, "9 payment methods, 6 addresses, and more"},
      {9, 6, 3, 5, 4, true,
       "9 payment methods, 6 addresses, and more (synced)"},
  };

  for (const TestCase& test_case : kTestCases) {
    AutofillCounter::AutofillResult result(
        &counter, test_case.num_suggestions, test_case.num_credit_cards,
        test_case.num_addresses, test_case.num_entities,
        test_case.num_suppressions, test_case.sync_enabled);

    SCOPED_TRACE(base::StringPrintf(
        "Test params: %d payment method(s), %d address(es), %d suggestion(s), "
        "%d entities, %d suppressions",
        test_case.num_credit_cards, test_case.num_addresses,
        test_case.num_suggestions, test_case.num_entities,
        test_case.num_suppressions));

    std::u16string output = browsing_data::GetCounterTextFromResult(&result);
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(BrowsingDataUtilsTest,
       AutofillCounterResultWithReplaceSyncPromosWithSignInPromosEnabled) {
  base::test::ScopedFeatureList feature_list(
      syncer::kReplaceSyncPromosWithSignInPromos);

  autofill::TestPersonalDataManager test_personal_data_manager;
  AutofillCounter counter(&test_personal_data_manager,
                          base::MakeRefCounted<FakeWebDataService>(),
                          /*entity_data_manager=*/nullptr,
                          /*entity_suppression_manager=*/nullptr, nullptr);

  // Test cases are grouped by the shape of the resulting string. Each group
  // covers singular and plural counts, the different data types that map onto
  // the "Other data" / "and more" bucket, and the synced variant.
  const struct TestCase {
    int num_credit_cards;
    int num_addresses;
    int num_suggestions;
    int num_entities;
    int num_suppressions;
    bool sync_enabled;
    std::string expected_output;
  } kTestCases[] = {
      // "None"
      {0, 0, 0, 0, 0, false, "None"},
      {0, 0, 0, 0, 0, true, "None"},
      // "N payment methods"
      {1, 0, 0, 0, 0, false, "1 payment method"},
      {4, 0, 0, 0, 0, false, "4 payment methods"},
      {4, 0, 0, 0, 0, true, "4 payment methods"},
      // "N addresses"
      {0, 1, 0, 0, 0, false, "1 address"},
      {0, 5, 0, 0, 0, false, "5 addresses"},
      {0, 5, 0, 0, 0, true, "5 addresses"},
      // "N payment methods, N addresses"
      {1, 1, 0, 0, 0, false, "1 payment method, 1 address"},
      {4, 7, 0, 0, 0, false, "4 payment methods, 7 addresses"},
      {4, 7, 0, 0, 0, true, "4 payment methods, 7 addresses"},
      // "Other data": autocomplete suggestions, Autofill AI entities and entity
      // suppressions are not counted individually.
      {0, 0, 1, 0, 0, false, "Other data"},
      {0, 0, 0, 1, 0, false, "Other data"},
      {0, 0, 0, 0, 1, false, "Other data"},
      {0, 0, 2, 1, 3, false, "Other data"},
      {0, 0, 2, 1, 3, true, "Other data"},
      // "N payment methods, and more"
      {1, 0, 1, 0, 0, false, "1 payment method, and more"},
      {3, 0, 0, 2, 0, false, "3 payment methods, and more"},
      {3, 0, 0, 0, 1, false, "3 payment methods, and more"},
      {3, 0, 2, 1, 3, true, "3 payment methods, and more"},
      // "N addresses, and more"
      {0, 1, 1, 0, 0, false, "1 address, and more"},
      {0, 5, 0, 2, 0, false, "5 addresses, and more"},
      {0, 5, 0, 0, 1, false, "5 addresses, and more"},
      {0, 5, 2, 1, 3, true, "5 addresses, and more"},
      // "N payment methods, N addresses, and more"
      {1, 1, 1, 0, 0, false, "1 payment method, 1 address, and more"},
      {9, 6, 0, 5, 0, false, "9 payment methods, 6 addresses, and more"},
      {9, 6, 0, 0, 4, false, "9 payment methods, 6 addresses, and more"},
      {9, 6, 3, 5, 4, true, "9 payment methods, 6 addresses, and more"},
  };

  for (const TestCase& test_case : kTestCases) {
    AutofillCounter::AutofillResult result(
        &counter, test_case.num_suggestions, test_case.num_credit_cards,
        test_case.num_addresses, test_case.num_entities,
        test_case.num_suppressions, test_case.sync_enabled);

    SCOPED_TRACE(base::StringPrintf(
        "Test params: %d payment method(s), %d address(es), %d suggestion(s), "
        "%d entities, %d suppressions",
        test_case.num_credit_cards, test_case.num_addresses,
        test_case.num_suggestions, test_case.num_entities,
        test_case.num_suppressions));

    std::u16string output = browsing_data::GetCounterTextFromResult(&result);
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
}

// Tests the output of the Passwords counter.
TEST_F(BrowsingDataUtilsTest, PasswordsCounterResult) {
  auto store = base::MakeRefCounted<password_manager::TestPasswordStore>();
  store->Init();
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
                         /*web_history_service=*/nullptr, nullptr);
  counter.Init(prefs(), base::DoNothing());

  const struct TestCase {
    int unique_domains;
    int is_sync_enabled;
    int has_sync_visits;
    std::string last_visited_domain;
    std::string expected_output;
  } kTestCases[] = {
      // No sync, no synced visits:
      {0, false, false, "", "None"},
      {1, false, false, "google.com", "From google.com"},
      {5, false, false, "google.com", "From google.com + 4 sites"},
      // Sync but not synced visits:
      {0, true, false, "", "None"},
      {1, true, false, "google.com", "From google.com"},
      {5, true, false, "google.com", "From google.com + 4 sites"},
      // Sync and synced visits:
      {0, true, true, "", "From at least 1 site on synced devices"},
      {1, true, true, "google.com", "From google.com (more on synced devices)"},
      {5, true, true, "google.com",
       "From google.com + 4 sites (more on synced devices)"},
  };

  for (const TestCase& test_case : kTestCases) {
    HistoryCounter::HistoryResult result(
        &counter, test_case.unique_domains, test_case.is_sync_enabled,
        test_case.has_sync_visits, test_case.last_visited_domain);
    SCOPED_TRACE(base::StringPrintf(
        "Test params: %d unique domains, %d has_synced_visits, "
        "last visted domain is %s",
        test_case.unique_domains, test_case.has_sync_visits,
        test_case.last_visited_domain));
    std::u16string output = browsing_data::GetCounterTextFromResult(&result);
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
}

// Tests the output of the Skills counter.
TEST_F(BrowsingDataUtilsTest, SkillsCounterResult) {
  FakeSkillsCounter counter;

  const struct TestCase {
    int num_skills;
    bool sync_enabled;
    std::string expected_output;
  } kTestCases[] = {
      {0, false, "None"},
      {1, false, "1 skill"},
      {5, false, "5 skills"},
      {0, true, "None"},
      {1, true, "1 skill (synced)"},
      {5, true, "5 skills (synced)"},
  };

  for (const TestCase& test_case : kTestCases) {
    BrowsingDataCounter::SyncResult result(&counter, test_case.num_skills,
                                           test_case.sync_enabled);
    SCOPED_TRACE(base::StringPrintf("Test params: %d skill(s), sync: %d",
                                    test_case.num_skills,
                                    test_case.sync_enabled));
    std::u16string output = browsing_data::GetCounterTextFromResult(&result);
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
}

}  // namespace browsing_data
