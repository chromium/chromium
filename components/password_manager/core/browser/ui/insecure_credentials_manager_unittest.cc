// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

constexpr char kExampleCom[] = "https://example.com";
constexpr char kExampleOrg[] = "https://example.org";

constexpr char16_t kUsername1[] = u"alice";
constexpr char16_t kUsername2[] = u"bob";

constexpr char16_t kPassword1[] = u"f00b4r";
constexpr char16_t kPassword216[] = u"s3cr3t";

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
constexpr char16_t kWeakPassword1[] = u"123456";
constexpr char16_t kWeakPassword216[] =
    u"abcdabcdabcdabcdabcdabcdabcdabcdabcdabcda";
constexpr char16_t kStrongPassword116[] = u"fnlsr4@cm^mdls@fkspnsg3d";
constexpr char16_t kStrongPassword2[] =
    u"pmsFlsnoab4nsl#losb@skpfnsbkjb^klsnbs!cns";
// Delay in milliseconds.
constexpr int kDelay = 2;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::SizeIs;

struct MockInsecureCredentialsManagerObserver
    : InsecureCredentialsManager::Observer {
  MOCK_METHOD(void, OnInsecureCredentialsChanged, (), (override));
  MOCK_METHOD(void, OnWeakCredentialsChanged, (), (override));
};

using StrictMockInsecureCredentialsManagerObserver =
    ::testing::StrictMock<MockInsecureCredentialsManagerObserver>;

PasswordForm MakeSavedPassword(base::StringPiece signon_realm,
                               base::StringPiece16 username,
                               base::StringPiece16 password,
                               base::StringPiece16 username_element = u"") {
  PasswordForm form;
  form.signon_realm = std::string(signon_realm);
  form.url = GURL(signon_realm);
  form.username_value = std::u16string(username);
  form.password_value = std::u16string(password);
  form.username_element = std::u16string(username_element);
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

LeakCheckCredential MakeLeakCredential(base::StringPiece16 username,
                                       base::StringPiece16 password) {
  return LeakCheckCredential(std::u16string(username),
                             std::u16string(password));
}

class InsecureCredentialsManagerTest : public ::testing::Test {
 protected:
  InsecureCredentialsManagerTest() {
    store_->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  }

  ~InsecureCredentialsManagerTest() override {
    store_->ShutdownOnUIThread();
    task_env_.RunUntilIdle();
  }

  TestPasswordStore& store() { return *store_; }
  SavedPasswordsPresenter& presenter() { return presenter_; }
  InsecureCredentialsManager& provider() { return provider_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

  // Returns saved password if it matches with |signon_realm| and |username|.
  // Otherwise, returns an empty string, because the saved password should never
  // be empty, unless it's a federated credential or "Never save" entry.
  std::u16string GetSavedPasswordForUsername(const std::string& signon_realm,
                                             const std::u16string& username) {
    SavedPasswordsPresenter::SavedPasswordsView saved_passwords =
        presenter_.GetSavedPasswords();
    for (const auto& form : saved_passwords) {
      if (form.signon_realm == signon_realm &&
          form.username_value == username) {
        return form.password_value;
      }
    }
    return std::u16string();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  void AdvanceClock(base::TimeDelta time) { task_env_.AdvanceClock(time); }

 private:
  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  scoped_refptr<TestPasswordStore> store_ =
      base::MakeRefCounted<TestPasswordStore>();
  SavedPasswordsPresenter presenter_{store_};
  InsecureCredentialsManager provider_{&presenter_, store_};
};

}  // namespace

// Tests whether adding and removing an observer works as expected.
TEST_F(InsecureCredentialsManagerTest,
       NotifyObserversAboutCompromisedCredentialChanges) {
  PasswordForm password_form =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  store().AddLogin(password_form);
  RunUntilIdle();

  StrictMockInsecureCredentialsManagerObserver observer;
  provider().AddObserver(&observer);

  // Adding a compromised credential should notify observers.
  password_form.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});

  EXPECT_CALL(observer, OnInsecureCredentialsChanged);
  EXPECT_CALL(observer, OnWeakCredentialsChanged);
  store().UpdateLogin(password_form);
  RunUntilIdle();

  // Remove should notify, and observers should be passed an empty list.
  password_form.password_issues.clear();
  EXPECT_CALL(observer, OnInsecureCredentialsChanged);
  EXPECT_CALL(observer, OnWeakCredentialsChanged);
  store().UpdateLogin(password_form);

  RunUntilIdle();
  EXPECT_THAT(provider().GetInsecureCredentialEntries(), IsEmpty());

  // After an observer is removed it should no longer receive notifications.
  provider().RemoveObserver(&observer);
  EXPECT_CALL(observer, OnInsecureCredentialsChanged).Times(0);
  password_form.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});
  store().UpdateLogin(password_form);
  RunUntilIdle();
  EXPECT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(password_form)));
}

