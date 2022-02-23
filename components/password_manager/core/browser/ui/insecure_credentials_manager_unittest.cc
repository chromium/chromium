// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

constexpr char kExampleCom[] = "https://example.com";
constexpr char kExampleOrg[] = "https://example.org";

constexpr char16_t kUsername1[] = u"alice";
constexpr char16_t kUsername2[] = u"bob";

constexpr char16_t kPassword1[] = u"f00b4r";
constexpr char kPassword2[] = "s3cr3t";
constexpr char16_t kPassword216[] = u"s3cr3t";
constexpr char16_t kPassword3[] = u"484her";

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
constexpr char16_t kWeakPassword1[] = u"123456";
constexpr char kWeakPassword2[] = "abcdabcdabcdabcdabcdabcdabcdabcdabcdabcda";
constexpr char16_t kWeakPassword216[] =
    u"abcdabcdabcdabcdabcdabcdabcdabcdabcdabcda";
constexpr char kStrongPassword1[] = "fnlsr4@cm^mdls@fkspnsg3d";
constexpr char16_t kStrongPassword116[] = u"fnlsr4@cm^mdls@fkspnsg3d";
constexpr char16_t kStrongPassword2[] =
    u"pmsFlsnoab4nsl#losb@skpfnsbkjb^klsnbs!cns";
// Delay in milliseconds.
constexpr int kDelay = 2;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

struct MockInsecureCredentialsManagerObserver
    : InsecureCredentialsManager::Observer {
  MOCK_METHOD(void,
              OnInsecureCredentialsChanged,
              (InsecureCredentialsManager::CredentialsView),
              (override));
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
  return form;
}

LeakCheckCredential MakeLeakCredential(base::StringPiece16 username,
                                       base::StringPiece16 password) {
  return LeakCheckCredential(std::u16string(username),
                             std::u16string(password));
}

