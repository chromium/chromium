// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_local_data_batch_uploader.h"

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/service/local_data_description.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

constexpr char kNumUploadsMetric[] = "Sync.PasswordsBatchUpload.Count";

// Arbitrary.
constexpr base::Time kDate =
    base::Time::FromMillisecondsSinceUnixEpoch(1721351144088);

// Compares PasswordForms ignoring in_store.
MATCHER_P(MatchesForm, expected, "") {
  PasswordForm arg_copy = arg;
  arg_copy.in_store = expected.in_store;
  return arg_copy == expected;
}

PasswordForm CreatePasswordForm(const std::string& url) {
  PasswordForm form;
  form.signon_realm = url;
  form.url = GURL(form.signon_realm);
  form.username_value = u"username";
  form.password_value = u"password";
  return form;
}

// Extension of TestPasswordStore that allows controlling the value of
// IsAbleToSavePasswords() (without actually having other methods honor it).
class FakePasswordStore : public TestPasswordStore {
 public:
  explicit FakePasswordStore(password_manager::IsAccountStore is_account_store)
      : TestPasswordStore(is_account_store) {}

  // PasswordStoreInterface implementation.
  bool IsAbleToSavePasswords() const override { return able_to_save_; }

  void SetAbleToSavePasswords(bool able_to_save) {
    able_to_save_ = able_to_save;
  }

 private:
  ~FakePasswordStore() override = default;

  bool able_to_save_ = true;
};

class PasswordLocalDataBatchUploaderTest : public ::testing::Test {
 public:
  PasswordLocalDataBatchUploaderTest() {
    profile_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
    account_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
  }

  ~PasswordLocalDataBatchUploaderTest() override {
    RunUntilIdle();
    account_store_->ShutdownOnUIThread();
    profile_store_->ShutdownOnUIThread();
  }

  FakePasswordStore* profile_store() { return profile_store_.get(); }