// Tests whether adding and removing an observer works as expected.
TEST_F(InsecureCredentialsManagerTest,
       NotifyObserversAboutSavedPasswordsChanges) {
  StrictMockInsecureCredentialsManagerObserver observer;
  provider().AddObserver(&observer);

  PasswordForm saved_password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);

  // Adding a saved password should notify observers.
  EXPECT_CALL(observer, OnInsecureCredentialsChanged);
  EXPECT_CALL(observer, OnWeakCredentialsChanged);
  store().AddLogin(saved_password);
  RunUntilIdle();

  // Updating a saved password should notify observers.
  saved_password.password_value = kPassword216;
  EXPECT_CALL(observer, OnInsecureCredentialsChanged);
  EXPECT_CALL(observer, OnWeakCredentialsChanged);
  store().UpdateLogin(saved_password);
  RunUntilIdle();

  // Removing a saved password should notify observers.
  EXPECT_CALL(observer, OnInsecureCredentialsChanged);
  EXPECT_CALL(observer, OnWeakCredentialsChanged);
  store().RemoveLogin(saved_password);
  RunUntilIdle();

  // After an observer is removed it should no longer receive notifications.
  provider().RemoveObserver(&observer);
  EXPECT_CALL(observer, OnInsecureCredentialsChanged).Times(0);
  EXPECT_CALL(observer, OnWeakCredentialsChanged).Times(0);
  store().AddLogin(saved_password);
  RunUntilIdle();
}

// Tests that the provider is able to join a single password with a compromised
// credential.
TEST_F(InsecureCredentialsManagerTest, JoinSingleCredentials) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  password.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});

  store().AddLogin(password);
  RunUntilIdle();

  EXPECT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(password)));
}

// Tests that the provider is able to join a password with a credential that was
// compromised in multiple ways.
TEST_F(InsecureCredentialsManagerTest, JoinPhishedAndLeaked) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);

  password.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});
  password.password_issues.insert(
      {InsecureType::kPhished, InsecurityMetadata()});

  store().AddLogin(password);
  RunUntilIdle();

  EXPECT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(password)));
}

// Tests that the provider reacts whenever the saved passwords or the
// compromised credentials change.
TEST_F(InsecureCredentialsManagerTest, ReactToChangesInBothTables) {
  PasswordForm password1 =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  PasswordForm password2 =
      MakeSavedPassword(kExampleCom, kUsername2, kPassword216);

  store().AddLogin(password1);
  RunUntilIdle();
  EXPECT_THAT(provider().GetInsecureCredentialEntries(), IsEmpty());

  password1.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});
  store().UpdateLogin(password1);
  RunUntilIdle();
  EXPECT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(password1)));

  store().AddLogin(password2);
  RunUntilIdle();
  EXPECT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(password1)));

  password2.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});
  store().UpdateLogin(password2);
  RunUntilIdle();
  EXPECT_THAT(provider().GetInsecureCredentialEntries(),
              testing::UnorderedElementsAre(CredentialUIEntry(password1),
                                            CredentialUIEntry(password2)));

  store().RemoveLogin(password1);
  RunUntilIdle();
  EXPECT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(password2)));

  store().RemoveLogin(password2);
  RunUntilIdle();
  EXPECT_THAT(provider().GetInsecureCredentialEntries(), IsEmpty());
}

// Tests that the provider is able to join multiple passwords with compromised
// credentials.
TEST_F(InsecureCredentialsManagerTest, JoinMultipleCredentials) {
  PasswordForm password1 =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  password1.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});

  PasswordForm password2 =
      MakeSavedPassword(kExampleCom, kUsername2, kPassword216);
  password2.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});

  store().AddLogin(password1);
  store().AddLogin(password2);
  RunUntilIdle();

  EXPECT_THAT(provider().GetInsecureCredentialEntries(),
              testing::UnorderedElementsAre(CredentialUIEntry(password1),
                                            CredentialUIEntry(password2)));
}

// Tests that joining a compromised credential with multiple saved passwords for
// the same signon_realm and username combination results in a single entry
// when the passwords are the same.
TEST_F(InsecureCredentialsManagerTest, JoinWithMultipleRepeatedPasswords) {
  PasswordForm password1 =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  password1.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});

  PasswordForm password2 =
      MakeSavedPassword(kExampleCom, kUsername2, kPassword216);

  store().AddLogin(password1);
  store().AddLogin(password2);
  RunUntilIdle();

  EXPECT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(password1)));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(InsecureCredentialsManagerTest, StartWeakCheckNotifiesOnCompletion) {
  base::MockOnceClosure closure;
  provider().StartWeakCheck(closure.Get());
  EXPECT_CALL(closure, Run);
  RunUntilIdle();
}