CredentialWithPassword MakeCompromisedCredential(
    const PasswordForm& form,
    const InsecureCredentialTypeFlags type =
        InsecureCredentialTypeFlags::kCredentialLeaked,
    const bool is_muted = false) {
  CredentialWithPassword credential_with_password((CredentialView(form)));
  credential_with_password.insecure_type = type;
  credential_with_password.is_muted = IsMuted(is_muted);
  return credential_with_password;
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
CredentialWithPassword MakeWeakCredential(const PasswordForm& form) {
  CredentialWithPassword weak_credential{CredentialView(form)};
  weak_credential.insecure_type = InsecureCredentialTypeFlags::kWeakCredential;
  return weak_credential;
}

CredentialWithPassword MakeWeakAndCompromisedCredential(
    const PasswordForm& form) {
  CredentialWithPassword credential_with_password =
      MakeCompromisedCredential(form);
  credential_with_password.insecure_type |=
      InsecureCredentialTypeFlags::kWeakCredential;
  return credential_with_password;
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

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

bool operator==(const CredentialWithPassword& lhs,
                const CredentialWithPassword& rhs) {
  return lhs.signon_realm == rhs.signon_realm && lhs.username == rhs.username &&
         lhs.create_time == rhs.create_time && lhs.is_muted == rhs.is_muted &&
         lhs.insecure_type == rhs.insecure_type && lhs.password == rhs.password;
}

std::ostream& operator<<(std::ostream& out,
                         const CredentialWithPassword& credential) {
  return out << "{ signon_realm: " << credential.signon_realm
             << ", username: " << credential.username
             << ", create_time: " << credential.create_time
             << ", is_muted: " << credential.is_muted << ", insecure_type: "
             << static_cast<int>(credential.insecure_type)
             << ", password: " << credential.password << " }";
}

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
  EXPECT_CALL(observer, OnInsecureCredentialsChanged(IsEmpty()));
  EXPECT_CALL(observer, OnWeakCredentialsChanged);
  store().UpdateLogin(password_form);

  RunUntilIdle();
  EXPECT_THAT(provider().GetInsecureCredentials(), IsEmpty());

  // After an observer is removed it should no longer receive notifications.
  provider().RemoveObserver(&observer);
  EXPECT_CALL(observer, OnInsecureCredentialsChanged).Times(0);
  password_form.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});
  store().UpdateLogin(password_form);
  RunUntilIdle();
  EXPECT_THAT(provider().GetInsecureCredentials(),
              ElementsAre(MakeCompromisedCredential(password_form)));
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

  EXPECT_THAT(provider().GetInsecureCredentials(),
              ElementsAre(MakeCompromisedCredential(password)));
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

  CredentialWithPassword expected = MakeCompromisedCredential(password);
  expected.insecure_type = (InsecureCredentialTypeFlags::kCredentialLeaked |
                            InsecureCredentialTypeFlags::kCredentialPhished);

  EXPECT_THAT(provider().GetInsecureCredentials(), ElementsAre(expected));
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
  EXPECT_THAT(provider().GetInsecureCredentials(), IsEmpty());

  password1.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});
  store().UpdateLogin(password1);
  RunUntilIdle();
  EXPECT_THAT(provider().GetInsecureCredentials(),
              ElementsAre(MakeCompromisedCredential(password1)));

  store().AddLogin(password2);
  RunUntilIdle();
  EXPECT_THAT(provider().GetInsecureCredentials(),
              ElementsAre(MakeCompromisedCredential(password1)));

  password2.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});
  store().UpdateLogin(password2);
  RunUntilIdle();
  EXPECT_THAT(
      provider().GetInsecureCredentials(),
      testing::UnorderedElementsAre(MakeCompromisedCredential(password1),
                                    MakeCompromisedCredential(password2)));

  store().RemoveLogin(password1);
  RunUntilIdle();
  EXPECT_THAT(provider().GetInsecureCredentials(),
              ElementsAre(MakeCompromisedCredential(password2)));

  store().RemoveLogin(password2);
  RunUntilIdle();
  EXPECT_THAT(provider().GetInsecureCredentials(), IsEmpty());
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

  EXPECT_THAT(
      provider().GetInsecureCredentials(),
      testing::UnorderedElementsAre(MakeCompromisedCredential(password1),
                                    MakeCompromisedCredential(password2)));
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

  EXPECT_THAT(provider().GetInsecureCredentials(),
              ElementsAre(MakeCompromisedCredential(password1)));
}