  FakePasswordStore* account_store() { return account_store_.get(); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<FakePasswordStore> profile_store_ =
      base::MakeRefCounted<FakePasswordStore>(IsAccountStore(false));
  scoped_refptr<FakePasswordStore> account_store_ =
      base::MakeRefCounted<FakePasswordStore>(IsAccountStore(true));
};

TEST_F(PasswordLocalDataBatchUploaderTest, DescriptionEmptyIfAccountStoreNull) {
  base::test::TestFuture<void> wait_add;
  profile_store()->AddLogin(CreatePasswordForm("http://local.com"),
                            wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordLocalDataBatchUploader uploader(profile_store(), nullptr);
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_EQ(description.Get().item_count, 0u);
  EXPECT_EQ(description.Get().domain_count, 0u);
  EXPECT_EQ(description.Get().domains, std::vector<std::string>{});
  EXPECT_EQ(description.Get().local_data_models,
            std::vector<syncer::LocalDataItemModel>{});
}

// This should not happen outside of tests, it's just tested for symmetry with
// the test above.
TEST_F(PasswordLocalDataBatchUploaderTest, DescriptionEmptyIfProfileStoreNull) {
  base::test::TestFuture<void> wait_add;
  account_store()->AddLogin(CreatePasswordForm("http://account.com"),
                            wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordLocalDataBatchUploader uploader(nullptr, account_store());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_EQ(description.Get().item_count, 0u);
  EXPECT_EQ(description.Get().domain_count, 0u);
  EXPECT_EQ(description.Get().domains, std::vector<std::string>{});
  EXPECT_EQ(description.Get().local_data_models,
            std::vector<syncer::LocalDataItemModel>{});
}

TEST_F(PasswordLocalDataBatchUploaderTest,
       DescriptionEmptyIfAccountStoreCannotSave) {
  base::test::TestFuture<void> wait_add;
  profile_store()->AddLogin(CreatePasswordForm("http://local.com"),
                            wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  account_store()->AddLogin(CreatePasswordForm("http://account.com"),
                            wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  account_store()->SetAbleToSavePasswords(false);
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_EQ(description.Get().item_count, 0u);
  EXPECT_EQ(description.Get().domain_count, 0u);
  EXPECT_EQ(description.Get().domains, std::vector<std::string>{});
  EXPECT_EQ(description.Get().local_data_models,
            std::vector<syncer::LocalDataItemModel>{});
}

TEST_F(PasswordLocalDataBatchUploaderTest,
       DescriptionContainsOnlyLocalPasswords) {
  base::test::TestFuture<void> wait_add;
  profile_store()->AddLogin(CreatePasswordForm("http://local.com"),
                            wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  account_store()->AddLogin(CreatePasswordForm("http://account.com"),
                            wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_EQ(description.Get().item_count, 1u);
  EXPECT_EQ(description.Get().domain_count, 1u);
  EXPECT_EQ(description.Get().domains, std::vector<std::string>{"local.com"});
  EXPECT_EQ(description.Get().local_data_models.size(), 0u);
}

TEST_F(PasswordLocalDataBatchUploaderTest,
       DescriptionCanBeQueriedBySimultaneousRequests) {
  // Add one local password and one account password.
  base::test::TestFuture<void> wait_add;
  PasswordForm local_password = CreatePasswordForm("http://local.com");
  profile_store()->AddLogin(local_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordForm account_password = CreatePasswordForm("http://account.com");
  account_store()->AddLogin(account_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());
  base::test::TestFuture<syncer::LocalDataDescription> first_description;
  base::test::TestFuture<syncer::LocalDataDescription> second_description;

  uploader.GetLocalDataDescription(first_description.GetCallback());
  uploader.GetLocalDataDescription(second_description.GetCallback());

  EXPECT_EQ(first_description.Get().item_count, 1u);
  EXPECT_EQ(first_description.Get().domain_count, 1u);
  EXPECT_EQ(first_description.Get().domains,
            std::vector<std::string>{"local.com"});
  EXPECT_EQ(first_description.Get().local_data_models.size(), 0u);
  EXPECT_EQ(second_description.Get(), first_description.Get());
}

TEST_F(PasswordLocalDataBatchUploaderTest, MigrationNoOpsIfAccountStoreNull) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> wait_add;
  PasswordForm local_password = CreatePasswordForm("http://local.com");
  profile_store()->AddLogin(local_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordLocalDataBatchUploader uploader(profile_store(), nullptr);

  uploader.TriggerLocalDataMigration();
  RunUntilIdle();

  EXPECT_THAT(profile_store()->stored_passwords(),
              UnorderedElementsAre(
                  Pair(local_password.signon_realm,
                       UnorderedElementsAre(MatchesForm(local_password)))));
  histogram_tester.ExpectTotalCount(kNumUploadsMetric, 0);
}

// This should not happen outside of tests, it's just tested for symmetry with
// the test above.
TEST_F(PasswordLocalDataBatchUploaderTest, MigrationNoOpsIfProfileStoreNull) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> wait_add;
  PasswordForm account_password = CreatePasswordForm("http://account.com");
  account_store()->AddLogin(account_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordLocalDataBatchUploader uploader(nullptr, account_store());

  uploader.TriggerLocalDataMigration();
  RunUntilIdle();

  EXPECT_THAT(account_store()->stored_passwords(),
              UnorderedElementsAre(
                  Pair(account_password.signon_realm,
                       UnorderedElementsAre(MatchesForm(account_password)))));
  histogram_tester.ExpectTotalCount(kNumUploadsMetric, 0);
}

TEST_F(PasswordLocalDataBatchUploaderTest,
       MigrationNoOpsIfAccountStoreCannotSave) {
  // Add one local password and one account password.
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> wait_add;
  PasswordForm local_password = CreatePasswordForm("http://local.com");
  profile_store()->AddLogin(local_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordForm account_password = CreatePasswordForm("http://account.com");
  account_store()->AddLogin(account_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  account_store()->SetAbleToSavePasswords(false);
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());

  uploader.TriggerLocalDataMigration();
  RunUntilIdle();

  EXPECT_THAT(profile_store()->stored_passwords(),
              UnorderedElementsAre(
                  Pair(local_password.signon_realm,
                       UnorderedElementsAre(MatchesForm(local_password)))));
  EXPECT_THAT(account_store()->stored_passwords(),
              UnorderedElementsAre(
                  Pair(account_password.signon_realm,
                       UnorderedElementsAre(MatchesForm(account_password)))));
  histogram_tester.ExpectTotalCount(kNumUploadsMetric, 0);
}

TEST_F(PasswordLocalDataBatchUploaderTest, MigrationUploadsLocalPassword) {
  // Add one local password and one account password.
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> wait_add;
  PasswordForm local_password = CreatePasswordForm("http://local.com");
  profile_store()->AddLogin(local_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordForm account_password = CreatePasswordForm("http://account.com");
  account_store()->AddLogin(account_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());

  uploader.TriggerLocalDataMigration();
  RunUntilIdle();

  EXPECT_THAT(profile_store()->stored_passwords(), IsEmpty());
  EXPECT_THAT(account_store()->stored_passwords(),
              UnorderedElementsAre(
                  Pair(local_password.signon_realm,
                       UnorderedElementsAre(MatchesForm(local_password))),
                  Pair(account_password.signon_realm,
                       UnorderedElementsAre(MatchesForm(account_password)))));
  histogram_tester.ExpectUniqueSample(kNumUploadsMetric, 1, 1);
}

TEST_F(PasswordLocalDataBatchUploaderTest,
       MigrationNoOpsIfOngoingMigrationAlreadyExists) {
  // Add one local password and one account password.
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> wait_add;
  PasswordForm local_password = CreatePasswordForm("http://local.com");
  profile_store()->AddLogin(local_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordForm account_password = CreatePasswordForm("http://account.com");
  account_store()->AddLogin(account_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());
  uploader.TriggerLocalDataMigration();
  ASSERT_TRUE(uploader.trigger_local_data_migration_ongoing_for_test());

  // A second migration is triggered.
  uploader.TriggerLocalDataMigration();
  RunUntilIdle();

  // The first migration should upload the local password, and second migration
  // should be ignored.
  EXPECT_THAT(profile_store()->stored_passwords(), IsEmpty());
  EXPECT_THAT(account_store()->stored_passwords(),
              UnorderedElementsAre(
                  Pair(local_password.signon_realm,
                       UnorderedElementsAre(MatchesForm(local_password))),
                  Pair(account_password.signon_realm,
                       UnorderedElementsAre(MatchesForm(account_password)))));
  // Only one migration should have been triggered.
  histogram_tester.ExpectUniqueSample(kNumUploadsMetric, 1, 1);
}

TEST_F(PasswordLocalDataBatchUploaderTest,
       DescriptionEmptyIfOngoingMigrationAlreadyExists) {
  // Add one local password and one account password.
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> wait_add;
  PasswordForm local_password = CreatePasswordForm("http://local.com");
  profile_store()->AddLogin(local_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordForm account_password = CreatePasswordForm("http://account.com");
  account_store()->AddLogin(account_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());
  uploader.TriggerLocalDataMigration();
  ASSERT_TRUE(uploader.trigger_local_data_migration_ongoing_for_test());

  // During an ongoing migration, the returned description should be empty.
  base::test::TestFuture<syncer::LocalDataDescription> description;
  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_EQ(description.Get().item_count, 0u);
  EXPECT_EQ(description.Get().domain_count, 0u);
  EXPECT_EQ(description.Get().domains, std::vector<std::string>{});

  // Complete the migration before destroying the uploader to avoid crashes.
  RunUntilIdle();
}

TEST_F(PasswordLocalDataBatchUploaderTest, MigrationRemovesDuplicate) {
  // Add the exact some password to both stores.
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> wait_add;
  PasswordForm duplicate_password = CreatePasswordForm("http://duplicate.com");
  profile_store()->AddLogin(duplicate_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  account_store()->AddLogin(duplicate_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());

  uploader.TriggerLocalDataMigration();
  RunUntilIdle();

  EXPECT_THAT(profile_store()->stored_passwords(), IsEmpty());
  EXPECT_THAT(account_store()->stored_passwords(),
              UnorderedElementsAre(
                  Pair(duplicate_password.signon_realm,
                       UnorderedElementsAre(MatchesForm(duplicate_password)))));
  histogram_tester.ExpectUniqueSample(kNumUploadsMetric, 0, 1);
}

TEST_F(PasswordLocalDataBatchUploaderTest,
       MigrationKeepsAccountPasswordIfMoreRecentInConflict) {
  // Add 2 versions of the same credential to each store, which differ in
  // password_value. The account version is more recent.
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> wait_add;
  PasswordForm old_local_password = CreatePasswordForm("http://conflict.com");
  old_local_password.password_value = u"older version";
  old_local_password.date_last_used = kDate;
  profile_store()->AddLogin(old_local_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordForm new_account_password = old_local_password;
  new_account_password.password_value = u"newer version";
  new_account_password.date_last_used = kDate + base::Days(1);
  account_store()->AddLogin(new_account_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());

  uploader.TriggerLocalDataMigration();
  RunUntilIdle();

  EXPECT_THAT(profile_store()->stored_passwords(), IsEmpty());
  EXPECT_THAT(account_store()->stored_passwords(),
              UnorderedElementsAre(Pair(
                  new_account_password.signon_realm,
                  UnorderedElementsAre(MatchesForm(new_account_password)))));
  histogram_tester.ExpectUniqueSample(kNumUploadsMetric, 0, 1);
}

TEST_F(PasswordLocalDataBatchUploaderTest,
       MigrationKeepsLocalPasswordIfMoreRecentInConflict) {
  // Add 2 versions of the same credential to each store, which differ in
  // password_value. The local version is more recent.
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> wait_add;
  PasswordForm old_account_password = CreatePasswordForm("http://conflict.com");
  old_account_password.password_value = u"older version";
  old_account_password.date_last_used = kDate;
  account_store()->AddLogin(old_account_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordForm new_local_password = old_account_password;
  new_local_password.password_value = u"newer version";
  new_local_password.date_last_used = kDate + base::Days(1);
  profile_store()->AddLogin(new_local_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());

  uploader.TriggerLocalDataMigration();
  RunUntilIdle();

  EXPECT_THAT(profile_store()->stored_passwords(), IsEmpty());
  EXPECT_THAT(account_store()->stored_passwords(),
              UnorderedElementsAre(
                  Pair(new_local_password.signon_realm,
                       UnorderedElementsAre(MatchesForm(new_local_password)))));
  histogram_tester.ExpectUniqueSample(kNumUploadsMetric, 1, 1);
}

TEST_F(PasswordLocalDataBatchUploaderTest,
       MigrationUsesOtherTimestampsAsFallbackInConflict) {
  // Add 2 versions of the same credential to each store, which differ in
  // password_value. One uses the "created" timestamp, the other, "modified".
  // The local version is newer.
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> wait_add;
  PasswordForm old_account_password = CreatePasswordForm("http://conflict.com");
  old_account_password.password_value = u"older version";
  old_account_password.date_created = kDate;
  account_store()->AddLogin(old_account_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordForm new_local_password = old_account_password;
  new_local_password.password_value = u"newer version";
  new_local_password.date_password_modified = kDate + base::Days(1);
  profile_store()->AddLogin(new_local_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());

  uploader.TriggerLocalDataMigration();
  RunUntilIdle();

  EXPECT_THAT(profile_store()->stored_passwords(), IsEmpty());
  EXPECT_THAT(account_store()->stored_passwords(),
              UnorderedElementsAre(
                  Pair(new_local_password.signon_realm,
                       UnorderedElementsAre(MatchesForm(new_local_password)))));
  histogram_tester.ExpectUniqueSample(kNumUploadsMetric, 1, 1);
}

TEST_F(PasswordLocalDataBatchUploaderTest,
       MigrationUploadsMultiplePasswordsAndRecordsMetricOnce) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> wait_add;
  profile_store()->AddLogin(CreatePasswordForm("http://local1.com"),
                            wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  profile_store()->AddLogin(CreatePasswordForm("http://local2.com"),
                            wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  profile_store()->AddLogin(CreatePasswordForm("http://local3.com"),
                            wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());

  uploader.TriggerLocalDataMigration();
  RunUntilIdle();

  EXPECT_THAT(profile_store()->stored_passwords(), IsEmpty());
  EXPECT_THAT(account_store()->stored_passwords(), SizeIs(3));
  histogram_tester.ExpectUniqueSample(kNumUploadsMetric, 3, 1);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class PasswordLocalDataBatchUploaderWithBatchUploadDesktopTest
    : public PasswordLocalDataBatchUploaderTest {
 public:
  // Create `count` local passwords and returns them as a list.
  std::vector<PasswordForm> CreatePasswordFormsInStore(
      int count,
      FakePasswordStore* store) {
    base::test::TestFuture<void> wait_add;
    std::vector<PasswordForm> passwords;
    std::string store_string = store->IsAccountStore() ? "account" : "local";
    for (int i = 0; i < count; ++i) {
      PasswordForm password = CreatePasswordForm(
          base::StringPrintf("http://%s%i.com", store_string, i));
      passwords.push_back(password);
      store->AddLogin(password, wait_add.GetCallback());
      EXPECT_TRUE(wait_add.WaitAndClear());
    }
    return passwords;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kBatchUploadDesktop};
};

TEST_F(PasswordLocalDataBatchUploaderWithBatchUploadDesktopTest,
       DescriptionContainsOnlyLocalPasswords) {
  base::test::TestFuture<void> wait_add;
  profile_store()->AddLogin(CreatePasswordForm("http://local.com"),
                            wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  account_store()->AddLogin(CreatePasswordForm("http://account.com"),
                            wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_EQ(description.Get().item_count, 1u);
  EXPECT_EQ(description.Get().domain_count, 1u);
  EXPECT_EQ(description.Get().domains, std::vector<std::string>{"local.com"});
  ASSERT_EQ(description.Get().local_data_models.size(), 1u);
  EXPECT_EQ(description.Get().local_data_models[0].title, "local.com");
  EXPECT_EQ(description.Get().local_data_models[0].subtitle, "username");
}

TEST_F(PasswordLocalDataBatchUploaderWithBatchUploadDesktopTest,
       DescriptionCanBeQueriedBySimultaneousRequests) {
  // Add one local password and one account password.
  base::test::TestFuture<void> wait_add;
  PasswordForm local_password = CreatePasswordForm("http://local.com");
  profile_store()->AddLogin(local_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordForm account_password = CreatePasswordForm("http://account.com");
  account_store()->AddLogin(account_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());
  base::test::TestFuture<syncer::LocalDataDescription> first_description;
  base::test::TestFuture<syncer::LocalDataDescription> second_description;

  uploader.GetLocalDataDescription(first_description.GetCallback());
  uploader.GetLocalDataDescription(second_description.GetCallback());

  EXPECT_EQ(first_description.Get().item_count, 1u);
  EXPECT_EQ(first_description.Get().domain_count, 1u);
  EXPECT_EQ(first_description.Get().domains,
            std::vector<std::string>{"local.com"});
  ASSERT_EQ(first_description.Get().local_data_models.size(), 1u);
  EXPECT_EQ(first_description.Get().local_data_models[0].title, "local.com");
  EXPECT_EQ(first_description.Get().local_data_models[0].subtitle, "username");
  EXPECT_EQ(second_description.Get(), first_description.Get());
}

TEST_F(PasswordLocalDataBatchUploaderWithBatchUploadDesktopTest,
       MigrationUploadsEmptyKeys) {
  base::HistogramTester histogram_tester;
  std::vector<PasswordForm> passwords =
      CreatePasswordFormsInStore(3, profile_store());
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());

  // Trigger upload with an empty list.
  uploader.TriggerLocalDataMigration({});
  RunUntilIdle();

  // All passwords still in profile store.
  EXPECT_THAT(profile_store()->stored_passwords(), SizeIs(3));
  // No password uploaded in account store.
  EXPECT_THAT(account_store()->stored_passwords(), IsEmpty());
  histogram_tester.ExpectUniqueSample(kNumUploadsMetric, 0, 1);
}

TEST_F(PasswordLocalDataBatchUploaderWithBatchUploadDesktopTest,
       MigrationUploadsPartialPasswordsAndRecordsMetricOnce) {
  base::HistogramTester histogram_tester;
  std::vector<PasswordForm> passwords =
      CreatePasswordFormsInStore(3, profile_store());
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());

  // Trigger upload for password 0 and 2.
  uploader.TriggerLocalDataMigration({
      PasswordFormUniqueKey(passwords[0]),
      PasswordFormUniqueKey(passwords[2]),
  });
  RunUntilIdle();

  // Password 1 still in profile_store.
  EXPECT_THAT(profile_store()->stored_passwords(),
              UnorderedElementsAre(
                  Pair(passwords[1].signon_realm,
                       UnorderedElementsAre(MatchesForm(passwords[1])))));
  // Password 0 and 2 in account_store.
  EXPECT_THAT(account_store()->stored_passwords(),
              UnorderedElementsAre(
                  Pair(passwords[0].signon_realm,
                       UnorderedElementsAre(MatchesForm(passwords[0]))),
                  Pair(passwords[2].signon_realm,
                       UnorderedElementsAre(MatchesForm(passwords[2])))));
  histogram_tester.ExpectUniqueSample(kNumUploadsMetric, 2, 1);
}

TEST_F(PasswordLocalDataBatchUploaderWithBatchUploadDesktopTest,
       MigrationUploadsAllPasswordsWithKeys) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> wait_add;
  std::vector<PasswordForm> passwords =
      CreatePasswordFormsInStore(3, profile_store());
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());

  // Trigger upload for all passwords with their keys.
  uploader.TriggerLocalDataMigration({PasswordFormUniqueKey(passwords[0]),
                                      PasswordFormUniqueKey(passwords[1]),
                                      PasswordFormUniqueKey(passwords[2])});
  RunUntilIdle();

  EXPECT_THAT(profile_store()->stored_passwords(), IsEmpty());
  EXPECT_THAT(account_store()->stored_passwords(),
              UnorderedElementsAre(
                  Pair(passwords[0].signon_realm,
                       UnorderedElementsAre(MatchesForm(passwords[0]))),
                  Pair(passwords[1].signon_realm,
                       UnorderedElementsAre(MatchesForm(passwords[1]))),
                  Pair(passwords[2].signon_realm,
                       UnorderedElementsAre(MatchesForm(passwords[2])))));
  histogram_tester.ExpectUniqueSample(kNumUploadsMetric, passwords.size(), 1);
}

TEST_F(PasswordLocalDataBatchUploaderWithBatchUploadDesktopTest,
       MigrationUploadsPasswordsSameKey) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> wait_add;
  std::vector<PasswordForm> passwords =
      CreatePasswordFormsInStore(3, profile_store());
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());

  // Trigger upload for the same key multiple times. Only password 0.
  uploader.TriggerLocalDataMigration({PasswordFormUniqueKey(passwords[0]),
                                      PasswordFormUniqueKey(passwords[0]),
                                      PasswordFormUniqueKey(passwords[0])});
  RunUntilIdle();

  // Only password 0 should be uploaded.
  EXPECT_THAT(profile_store()->stored_passwords(),
              UnorderedElementsAre(
                  Pair(passwords[1].signon_realm,
                       UnorderedElementsAre(MatchesForm(passwords[1]))),
                  Pair(passwords[2].signon_realm,
                       UnorderedElementsAre(MatchesForm(passwords[2])))));
  EXPECT_THAT(account_store()->stored_passwords(),
              UnorderedElementsAre(
                  Pair(passwords[0].signon_realm,
                       UnorderedElementsAre(MatchesForm(passwords[0])))));
  histogram_tester.ExpectUniqueSample(kNumUploadsMetric, 1, 1);
}

TEST_F(PasswordLocalDataBatchUploaderWithBatchUploadDesktopTest,
       MigrationUploadsPasswordsWithUnavailableKey) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> wait_add;
  std::vector<PasswordForm> passwords =
      CreatePasswordFormsInStore(3, profile_store());
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());

  PasswordForm password_not_in_local_store =
      CreatePasswordForm("http://password_not_local.com");
  // Trigger upload for password 0 and a password not in profile store.
  uploader.TriggerLocalDataMigration(
      {PasswordFormUniqueKey(passwords[0]),
       PasswordFormUniqueKey(password_not_in_local_store)});
  RunUntilIdle();

  // Only password 0 should be uploaded, `password_not_in_local_store` ignored.
  EXPECT_THAT(profile_store()->stored_passwords(),
              UnorderedElementsAre(
                  Pair(passwords[1].signon_realm,
                       UnorderedElementsAre(MatchesForm(passwords[1]))),
                  Pair(passwords[2].signon_realm,
                       UnorderedElementsAre(MatchesForm(passwords[2])))));
  EXPECT_THAT(account_store()->stored_passwords(),
              UnorderedElementsAre(
                  Pair(passwords[0].signon_realm,
                       UnorderedElementsAre(MatchesForm(passwords[0])))));
  histogram_tester.ExpectUniqueSample(kNumUploadsMetric, 1, 1);
}

TEST_F(PasswordLocalDataBatchUploaderWithBatchUploadDesktopTest,
       MigrationUploadsPasswordsKeyAlreadyInAccountStore) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> wait_add;
  std::vector<PasswordForm> local_passwords =
      CreatePasswordFormsInStore(3, profile_store());
  std::vector<PasswordForm> account_passwords =
      CreatePasswordFormsInStore(3, account_store());
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());

  // Trigger upload for local password 0 and a account password 0 already in
  // account store.
  uploader.TriggerLocalDataMigration(
      {PasswordFormUniqueKey(local_passwords[0]),
       PasswordFormUniqueKey(account_passwords[0])});
  RunUntilIdle();

  // Only password 0 should be uploaded, `account_passwords[0]` should not be
  // duplicated and appear only once in account store.
  EXPECT_THAT(profile_store()->stored_passwords(),
              UnorderedElementsAre(
                  Pair(local_passwords[1].signon_realm,
                       UnorderedElementsAre(MatchesForm(local_passwords[1]))),
                  Pair(local_passwords[2].signon_realm,
                       UnorderedElementsAre(MatchesForm(local_passwords[2])))));
  EXPECT_THAT(
      account_store()->stored_passwords(),
      UnorderedElementsAre(
          Pair(local_passwords[0].signon_realm,
               UnorderedElementsAre(MatchesForm(local_passwords[0]))),
          Pair(account_passwords[0].signon_realm,
               UnorderedElementsAre(MatchesForm(account_passwords[0]))),
          Pair(account_passwords[1].signon_realm,
               UnorderedElementsAre(MatchesForm(account_passwords[1]))),
          Pair(account_passwords[2].signon_realm,
               UnorderedElementsAre(MatchesForm(account_passwords[2])))));
  histogram_tester.ExpectUniqueSample(kNumUploadsMetric, 1, 1);
}

TEST_F(PasswordLocalDataBatchUploaderWithBatchUploadDesktopTest,
       MigrationUploadsPasswordInAccountAndProfileStore) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> wait_add;
  PasswordForm common_password = CreatePasswordForm("http://common.com");
  profile_store()->AddLogin(common_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  account_store()->AddLogin(common_password, wait_add.GetCallback());
  ASSERT_TRUE(wait_add.WaitAndClear());
  PasswordLocalDataBatchUploader uploader(profile_store(), account_store());

  // Password to migrate already exists in the account store.
  ASSERT_THAT(account_store()->stored_passwords(),
              UnorderedElementsAre(
                  Pair(common_password.signon_realm,
                       UnorderedElementsAre(MatchesForm(common_password)))));

  uploader.TriggerLocalDataMigration({PasswordFormUniqueKey(common_password)});
  RunUntilIdle();

  // Common password should be removed from the profile store and not duplicated
  // in the account store.
  EXPECT_THAT(profile_store()->stored_passwords(), IsEmpty());
  EXPECT_THAT(account_store()->stored_passwords(),
              UnorderedElementsAre(
                  Pair(common_password.signon_realm,
                       UnorderedElementsAre(MatchesForm(common_password)))));
  // No upload recorded.
  histogram_tester.ExpectUniqueSample(kNumUploadsMetric, 0, 1);
}

#endif

}  // namespace
}  // namespace password_manager