TEST_F(InsecureCredentialsManagerTest, StartWeakCheckOnEmptyPasswordsList) {
  EXPECT_THAT(
      histogram_tester().GetTotalCountsForPrefix("PasswordManager.WeakCheck"),
      IsEmpty());

  RunUntilIdle();
  provider().StartWeakCheck();
  AdvanceClock(base::Milliseconds(kDelay));
  RunUntilIdle();

  EXPECT_THAT(provider().GetWeakCredentialEntries(), IsEmpty());

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.CheckedPasswords", 0, 1);
  histogram_tester().ExpectUniqueSample("PasswordManager.WeakCheck.Time",
                                        kDelay, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.WeakPasswords", 0, 1);
}

TEST_F(InsecureCredentialsManagerTest, WeakCredentialsNotFound) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kStrongPassword116),
      MakeSavedPassword(kExampleCom, kUsername2, kStrongPassword2)};

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  EXPECT_THAT(
      histogram_tester().GetTotalCountsForPrefix("PasswordManager.WeakCheck"),
      IsEmpty());

  RunUntilIdle();
  provider().StartWeakCheck();
  AdvanceClock(base::Milliseconds(2 * kDelay));
  RunUntilIdle();

  EXPECT_THAT(provider().GetWeakCredentialEntries(), IsEmpty());

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.CheckedPasswords", 2, 1);
  histogram_tester().ExpectUniqueSample("PasswordManager.WeakCheck.Time",
                                        2 * kDelay, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.WeakPasswords", 0, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.PasswordScore", 4, 2);
}

TEST_F(InsecureCredentialsManagerTest, DetectedWeakCredential) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1),
      MakeSavedPassword(kExampleCom, kUsername2, kStrongPassword116)};

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  EXPECT_THAT(
      histogram_tester().GetTotalCountsForPrefix("PasswordManager.WeakCheck"),
      IsEmpty());

  RunUntilIdle();
  provider().StartWeakCheck();
  AdvanceClock(base::Milliseconds(kDelay));
  RunUntilIdle();

  EXPECT_THAT(provider().GetWeakCredentialEntries(),
              ElementsAre(CredentialUIEntry(passwords[0])));

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.CheckedPasswords", 2, 1);
  histogram_tester().ExpectUniqueSample("PasswordManager.WeakCheck.Time",
                                        kDelay, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.WeakPasswords", 1, 1);
  histogram_tester().ExpectTotalCount("PasswordManager.WeakCheck.PasswordScore",
                                      2);
}

// Tests that credentials with the same signon_realm and username, but different
// passwords will be both returned by GetWeakCredentialEntries().
TEST_F(InsecureCredentialsManagerTest,
       FindBothWeakCredentialsWithDifferentPasswords) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1, u"element_1"),
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword216,
                        u"element_2")};

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);

  RunUntilIdle();
  provider().StartWeakCheck();
  AdvanceClock(base::Milliseconds(kDelay));
  RunUntilIdle();

  EXPECT_THAT(provider().GetWeakCredentialEntries(),
              ElementsAre(CredentialUIEntry(passwords[0]),
                          CredentialUIEntry(passwords[1])));

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.CheckedPasswords", 2, 1);
  histogram_tester().ExpectUniqueSample("PasswordManager.WeakCheck.Time",
                                        kDelay, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.WeakPasswords", 2, 1);
  histogram_tester().ExpectTotalCount("PasswordManager.WeakCheck.PasswordScore",
                                      2);
}

// Tests that credentials with the same signon_realm, username and passwords
// will be joind and GetWeakCredentialEntries() will return one credential.
TEST_F(InsecureCredentialsManagerTest,
       JoinWeakCredentialsWithTheSamePasswords) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1, u"element_1"),
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1, u"element_2")};

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);

  RunUntilIdle();
  provider().StartWeakCheck();
  AdvanceClock(base::Milliseconds(kDelay));
  RunUntilIdle();

  EXPECT_THAT(provider().GetWeakCredentialEntries(),
              ElementsAre(CredentialUIEntry(passwords[0])));

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.CheckedPasswords", 1, 1);
  histogram_tester().ExpectUniqueSample("PasswordManager.WeakCheck.Time",
                                        kDelay, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.WeakPasswords", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.PasswordScore", 0, 1);
}

TEST_F(InsecureCredentialsManagerTest, BothWeakAndCompromisedCredentialsExist) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1),
      MakeSavedPassword(kExampleCom, kUsername2, kStrongPassword116)};
  passwords.at(0).password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});
  passwords.at(1).password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);

  RunUntilIdle();
  provider().StartWeakCheck();
  AdvanceClock(base::Milliseconds(kDelay));
  RunUntilIdle();

  EXPECT_THAT(provider().GetWeakCredentialEntries(),
              ElementsAre(CredentialUIEntry(passwords[0])));
  EXPECT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(passwords[0]),
                          CredentialUIEntry(passwords[1])));

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.CheckedPasswords", 2, 1);
  histogram_tester().ExpectUniqueSample("PasswordManager.WeakCheck.Time",
                                        kDelay, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.WeakPasswords", 1, 1);
  histogram_tester().ExpectTotalCount("PasswordManager.WeakCheck.PasswordScore",
                                      2);
}