// Tests that verifies mapping compromised credentials to passwords works
// correctly.
TEST_F(InsecureCredentialsManagerTest, MapCompromisedPasswordsToPasswords) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1, u"element_1"),
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1, u"element_2"),
      MakeSavedPassword(kExampleOrg, kUsername2, kPassword216)};

  passwords.at(0).password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});
  passwords.at(1).password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});
  passwords.at(2).password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});

  std::vector<CredentialWithPassword> credentials_with_password = {
      MakeCompromisedCredential(passwords[0]),
      MakeCompromisedCredential(passwords[1]),
      MakeCompromisedCredential(passwords[2])};

  for (const auto& form : passwords)
    store().AddLogin(form);

  RunUntilIdle();
  EXPECT_THAT(provider().GetSavedPasswordsFor(credentials_with_password[0]),
              ElementsAreArray(store().stored_passwords().at(kExampleCom)));

  EXPECT_THAT(provider().GetSavedPasswordsFor(credentials_with_password[1]),
              ElementsAreArray(store().stored_passwords().at(kExampleCom)));

  EXPECT_THAT(provider().GetSavedPasswordsFor(credentials_with_password[2]),
              ElementsAreArray(store().stored_passwords().at(kExampleOrg)));
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

  EXPECT_THAT(provider().GetWeakCredentials(), IsEmpty());

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

  EXPECT_THAT(provider().GetWeakCredentials(), IsEmpty());

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

  std::vector<CredentialWithPassword> weak_credentials =
      provider().GetWeakCredentials();

  ASSERT_EQ(weak_credentials.size(), 1u);
  EXPECT_EQ(weak_credentials[0].password, kWeakPassword1);
  EXPECT_TRUE(IsWeak(weak_credentials[0].insecure_type));

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
// passwords will be both returned by GetWeakCredentials().
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

  std::vector<CredentialWithPassword> weak_credentials =
      provider().GetWeakCredentials();

  ASSERT_EQ(weak_credentials.size(), 2u);
  EXPECT_TRUE(IsWeak(weak_credentials[0].insecure_type));
  EXPECT_TRUE(IsWeak(weak_credentials[1].insecure_type));

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
// will be joind and GetWeakCredentials() will return one credential.
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

  std::vector<CredentialWithPassword> weak_credentials =
      provider().GetWeakCredentials();

  ASSERT_EQ(weak_credentials.size(), 1u);
  EXPECT_EQ(weak_credentials[0].password, kWeakPassword1);
  EXPECT_TRUE(IsWeak(weak_credentials[0].insecure_type));

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

  std::vector<CredentialWithPassword> returned_weak_credentials =
      provider().GetWeakCredentials();
  std::vector<CredentialWithPassword> returned_compromised_credentials =
      provider().GetInsecureCredentials();

  ASSERT_EQ(returned_weak_credentials.size(), 1u);
  EXPECT_EQ(returned_weak_credentials[0].password, kWeakPassword1);
  EXPECT_TRUE(IsWeak(returned_weak_credentials[0].insecure_type));

  ASSERT_EQ(returned_compromised_credentials.size(), 2u);
  EXPECT_TRUE(IsInsecure(returned_compromised_credentials[0].insecure_type));
  EXPECT_TRUE(IsInsecure(returned_compromised_credentials[1].insecure_type));

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
// getWeakCredentials and GetInsecureCredentials will return this credential
// in one instance.
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

  std::vector<CredentialWithPassword> returned_weak_credentials =
      provider().GetWeakCredentials();
  std::vector<CredentialWithPassword> returned_compromised_credentials =
      provider().GetInsecureCredentials();

  // Since the credential is weak and compromised, the |insecure_type| should be
  // weak and compromised for elements from |returned_weak_credentials| and
  // |returned_compromised_credentials|.
  ASSERT_EQ(returned_weak_credentials.size(), 1u);
  EXPECT_EQ(returned_weak_credentials[0].password, kWeakPassword1);
  EXPECT_TRUE(IsWeak(returned_weak_credentials[0].insecure_type));
  EXPECT_TRUE(IsInsecure(returned_weak_credentials[0].insecure_type));

  ASSERT_EQ(returned_compromised_credentials.size(), 1u);
  EXPECT_EQ(returned_compromised_credentials[0].password, kWeakPassword1);
  EXPECT_TRUE(IsWeak(returned_compromised_credentials[0].insecure_type));
  EXPECT_TRUE(IsInsecure(returned_compromised_credentials[0].insecure_type));

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

  CredentialWithPassword expected = MakeCompromisedCredential(password_form);
  expected.create_time = base::Time::Now();

  provider().SaveInsecureCredential(credential);
  RunUntilIdle();

  EXPECT_THAT(provider().GetInsecureCredentials(), ElementsAre(expected));
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

  CredentialWithPassword expected = MakeCompromisedCredential(password);

  EXPECT_THAT(provider().GetInsecureCredentials(), ElementsAre(expected));
  EXPECT_FALSE(provider().GetInsecureCredentials()[0].is_muted);

  EXPECT_TRUE(provider().MuteCredential(expected));
  RunUntilIdle();
  EXPECT_TRUE(provider().GetInsecureCredentials()[0].is_muted);
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

  CredentialWithPassword expected = MakeCompromisedCredential(
      password, InsecureCredentialTypeFlags::kCredentialLeaked, true);

  EXPECT_THAT(provider().GetInsecureCredentials(), ElementsAre(expected));
  EXPECT_TRUE(provider().GetInsecureCredentials()[0].is_muted);

  EXPECT_TRUE(provider().UnmuteCredential(expected));
  RunUntilIdle();
  EXPECT_FALSE(provider().GetInsecureCredentials()[0].is_muted);
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

  CredentialWithPassword expected = MakeCompromisedCredential(
      password, InsecureCredentialTypeFlags::kCredentialLeaked, false);

  EXPECT_THAT(provider().GetInsecureCredentials(), ElementsAre(expected));
  EXPECT_FALSE(provider().GetInsecureCredentials()[0].is_muted);

  EXPECT_FALSE(provider().UnmuteCredential(expected));
  RunUntilIdle();
  EXPECT_FALSE(provider().GetInsecureCredentials()[0].is_muted);
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

  CredentialWithPassword expected = MakeCompromisedCredential(
      password,
      InsecureCredentialTypeFlags::kCredentialLeaked |
          InsecureCredentialTypeFlags::kCredentialPhished,
      true);

  EXPECT_THAT(provider().GetInsecureCredentials(), ElementsAre(expected));

  EXPECT_TRUE(provider().GetInsecureCredentials()[0].is_muted);

  EXPECT_TRUE(provider().UnmuteCredential(expected));
  RunUntilIdle();
  EXPECT_FALSE(provider().GetInsecureCredentials()[0].is_muted);
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