// Checks that for a credential that is both weak and compromised,
// GetWeakCredentialEntries and GetInsecureCredentials will return this
// credential in one instance.
TEST_F(InsecureCredentialsManagerTest, SingleCredentialIsWeakAndCompromised) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1)};

  passwords.at(0).password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});

  store().AddLogin(passwords[0]);

  RunUntilIdle();
  provider().StartWeakCheck();
  AdvanceClock(base::Milliseconds(kDelay));
  RunUntilIdle();

  EXPECT_THAT(provider().GetWeakCredentialEntries(),
              ElementsAre(CredentialUIEntry(passwords[0])));
  EXPECT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(passwords[0])));

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.CheckedPasswords", 1, 1);
  histogram_tester().ExpectUniqueSample("PasswordManager.WeakCheck.Time",
                                        kDelay, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.WeakPasswords", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.PasswordScore", 0, 1);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Test verifies that saving LeakCheckCredential via provider adds expected
// compromised credential.
TEST_F(InsecureCredentialsManagerTest, SaveCompromisedPassword) {
  PasswordForm password_form =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  LeakCheckCredential credential = MakeLeakCredential(kUsername1, kPassword1);

  store().AddLogin(password_form);
  RunUntilIdle();

  EXPECT_THAT(provider().GetInsecureCredentialEntries(), IsEmpty());

  password_form.password_issues[InsecureType::kLeaked].create_time =
      base::Time::Now();
  provider().SaveInsecureCredential(credential);
  RunUntilIdle();

  EXPECT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(password_form)));
}

// Test verifies that saving LeakCheckCredential doesn't occur for already
// leaked passwords.
TEST_F(InsecureCredentialsManagerTest, SaveCompromisedPasswordForExistingLeak) {
  PasswordForm password_form =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  LeakCheckCredential credential = MakeLeakCredential(kUsername1, kPassword1);

  InsecurityMetadata insecurity_metadata(base::Time::Now() - base::Days(3),
                                         IsMuted(true));
  password_form.password_issues.insert(
      {InsecureType::kLeaked, insecurity_metadata});

  store().AddLogin(password_form);
  RunUntilIdle();

  provider().SaveInsecureCredential(credential);
  RunUntilIdle();

  EXPECT_EQ(insecurity_metadata,
            store()
                .stored_passwords()
                .at(kExampleCom)
                .back()
                .password_issues.at(InsecureType::kLeaked));
}

TEST_F(InsecureCredentialsManagerTest, MuteCompromisedCredential) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  password.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});

  store().AddLogin(password);
  RunUntilIdle();

  ASSERT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(password)));

  EXPECT_TRUE(provider().MuteCredential(CredentialUIEntry(password)));
  RunUntilIdle();

  EXPECT_TRUE(provider()
                  .GetInsecureCredentialEntries()[0]
                  .password_issues.at(InsecureType::kLeaked)
                  .is_muted.value());
  EXPECT_TRUE(store()
                  .stored_passwords()
                  .at(kExampleCom)
                  .back()
                  .password_issues.at(InsecureType::kLeaked)
                  .is_muted.value());
}

TEST_F(InsecureCredentialsManagerTest, UnmuteCompromisedMutedCredential) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  password.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata(base::Time(), IsMuted(true))});

  store().AddLogin(password);
  RunUntilIdle();

  ASSERT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(password)));

  EXPECT_TRUE(provider().UnmuteCredential(CredentialUIEntry(password)));
  RunUntilIdle();
  EXPECT_FALSE(provider()
                   .GetInsecureCredentialEntries()[0]
                   .password_issues.at(InsecureType::kLeaked)
                   .is_muted.value());
  EXPECT_FALSE(store()
                   .stored_passwords()
                   .at(kExampleCom)
                   .back()
                   .password_issues.at(InsecureType::kLeaked)
                   .is_muted.value());
}

TEST_F(InsecureCredentialsManagerTest, UnmuteCompromisedNotMutedCredential) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  password.password_issues.insert(
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time(), IsMuted(false))});

  store().AddLogin(password);
  RunUntilIdle();

  ASSERT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(password)));

  EXPECT_FALSE(provider().UnmuteCredential(CredentialUIEntry(password)));
  RunUntilIdle();
  EXPECT_FALSE(provider()
                   .GetInsecureCredentialEntries()[0]
                   .password_issues.at(InsecureType::kLeaked)
                   .is_muted.value());
  EXPECT_FALSE(store()
                   .stored_passwords()
                   .at(kExampleCom)
                   .back()
                   .password_issues.at(InsecureType::kLeaked)
                   .is_muted.value());
}

TEST_F(InsecureCredentialsManagerTest,
       UnmuteCompromisedMutedCredentialWithMultipleInsecurityTypes) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  password.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata(base::Time(), IsMuted(true))});
  password.password_issues.insert(
      {InsecureType::kPhished,
       InsecurityMetadata(base::Time(), IsMuted(true))});

  store().AddLogin(password);
  RunUntilIdle();

  ASSERT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(password)));

  EXPECT_TRUE(provider().UnmuteCredential(CredentialUIEntry(password)));
  RunUntilIdle();
  EXPECT_FALSE(provider()
                   .GetInsecureCredentialEntries()[0]
                   .password_issues.at(InsecureType::kLeaked)
                   .is_muted.value());
  EXPECT_FALSE(provider()
                   .GetInsecureCredentialEntries()[0]
                   .password_issues.at(InsecureType::kPhished)
                   .is_muted.value());
  EXPECT_FALSE(store()
                   .stored_passwords()
                   .at(kExampleCom)
                   .back()
                   .password_issues.at(InsecureType::kLeaked)
                   .is_muted.value());
  EXPECT_FALSE(store()
                   .stored_passwords()
                   .at(kExampleCom)
                   .back()
                   .password_issues.at(InsecureType::kPhished)
                   .is_muted.value());
}

TEST_F(InsecureCredentialsManagerTest,
       FilterThenUnmuteMultipleInsecurityTypes) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  password.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata(base::Time(), IsMuted(true))});
  password.password_issues.insert(
      {InsecureType::kPhished,
       InsecurityMetadata(base::Time(), IsMuted(true))});
  password.password_issues.insert(
      {InsecureType::kReused, InsecurityMetadata(base::Time(), IsMuted(true))});
  password.password_issues.insert(
      {InsecureType::kWeak, InsecurityMetadata(base::Time(), IsMuted(true))});

  store().AddLogin(password);
  RunUntilIdle();

  ASSERT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(password)));

  EXPECT_TRUE(provider().UnmuteCredential(CredentialUIEntry(password)));
  RunUntilIdle();

  PasswordForm expected = password;
  expected.password_issues[InsecureType::kLeaked].is_muted = IsMuted(false);
  expected.password_issues[InsecureType::kPhished].is_muted = IsMuted(false);
  EXPECT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(expected)));
  EXPECT_FALSE(store()
                   .stored_passwords()
                   .at(kExampleCom)
                   .back()
                   .password_issues.at(InsecureType::kLeaked)
                   .is_muted.value());
  EXPECT_FALSE(store()
                   .stored_passwords()
                   .at(kExampleCom)
                   .back()
                   .password_issues.at(InsecureType::kPhished)
                   .is_muted.value());
  EXPECT_TRUE(store()
                  .stored_passwords()
                  .at(kExampleCom)
                  .back()
                  .password_issues.at(InsecureType::kReused)
                  .is_muted.value());
  EXPECT_TRUE(store()
                  .stored_passwords()
                  .at(kExampleCom)
                  .back()
                  .password_issues.at(InsecureType::kWeak)
                  .is_muted.value());
}

TEST_F(InsecureCredentialsManagerTest, MuteCompromisedCredentialOnMutedIsNoOp) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  password.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata(base::Time(), IsMuted(true))});

  store().AddLogin(password);
  RunUntilIdle();

  ASSERT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(password)));

  EXPECT_FALSE(provider().MuteCredential(CredentialUIEntry(password)));
  RunUntilIdle();
  EXPECT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(password)));
  EXPECT_TRUE(store()
                  .stored_passwords()
                  .at(kExampleCom)
                  .back()
                  .password_issues.at(InsecureType::kLeaked)
                  .is_muted.value());
}

TEST_F(InsecureCredentialsManagerTest,
       MuteCompromisedCredentialLeakedMutesMultipleInsecurityTypes) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  password.password_issues.insert(
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time(), IsMuted(false))});
  password.password_issues.insert(
      {InsecureType::kPhished,
       InsecurityMetadata(base::Time(), IsMuted(false))});

  store().AddLogin(password);
  RunUntilIdle();

  ASSERT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(password)));

  EXPECT_TRUE(provider().MuteCredential(CredentialUIEntry(password)));
  RunUntilIdle();
  PasswordForm expected = password;
  expected.password_issues[InsecureType::kLeaked].is_muted = IsMuted(true);
  expected.password_issues[InsecureType::kPhished].is_muted = IsMuted(true);
  EXPECT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(expected)));
  EXPECT_TRUE(store()
                  .stored_passwords()
                  .at(kExampleCom)
                  .back()
                  .password_issues.at(InsecureType::kLeaked)
                  .is_muted.value());
  EXPECT_TRUE(store()
                  .stored_passwords()
                  .at(kExampleCom)
                  .back()
                  .password_issues.at(InsecureType::kPhished)
                  .is_muted.value());
}