TEST_F(InsecureCredentialsManagerTest, MuteCompromisedCredentialOnMutedIsNoOp) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  password.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata(base::Time(), IsMuted(true))});

  store().AddLogin(password);
  RunUntilIdle();

  CredentialWithPassword expected = MakeCompromisedCredential(
      password, InsecureCredentialTypeFlags::kCredentialLeaked, true);

  EXPECT_THAT(provider().GetInsecureCredentials(), ElementsAre(expected));
  EXPECT_TRUE(provider().GetInsecureCredentials()[0].is_muted);

  EXPECT_FALSE(provider().MuteCredential(expected));
  RunUntilIdle();
  EXPECT_TRUE(provider().GetInsecureCredentials()[0].is_muted);
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

  CredentialWithPassword expected = MakeCompromisedCredential(
      password,
      InsecureCredentialTypeFlags::kCredentialLeaked |
          InsecureCredentialTypeFlags::kCredentialPhished,
      false);

  EXPECT_THAT(provider().GetInsecureCredentials(), ElementsAre(expected));

  EXPECT_FALSE(provider().GetInsecureCredentials()[0].is_muted);

  EXPECT_TRUE(provider().MuteCredential(expected));
  RunUntilIdle();
  EXPECT_TRUE(provider().GetInsecureCredentials()[0].is_muted);
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

// Test verifies that editing Compromised Credential via provider change the
// original password form.
TEST_F(InsecureCredentialsManagerTest, UpdateCompromisedPassword) {
  PasswordForm password_form =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  password_form.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});

  store().AddLogin(password_form);

  RunUntilIdle();
  CredentialWithPassword expected =
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      MakeWeakAndCompromisedCredential(password_form);
#else
      MakeCompromisedCredential(password_form);