TEST_F(InsecureCredentialsManagerTest, FilterThenMuteMultipleInsecurityTypes) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  password.password_issues.insert(
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time(), IsMuted(false))});
  password.password_issues.insert(
      {InsecureType::kPhished,
       InsecurityMetadata(base::Time(), IsMuted(false))});
  password.password_issues.insert(
      {InsecureType::kReused,
       InsecurityMetadata(base::Time(), IsMuted(false))});
  password.password_issues.insert(
      {InsecureType::kWeak, InsecurityMetadata(base::Time(), IsMuted(false))});

  store().AddLogin(password);
  RunUntilIdle();

  ASSERT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(password)));

  EXPECT_TRUE(provider().MuteCredential(CredentialUIEntry(password)));

  RunUntilIdle();

  PasswordForm expected = password;
  expected.password_issues[InsecureType::kLeaked].is_muted = IsMuted(true);
  expected.password_issues[InsecureType::kPhished].is_muted = IsMuted(true);
  EXPECT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(expected)));
  EXPECT_TRUE(store()
                  .stored_passwords()
                  .at(kExampleCom)
                  .back()
                  .password_issues.at(InsecureType::kLeaked)
                  .is_muted.value());
  EXPECT_TRUE(store()
                  .stored_passwords()
                  .at(kExampleCom)
                  .back()
                  .password_issues.at(InsecureType::kPhished)
                  .is_muted.value());
  EXPECT_FALSE(store()
                   .stored_passwords()
                   .at(kExampleCom)
                   .back()
                   .password_issues.at(InsecureType::kReused)
                   .is_muted.value());
  EXPECT_FALSE(store()
                   .stored_passwords()
                   .at(kExampleCom)
                   .back()
                   .password_issues.at(InsecureType::kWeak)
                   .is_muted.value());
}

TEST_F(InsecureCredentialsManagerTest, MuteWeakPasswordNoOp) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);

  password.password_issues.insert(
      {InsecureType::kWeak, InsecurityMetadata(base::Time(), IsMuted(false))});

  store().AddLogin(password);
  RunUntilIdle();

  ASSERT_THAT(provider().GetInsecureCredentialEntries(), IsEmpty());

  EXPECT_FALSE(provider().MuteCredential(CredentialUIEntry(password)));

  RunUntilIdle();

  EXPECT_THAT(provider().GetInsecureCredentialEntries(), IsEmpty());
  EXPECT_FALSE(store()
                   .stored_passwords()
                   .at(kExampleCom)
                   .back()
                   .password_issues.at(InsecureType::kWeak)
                   .is_muted.value());
}

TEST_F(InsecureCredentialsManagerTest, UnMuteWeakPasswordNoOp) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);

  password.password_issues.insert(
      {InsecureType::kWeak, InsecurityMetadata(base::Time(), IsMuted(true))});

  store().AddLogin(password);
  RunUntilIdle();

  ASSERT_THAT(provider().GetInsecureCredentialEntries(), IsEmpty());

  EXPECT_FALSE(provider().UnmuteCredential(CredentialUIEntry(password)));

  RunUntilIdle();

  EXPECT_THAT(provider().GetInsecureCredentialEntries(), IsEmpty());

  EXPECT_TRUE(store()
                  .stored_passwords()
                  .at(kExampleCom)
                  .back()
                  .password_issues.at(InsecureType::kWeak)
                  .is_muted.value());
}

TEST_F(InsecureCredentialsManagerTest, MuteReusedPasswordNoOp) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);

  password.password_issues.insert(
      {InsecureType::kReused,
       InsecurityMetadata(base::Time(), IsMuted(false))});

  store().AddLogin(password);
  RunUntilIdle();

  ASSERT_THAT(provider().GetInsecureCredentialEntries(), IsEmpty());

  EXPECT_FALSE(provider().MuteCredential(CredentialUIEntry(password)));

  RunUntilIdle();

  EXPECT_THAT(provider().GetInsecureCredentialEntries(), IsEmpty());
  EXPECT_FALSE(store()
                   .stored_passwords()
                   .at(kExampleCom)
                   .back()
                   .password_issues.at(InsecureType::kReused)
                   .is_muted.value());
}

TEST_F(InsecureCredentialsManagerTest, UnMuteReusedPasswordNoOp) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);

  password.password_issues.insert(
      {InsecureType::kReused, InsecurityMetadata(base::Time(), IsMuted(true))});

  store().AddLogin(password);
  RunUntilIdle();

  ASSERT_THAT(provider().GetInsecureCredentialEntries(), IsEmpty());

  EXPECT_FALSE(provider().UnmuteCredential(CredentialUIEntry(password)));

  RunUntilIdle();

  EXPECT_THAT(provider().GetInsecureCredentialEntries(), IsEmpty());
  EXPECT_TRUE(store()
                  .stored_passwords()
                  .at(kExampleCom)
                  .back()
                  .password_issues.at(InsecureType::kReused)
                  .is_muted.value());
}

TEST_F(InsecureCredentialsManagerTest, GetInsecureCredentialEntriesWithFlag) {
  PasswordForm password_form =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);

  store().AddLogin(password_form);
  RunUntilIdle();

  EXPECT_THAT(provider().GetInsecureCredentialEntries(), IsEmpty());

  // Turn on the field trial parameter to force password leaks everywhere.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      password_manager::features::kPasswordChangeInSettings,
      {{password_manager::features::
            kPasswordChangeInSettingsWithForcedWarningForEverySite,
        "true"}});

  // The credential is now returned as insecure.
  EXPECT_THAT(provider().GetInsecureCredentialEntries(), SizeIs(1u));
}

// Test verifies that editing Compromised Credential makes it secure.
TEST_F(InsecureCredentialsManagerTest, UpdateCompromisedPassword) {
  PasswordForm password_form =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  password_form.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});
  store().AddLogin(password_form);
  RunUntilIdle();

  EXPECT_THAT(provider().GetInsecureCredentialEntries(), SizeIs(1u));

  CredentialUIEntry original_credential(password_form),
      updated_credential = original_credential;
  updated_credential.password = kPassword216;
  presenter().EditSavedCredentials(original_credential, updated_credential);
  RunUntilIdle();

  EXPECT_TRUE(provider().GetInsecureCredentialEntries().empty());
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Test verifies that editing a weak credential to another weak credential
// continues to be treated weak.
TEST_F(InsecureCredentialsManagerTest, UpdatedWeakPasswordBecomesStrong) {
  PasswordForm password_form =
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1);

  store().AddLogin(password_form);
  RunUntilIdle();

  provider().StartWeakCheck();
  RunUntilIdle();
  EXPECT_THAT(provider().GetWeakCredentialEntries(),
              ElementsAre(CredentialUIEntry(password_form)));

  CredentialUIEntry original_credential(password_form),
      updated_credential = original_credential;
  updated_credential.password = kStrongPassword116;
  presenter().EditSavedCredentials(original_credential, updated_credential);
  RunUntilIdle();

  EXPECT_THAT(provider().GetWeakCredentialEntries(), IsEmpty());
}

// Test verifies that editing a weak credential to another weak credential
// continues to be treated weak.
TEST_F(InsecureCredentialsManagerTest, UpdatedWeakPasswordRemainsWeak) {
  PasswordForm password_form =
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1);

  store().AddLogin(password_form);
  RunUntilIdle();

  provider().StartWeakCheck();
  RunUntilIdle();
  EXPECT_THAT(provider().GetWeakCredentialEntries(),
              ElementsAre(CredentialUIEntry(password_form)));

  CredentialUIEntry original_credential(password_form),
      updated_credential = original_credential;
  updated_credential.password = kWeakPassword216;
  presenter().EditSavedCredentials(original_credential, updated_credential);
  RunUntilIdle();

  EXPECT_THAT(provider().GetWeakCredentialEntries(),
              ElementsAre(updated_credential));
}

// Verifues that GetWeakCredentialEntries() returns sorted weak credentials by
// using CreateSortKey.
TEST_F(InsecureCredentialsManagerTest, GetWeakCredentialsReturnsSortedData) {
  const std::vector<PasswordForm> password_forms = {
      MakeSavedPassword("http://example-a.com", u"user_a1", u"pwd"),
      MakeSavedPassword("http://example-a.com", u"user_a2", u"pwd"),
      MakeSavedPassword("http://example-b.com", u"user_a", u"pwd"),
      MakeSavedPassword("http://example-c.com", u"user_a", u"pwd")};
  store().AddLogin(password_forms[0]);
  store().AddLogin(password_forms[1]);
  store().AddLogin(password_forms[2]);
  store().AddLogin(password_forms[3]);
  RunUntilIdle();

  provider().StartWeakCheck();
  RunUntilIdle();

  EXPECT_THAT(provider().GetWeakCredentialEntries(),
              ElementsAre(CredentialUIEntry(password_forms[0]),
                          CredentialUIEntry(password_forms[1]),
                          CredentialUIEntry(password_forms[2]),
                          CredentialUIEntry(password_forms[3])));
}

// Verifues that GetWeakCredentialEntries() returns sorted weak credentials by
// using CreateSortKey.
TEST_F(InsecureCredentialsManagerTest, GetWeakCredentialEntries) {
  const std::vector<PasswordForm> password_forms = {
      MakeSavedPassword("http://example-a.com", u"user_a1", u"pwd"),
      MakeSavedPassword("http://example-a.com", u"user_a2", u"pwd")};
  store().AddLogin(password_forms[0]);
  store().AddLogin(password_forms[1]);
  RunUntilIdle();

  provider().StartWeakCheck();
  RunUntilIdle();

  EXPECT_THAT(provider().GetWeakCredentialEntries(),
              ElementsAre(CredentialUIEntry(password_forms[0]),
                          CredentialUIEntry(password_forms[1])));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

TEST_F(InsecureCredentialsManagerTest, GetInsecureCredentialsFiltersWeak) {
  PasswordForm password1 =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  PasswordForm password2 =
      MakeSavedPassword(kExampleCom, kUsername2, kPassword216);

  password1.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});
  password2.password_issues.insert({InsecureType::kWeak, InsecurityMetadata()});

  store().AddLogin(password1);
  store().AddLogin(password2);

  RunUntilIdle();

  EXPECT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(password1)));
}