#endif

  EXPECT_TRUE(provider().UpdateCredential(expected, kPassword2));
  RunUntilIdle();
  expected.password = kPassword216;

  EXPECT_TRUE(provider().GetInsecureCredentials().empty());
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Test verifies that editing weak credential via provider has affect on weak
// credentials and updates password in the store.
TEST_F(InsecureCredentialsManagerTest, UpdateWeakPassword) {
  PasswordForm password_form =
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1);

  store().AddLogin(password_form);
  RunUntilIdle();
  provider().StartWeakCheck();
  RunUntilIdle();

  EXPECT_EQ(provider().GetWeakCredentials().size(), 1u);
  EXPECT_TRUE(provider().UpdateCredential(CredentialView(password_form),
                                          kStrongPassword1));
  RunUntilIdle();

  EXPECT_THAT(provider().GetWeakCredentials(), IsEmpty());
  EXPECT_EQ(GetSavedPasswordForUsername(kExampleCom, kUsername1),
            kStrongPassword116);
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

  CredentialWithPassword expected = MakeWeakCredential(password_form);
  EXPECT_THAT(provider().GetWeakCredentials(), ElementsAre(expected));

  EXPECT_TRUE(provider().UpdateCredential(expected, kWeakPassword2));
  RunUntilIdle();

  expected.password = kWeakPassword216;
  EXPECT_THAT(provider().GetWeakCredentials(), ElementsAre(expected));
}

// Test verifies that editing credential that is weak and compromised via
// provider change the saved password.
TEST_F(InsecureCredentialsManagerTest, UpdateInsecurePassword) {
  PasswordForm password_form =
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1);
  password_form.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});

  store().AddLogin(password_form);
  RunUntilIdle();
  provider().StartWeakCheck();
  RunUntilIdle();

  CredentialWithPassword expected =
      MakeWeakAndCompromisedCredential(password_form);
  EXPECT_THAT(provider().GetWeakCredentials(), ElementsAre(expected));
  EXPECT_THAT(provider().GetInsecureCredentials(), ElementsAre(expected));

  EXPECT_TRUE(provider().UpdateCredential(expected, kStrongPassword1));
  RunUntilIdle();

  EXPECT_EQ(GetSavedPasswordForUsername(kExampleCom, kUsername1),
            kStrongPassword116);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

TEST_F(InsecureCredentialsManagerTest, RemoveCompromisedCredential) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  password.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});

  store().AddLogin(password);
  RunUntilIdle();

  CredentialWithPassword expected = MakeCompromisedCredential(password);
  expected.password = password.password_value;

  EXPECT_THAT(provider().GetInsecureCredentials(), ElementsAre(expected));

  EXPECT_TRUE(provider().RemoveCredential(expected));
  RunUntilIdle();
  EXPECT_THAT(provider().GetInsecureCredentials(), IsEmpty());
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(InsecureCredentialsManagerTest, RemoveWeakCredential) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1);

  store().AddLogin(password);
  RunUntilIdle();
  provider().StartWeakCheck();
  RunUntilIdle();

  EXPECT_EQ(provider().GetWeakCredentials().size(), 1u);
  EXPECT_TRUE(provider().RemoveCredential(CredentialView(password)));
  RunUntilIdle();
  EXPECT_THAT(GetSavedPasswordForUsername(kExampleCom, kUsername1), IsEmpty());
}

TEST_F(InsecureCredentialsManagerTest, RemoveInsecureCredential) {
  PasswordForm password_form =
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1);
  password_form.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});

  store().AddLogin(password_form);
  RunUntilIdle();
  provider().StartWeakCheck();
  RunUntilIdle();

  CredentialWithPassword expected =
      MakeWeakAndCompromisedCredential(password_form);
  EXPECT_THAT(provider().GetWeakCredentials(), ElementsAre(expected));
  EXPECT_THAT(provider().GetInsecureCredentials(), ElementsAre(expected));

  EXPECT_TRUE(provider().RemoveCredential(expected));
  RunUntilIdle();
  EXPECT_THAT(GetSavedPasswordForUsername(kExampleCom, kUsername1), IsEmpty());
}

// Verifues that GetWeakCredentials() returns sorted weak credentials by using
// CreateSortKey.
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

  EXPECT_THAT(provider().GetWeakCredentials(),
              ElementsAre(MakeWeakCredential(password_forms[0]),
                          MakeWeakCredential(password_forms[1]),
                          MakeWeakCredential(password_forms[2]),
                          MakeWeakCredential(password_forms[3])));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

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