TEST_F(InsecureCredentialsManagerTest,
       GetInsecureCredentialsFiltersDuplicates) {
  PasswordForm password1 =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  PasswordForm password2 = MakeSavedPassword(kExampleCom, kUsername1,
                                             kPassword1, u"username_element");

  password1.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});
  password2.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});

  store().AddLogin(password1);
  store().AddLogin(password2);

  RunUntilIdle();

  EXPECT_THAT(provider().GetInsecureCredentialEntries(),
              ElementsAre(CredentialUIEntry(password1)));
}

namespace {
class InsecureCredentialsManagerWithTwoStoresTest : public ::testing::Test {
 protected:
  InsecureCredentialsManagerWithTwoStoresTest() {
    profile_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
    account_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
  }

  ~InsecureCredentialsManagerWithTwoStoresTest() override {
    account_store_->ShutdownOnUIThread();
    profile_store_->ShutdownOnUIThread();
    task_env_.RunUntilIdle();
  }

  TestPasswordStore& profile_store() { return *profile_store_; }
  TestPasswordStore& account_store() { return *account_store_; }
  InsecureCredentialsManager& provider() { return provider_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_env_;
  scoped_refptr<TestPasswordStore> profile_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  scoped_refptr<TestPasswordStore> account_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  SavedPasswordsPresenter presenter_{profile_store_, account_store_};
  InsecureCredentialsManager provider_{&presenter_, profile_store_,
                                       account_store_};
};
}  // namespace

// Test verifies that saving LeakCheckCredential via provider adds expected
// compromised credential to the correct store.
TEST_F(InsecureCredentialsManagerWithTwoStoresTest, SaveCompromisedPassword) {
  ASSERT_TRUE(profile_store().stored_passwords().empty());
  ASSERT_TRUE(account_store().stored_passwords().empty());
  // Add `kUsername1`,`kPassword1` to both stores.
  // And add `kUsername1`,`kPassword2` to the account store only.
  profile_store().AddLogin(
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1));

  account_store().AddLogin(
      MakeSavedPassword(kExampleOrg, kUsername1, kPassword1));
  account_store().AddLogin(
      MakeSavedPassword(kExampleCom, kUsername1, kPassword216));

  RunUntilIdle();

  // Mark `kUsername1`, `kPassword1` as compromised, a new entry should be
  // added to both stores.
  provider().SaveInsecureCredential(MakeLeakCredential(kUsername1, kPassword1));
  RunUntilIdle();

  EXPECT_EQ(2U, provider().GetInsecureCredentialEntries().size());
  EXPECT_EQ(1U, profile_store()
                    .stored_passwords()
                    .at(kExampleCom)
                    .back()
                    .password_issues.size());
  EXPECT_EQ(1U, account_store()
                    .stored_passwords()
                    .at(kExampleOrg)
                    .back()
                    .password_issues.size());
  EXPECT_EQ(0U, account_store()
                    .stored_passwords()
                    .at(kExampleCom)
                    .back()
                    .password_issues.size());

  // Now, mark `kUsername1`, `kPassword216` as compromised, a new entry should
  // be added only to the account store.
  provider().SaveInsecureCredential(
      MakeLeakCredential(kUsername1, kPassword216));
  RunUntilIdle();

  EXPECT_EQ(3U, provider().GetInsecureCredentialEntries().size());
  EXPECT_EQ(1U, profile_store()
                    .stored_passwords()
                    .at(kExampleCom)
                    .back()
                    .password_issues.size());
  EXPECT_EQ(1U, account_store()
                    .stored_passwords()
                    .at(kExampleCom)
                    .back()
                    .password_issues.size());
  EXPECT_EQ(1U, account_store()
                    .stored_passwords()
                    .at(kExampleOrg)
                    .back()
                    .password_issues.size());
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(InsecureCredentialsManagerWithTwoStoresTest,
       GetInsecureCredentialsWeak) {
  profile_store().AddLogin(
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1));
  account_store().AddLogin(
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1));
  RunUntilIdle();
  provider().StartWeakCheck();
  RunUntilIdle();

  PasswordForm expected_form =
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1);
  expected_form.in_store =
      PasswordForm::Store::kProfileStore | PasswordForm::Store::kAccountStore;

  EXPECT_THAT(provider().GetWeakCredentialEntries(),
              ElementsAre(CredentialUIEntry(expected_form)));
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace password_manager