// Tests that verifies mapping compromised credentials to passwords works
// correctly.
TEST_F(InsecureCredentialsManagerWithTwoStoresTest,
       MapCompromisedPasswordsToPasswords) {
  // Add credentials for both `kExampleCom` and `kExampleOrg` in both stores
  // with the same username and difference passwords. For `kUsername1`, the
  // `kPassword1` are `kPassword2` are compromised while
  // `kPassword3` is safe.
  std::vector<PasswordForm> profile_store_passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1),
      MakeSavedPassword(kExampleOrg, kUsername1, kPassword3)};
  for (const PasswordForm& profile_password : profile_store_passwords)
    profile_store().AddLogin(profile_password);

  std::vector<PasswordForm> account_store_passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword3),
      MakeSavedPassword(kExampleOrg, kUsername1, kPassword216)};
  for (const PasswordForm& account_password : account_store_passwords)
    account_store().AddLogin(account_password);

  RunUntilIdle();

  LeakCheckCredential credential = MakeLeakCredential(kUsername1, kPassword3);

  // Mark `kUsername1` and `kPassword3` to be compromised.
  provider().SaveInsecureCredential(credential);

  RunUntilIdle();

  // Each password should be joined only with compromised credential from
  // their store.
  EXPECT_THAT(provider().GetSavedPasswordsFor(CredentialView(
                  kExampleCom, GURL(), kUsername1, kPassword1, base::Time())),
              IsEmpty());

  EXPECT_EQ(provider()
                .GetSavedPasswordsFor(CredentialView(
                    kExampleOrg, GURL(), kUsername1, kPassword3, base::Time()))
                .size(),
            1u);

  EXPECT_EQ(provider()
                .GetSavedPasswordsFor(CredentialView(
                    kExampleCom, GURL(), kUsername1, kPassword3, base::Time()))
                .size(),
            1u);

  EXPECT_THAT(provider().GetSavedPasswordsFor(CredentialView(
                  kExampleOrg, GURL(), kUsername1, kPassword216, base::Time())),
              IsEmpty());
}

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

  EXPECT_EQ(2U, provider().GetInsecureCredentials().size());
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

  EXPECT_EQ(3U, provider().GetInsecureCredentials().size());
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

TEST_F(InsecureCredentialsManagerWithTwoStoresTest,
       RemoveCompromisedCredential) {
  // Add `kUsername1`,`kPassword1` to both stores.
  PasswordForm profile_form =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  profile_form.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});
  PasswordForm account_form =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  account_form.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});
  profile_store().AddLogin(profile_form);
  account_store().AddLogin(account_form);
  RunUntilIdle();

  // Now remove the compromised credentials
  EXPECT_TRUE(provider().RemoveCredential(CredentialView(
      kExampleCom, GURL(), kUsername1, kPassword1, base::Time())));
  RunUntilIdle();

  // It should have been removed from both stores.
  EXPECT_TRUE(profile_store().stored_passwords().at(kExampleCom).empty());
  EXPECT_TRUE(account_store().stored_passwords().at(kExampleCom).empty());
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(InsecureCredentialsManagerWithTwoStoresTest, RemoveWeakCredential) {
  // Add `kUsername1`,`kPassword1` to both stores.
  profile_store().AddLogin(
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1));
  account_store().AddLogin(
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1));
  RunUntilIdle();
  provider().StartWeakCheck();
  RunUntilIdle();

  // Now remove the weak credential
  EXPECT_TRUE(provider().RemoveCredential(CredentialView(
      kExampleCom, GURL(), kUsername1, kWeakPassword1, base::Time())));
  RunUntilIdle();

  // It should have been removed from both stores.
  EXPECT_THAT(profile_store().stored_passwords().at(kExampleCom), IsEmpty());
  EXPECT_THAT(account_store().stored_passwords().at(kExampleCom), IsEmpty());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace password_manager
