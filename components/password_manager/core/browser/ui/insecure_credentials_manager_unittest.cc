// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/timer/elapsed_timer.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

constexpr char kExampleCom[] = "https://example.com";
constexpr char kExampleOrg[] = "https://example.org";

constexpr char kUsername1[] = "alice";
constexpr char kUsername2[] = "bob";

constexpr char kPassword1[] = "f00b4r";
constexpr char kPassword2[] = "s3cr3t";
constexpr char kPassword3[] = "484her";

constexpr char kWeakPassword1[] = "123456";
constexpr char kWeakPassword2[] = "abcdabcdabcdabcdabcdabcdabcdabcdabcdabcda";
constexpr char kStrongPassword1[] = "fnlsr4@cm^mdls@fkspnsg3d";
constexpr char kStrongPassword2[] = "pmsFlsnoab4nsl#losb@skpfnsbkjb^klsnbs!cns";

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

// Delay in milliseconds.
const int kDelay = 2;

struct MockInsecureCredentialsManagerObserver
    : InsecureCredentialsManager::Observer {
  MOCK_METHOD(void,
              OnCompromisedCredentialsChanged,
              (InsecureCredentialsManager::CredentialsView),
              (override));
  MOCK_METHOD(void, OnWeakCredentialsChanged, (), (override));
};

using StrictMockInsecureCredentialsManagerObserver =
    ::testing::StrictMock<MockInsecureCredentialsManagerObserver>;

CompromisedCredentials MakeCompromised(
    base::StringPiece signon_realm,
    base::StringPiece username,
    CompromiseType type = CompromiseType::kLeaked) {
  return {.signon_realm = std::string(signon_realm),
          .username = base::ASCIIToUTF16(username),
          .compromise_type = type};
}

PasswordForm MakeSavedPassword(base::StringPiece signon_realm,
                               base::StringPiece username,
                               base::StringPiece password,
                               base::StringPiece username_element = "") {
  PasswordForm form;
  form.signon_realm = std::string(signon_realm);
  form.url = GURL(signon_realm);
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16(password);
  form.username_element = base::ASCIIToUTF16(username_element);
  return form;
}

LeakCheckCredential MakeLeakCredential(base::StringPiece username,
                                       base::StringPiece password) {
  return LeakCheckCredential(base::ASCIIToUTF16(username),
                             base::ASCIIToUTF16(password));
}

CredentialWithPassword MakeCompromisedCredential(
    const PasswordForm& form,
    const CompromisedCredentials& credential) {
  CredentialWithPassword credential_with_password((CredentialView(form)));
  credential_with_password.create_time = credential.create_time;
  credential_with_password.insecure_type =
      credential.compromise_type == CompromiseType::kLeaked
          ? InsecureCredentialTypeFlags::kCredentialLeaked
          : InsecureCredentialTypeFlags::kCredentialPhished;
  return credential_with_password;
}

CredentialWithPassword MakeWeakCredential(const PasswordForm& form) {
  CredentialWithPassword weak_credential{CredentialView(form)};
  weak_credential.insecure_type = InsecureCredentialTypeFlags::kWeakCredential;
  return weak_credential;
}

CredentialWithPassword MakeWeakAndCompromisedCredential(
    const PasswordForm& form,
    const CompromisedCredentials& credential) {
  CredentialWithPassword credential_with_password =
      MakeCompromisedCredential(form, credential);
  credential_with_password.insecure_type |=
      InsecureCredentialTypeFlags::kWeakCredential;
  return credential_with_password;
}

class InsecureCredentialsManagerTest : public ::testing::Test {
 protected:
  InsecureCredentialsManagerTest() { store_->Init(/*prefs=*/nullptr); }

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
  std::string GetSavedPasswordForUsername(const std::string& signon_realm,
                                          const std::string& username) {
    SavedPasswordsPresenter::SavedPasswordsView saved_passwords =
        presenter_.GetSavedPasswords();
    for (const auto& form : saved_passwords) {
      if (form.signon_realm == signon_realm &&
          form.username_value == base::UTF8ToUTF16(username)) {
        return base::UTF16ToUTF8(form.password_value);
      }
    }
    return std::string();
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
         lhs.create_time == rhs.create_time &&
         lhs.insecure_type == rhs.insecure_type && lhs.password == rhs.password;
}

std::ostream& operator<<(std::ostream& out,
                         const CredentialWithPassword& credential) {
  return out << "{ signon_realm: " << credential.signon_realm
             << ", username: " << credential.username
             << ", create_time: " << credential.create_time
             << ", compromise_type: "
             << static_cast<int>(credential.insecure_type)
             << ", password: " << credential.password << " }";
}

// Tests whether adding and removing an observer works as expected.
TEST_F(InsecureCredentialsManagerTest,
       NotifyObserversAboutCompromisedCredentialChanges) {
  std::vector<CompromisedCredentials> credentials = {
      MakeCompromised(kExampleCom, kUsername1)};

  StrictMockInsecureCredentialsManagerObserver observer;
  provider().AddObserver(&observer);

  // Adding a compromised credential should notify observers.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged);
  store().AddCompromisedCredentials(credentials[0]);
  RunUntilIdle();
  EXPECT_THAT(store().compromised_credentials(), ElementsAreArray(credentials));

  // Adding the exact same credential should not result in a notification, as
  // the database is not actually modified.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(0);
  store().AddCompromisedCredentials(credentials[0]);
  RunUntilIdle();

  // Remove should notify, and observers should be passed an empty list.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged(IsEmpty()));
  store().RemoveCompromisedCredentials(
      credentials[0].signon_realm, credentials[0].username,
      RemoveCompromisedCredentialsReason::kRemove);
  RunUntilIdle();
  EXPECT_THAT(store().compromised_credentials(), IsEmpty());

  // Similarly to repeated add, a repeated remove should not notify either.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(0);
  store().RemoveCompromisedCredentials(
      credentials[0].signon_realm, credentials[0].username,
      RemoveCompromisedCredentialsReason::kRemove);
  RunUntilIdle();

  // After an observer is removed it should no longer receive notifications.
  provider().RemoveObserver(&observer);
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(0);
  store().AddCompromisedCredentials(credentials[0]);
  RunUntilIdle();
  EXPECT_THAT(store().compromised_credentials(), ElementsAreArray(credentials));
}

// Tests removing a compromised credentials by compromise type triggers an
// observer works as expected.
TEST_F(InsecureCredentialsManagerTest,
       NotifyObserversAboutRemovingCompromisedCredentialsByCompromisedType) {
  CompromisedCredentials phished_credentials =
      MakeCompromised(kExampleCom, kUsername1, CompromiseType::kPhished);
  CompromisedCredentials leaked_credentials =
      MakeCompromised(kExampleCom, kUsername1, CompromiseType::kLeaked);

  StrictMockInsecureCredentialsManagerObserver observer;
  provider().AddObserver(&observer);
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged);
  store().AddCompromisedCredentials(phished_credentials);
  RunUntilIdle();
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged);
  store().AddCompromisedCredentials(leaked_credentials);
  RunUntilIdle();

  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(1);
  store().RemoveCompromisedCredentialsByCompromiseType(
      phished_credentials.signon_realm, phished_credentials.username,
      CompromiseType::kPhished, RemoveCompromisedCredentialsReason::kRemove);
  RunUntilIdle();
  EXPECT_THAT(store().compromised_credentials(),
              ElementsAre(leaked_credentials));

  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(1);
  store().RemoveCompromisedCredentialsByCompromiseType(
      leaked_credentials.signon_realm, leaked_credentials.username,
      CompromiseType::kLeaked, RemoveCompromisedCredentialsReason::kRemove);
  RunUntilIdle();
  EXPECT_THAT(store().compromised_credentials(), IsEmpty());
  provider().RemoveObserver(&observer);
}

// Tests whether adding and removing an observer works as expected.
TEST_F(InsecureCredentialsManagerTest,
       NotifyObserversAboutSavedPasswordsChanges) {
  StrictMockInsecureCredentialsManagerObserver observer;
  provider().AddObserver(&observer);

  PasswordForm saved_password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);

  // Adding a saved password should notify observers.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged);
  EXPECT_CALL(observer, OnWeakCredentialsChanged);
  store().AddLogin(saved_password);
  RunUntilIdle();

  // Updating a saved password should notify observers.
  saved_password.password_value = base::ASCIIToUTF16(kPassword2);
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged);
  EXPECT_CALL(observer, OnWeakCredentialsChanged);
  store().UpdateLogin(saved_password);
  RunUntilIdle();

  // Removing a saved password should notify observers.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged);
  EXPECT_CALL(observer, OnWeakCredentialsChanged);
  store().RemoveLogin(saved_password);
  RunUntilIdle();

  // After an observer is removed it should no longer receive notifications.
  provider().RemoveObserver(&observer);
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(0);
  EXPECT_CALL(observer, OnWeakCredentialsChanged).Times(0);
  store().AddLogin(saved_password);
  RunUntilIdle();
}

// Tests that the provider is able to join a single password with a compromised
// credential.
TEST_F(InsecureCredentialsManagerTest, JoinSingleCredentials) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);

  store().AddLogin(password);
  store().AddCompromisedCredentials(credential);
  RunUntilIdle();

  CredentialWithPassword expected =
      MakeCompromisedCredential(password, credential);
  expected.password = password.password_value;
  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected));
}

// Tests that the provider is able to join a password with a credential that was
// compromised in multiple ways.
TEST_F(InsecureCredentialsManagerTest, JoinPhishedAndLeaked) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  CompromisedCredentials leaked =
      MakeCompromised(kExampleCom, kUsername1, CompromiseType::kLeaked);
  CompromisedCredentials phished =
      MakeCompromised(kExampleCom, kUsername1, CompromiseType::kPhished);

  store().AddLogin(password);
  store().AddCompromisedCredentials(leaked);
  store().AddCompromisedCredentials(phished);
  RunUntilIdle();

  CredentialWithPassword expected = MakeCompromisedCredential(password, leaked);
  expected.password = password.password_value;
  expected.insecure_type = (InsecureCredentialTypeFlags::kCredentialLeaked |
                            InsecureCredentialTypeFlags::kCredentialPhished);

  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected));
}

// Tests that the provider reacts whenever the saved passwords or the
// compromised credentials change.
TEST_F(InsecureCredentialsManagerTest, ReactToChangesInBothTables) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1),
      MakeSavedPassword(kExampleCom, kUsername2, kPassword2)};

  std::vector<CompromisedCredentials> credentials = {
      MakeCompromised(kExampleCom, kUsername1),
      MakeCompromised(kExampleCom, kUsername2)};

  std::vector<CredentialWithPassword> expected = {
      MakeCompromisedCredential(passwords[0], credentials[0]),
      MakeCompromisedCredential(passwords[1], credentials[1])};

  store().AddLogin(passwords[0]);
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(), IsEmpty());

  store().AddCompromisedCredentials(credentials[0]);
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected[0]));

  store().AddLogin(passwords[1]);
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected[0]));

  store().AddCompromisedCredentials(credentials[1]);
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(),
              ElementsAreArray(expected));

  store().RemoveLogin(passwords[0]);
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected[1]));

  store().RemoveLogin(passwords[1]);
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(), IsEmpty());
}

// Tests that the provider is able to join multiple passwords with compromised
// credentials.
TEST_F(InsecureCredentialsManagerTest, JoinMultipleCredentials) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1),
      MakeSavedPassword(kExampleCom, kUsername2, kPassword2)};

  std::vector<CompromisedCredentials> credentials = {
      MakeCompromised(kExampleCom, kUsername1),
      MakeCompromised(kExampleCom, kUsername2)};

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddCompromisedCredentials(credentials[0]);
  store().AddCompromisedCredentials(credentials[1]);
  RunUntilIdle();

  CredentialWithPassword expected1 =
      MakeCompromisedCredential(passwords[0], credentials[0]);
  CredentialWithPassword expected2 =
      MakeCompromisedCredential(passwords[1], credentials[1]);

  EXPECT_THAT(provider().GetCompromisedCredentials(),
              ElementsAre(expected1, expected2));
}

// Tests that joining a compromised credential with saved passwords with a
// different username results in an empty list.
TEST_F(InsecureCredentialsManagerTest, JoinWitDifferentUsername) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername2, kPassword1),
      MakeSavedPassword(kExampleCom, kUsername2, kPassword2)};

  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddCompromisedCredentials(credential);
  RunUntilIdle();

  EXPECT_THAT(provider().GetCompromisedCredentials(), IsEmpty());
}

// Tests that joining a compromised credential with saved passwords with a
// matching username but different signon_realm results in an empty list.
TEST_F(InsecureCredentialsManagerTest, JoinWitDifferentSignonRealm) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleOrg, kUsername1, kPassword1),
      MakeSavedPassword(kExampleOrg, kUsername1, kPassword2)};

  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddCompromisedCredentials(credential);
  RunUntilIdle();

  EXPECT_THAT(provider().GetCompromisedCredentials(), IsEmpty());
}

// Tests that joining a compromised credential with multiple saved passwords for
// the same signon_realm and username combination results in multiple entries
// when the passwords are distinct.
TEST_F(InsecureCredentialsManagerTest, JoinWithMultipleDistinctPasswords) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1, "element_1"),
      MakeSavedPassword(kExampleCom, kUsername1, kPassword2, "element_2")};

  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddCompromisedCredentials(credential);
  RunUntilIdle();

  CredentialWithPassword expected1 =
      MakeCompromisedCredential(passwords[0], credential);
  CredentialWithPassword expected2 =
      MakeCompromisedCredential(passwords[1], credential);

  EXPECT_THAT(provider().GetCompromisedCredentials(),
              ElementsAre(expected1, expected2));
}

// Tests that joining a compromised credential with multiple saved passwords for
// the same signon_realm and username combination results in a single entry
// when the passwords are the same.
TEST_F(InsecureCredentialsManagerTest, JoinWithMultipleRepeatedPasswords) {
  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1, "element_1"),
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1, "element_2")};

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddCompromisedCredentials(credential);
  RunUntilIdle();

  CredentialWithPassword expected =
      MakeCompromisedCredential(passwords[0], credential);

  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected));
}

// Tests that verifies mapping compromised credentials to passwords works
// correctly.
TEST_F(InsecureCredentialsManagerTest, MapCompromisedPasswordsToPasswords) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1, "element_1"),
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1, "element_2"),
      MakeSavedPassword(kExampleOrg, kUsername2, kPassword2)};

  std::vector<CompromisedCredentials> credentials = {
      MakeCompromised(kExampleCom, kUsername1),
      MakeCompromised(kExampleOrg, kUsername2)};

  std::vector<CredentialWithPassword> credentials_with_password = {
      MakeCompromisedCredential(passwords[0], credentials[0]),
      MakeCompromisedCredential(passwords[1], credentials[0]),
      MakeCompromisedCredential(passwords[2], credentials[1])};

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddLogin(passwords[2]);
  store().AddCompromisedCredentials(credentials[0]);
  store().AddCompromisedCredentials(credentials[1]);

  RunUntilIdle();
  EXPECT_THAT(provider().GetSavedPasswordsFor(credentials_with_password[0]),
              ElementsAreArray(store().stored_passwords().at(kExampleCom)));

  EXPECT_THAT(provider().GetSavedPasswordsFor(credentials_with_password[1]),
              ElementsAreArray(store().stored_passwords().at(kExampleCom)));

  EXPECT_THAT(provider().GetSavedPasswordsFor(credentials_with_password[2]),
              ElementsAreArray(store().stored_passwords().at(kExampleOrg)));
}

TEST_F(InsecureCredentialsManagerTest, StartWeakCheckOnEmptyPasswordsList) {
  EXPECT_THAT(
      histogram_tester().GetTotalCountsForPrefix("PasswordManager.WeakCheck"),
      IsEmpty());

  RunUntilIdle();
  provider().StartWeakCheck();
  AdvanceClock(base::TimeDelta::FromMilliseconds(kDelay));
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
      MakeSavedPassword(kExampleCom, kUsername1, kStrongPassword1),
      MakeSavedPassword(kExampleCom, kUsername2, kStrongPassword2)};

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  EXPECT_THAT(
      histogram_tester().GetTotalCountsForPrefix("PasswordManager.WeakCheck"),
      IsEmpty());

  RunUntilIdle();
  provider().StartWeakCheck();
  AdvanceClock(base::TimeDelta::FromMilliseconds(2 * kDelay));
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
      MakeSavedPassword(kExampleCom, kUsername2, kStrongPassword1)};

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  EXPECT_THAT(
      histogram_tester().GetTotalCountsForPrefix("PasswordManager.WeakCheck"),
      IsEmpty());

  RunUntilIdle();
  provider().StartWeakCheck();
  AdvanceClock(base::TimeDelta::FromMilliseconds(kDelay));
  RunUntilIdle();

  std::vector<CredentialWithPassword> weak_credentials =
      provider().GetWeakCredentials();

  ASSERT_EQ(weak_credentials.size(), 1u);
  EXPECT_EQ(base::UTF16ToUTF8(weak_credentials[0].password), kWeakPassword1);
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
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1, "element_1"),
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword2, "element_2")};

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);

  RunUntilIdle();
  provider().StartWeakCheck();
  AdvanceClock(base::TimeDelta::FromMilliseconds(kDelay));
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
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1, "element_1"),
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1, "element_2")};

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);

  RunUntilIdle();
  provider().StartWeakCheck();
  AdvanceClock(base::TimeDelta::FromMilliseconds(kDelay));
  RunUntilIdle();

  std::vector<CredentialWithPassword> weak_credentials =
      provider().GetWeakCredentials();

  ASSERT_EQ(weak_credentials.size(), 1u);
  EXPECT_EQ(base::UTF16ToUTF8(weak_credentials[0].password), kWeakPassword1);
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
      MakeSavedPassword(kExampleCom, kUsername2, kStrongPassword1)};
  std::vector<CompromisedCredentials> compromised_credentials = {
      MakeCompromised(kExampleCom, kUsername1),
      MakeCompromised(kExampleCom, kUsername2)};

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddCompromisedCredentials(compromised_credentials[0]);
  store().AddCompromisedCredentials(compromised_credentials[1]);

  RunUntilIdle();
  provider().StartWeakCheck();
  AdvanceClock(base::TimeDelta::FromMilliseconds(kDelay));
  RunUntilIdle();

  std::vector<CredentialWithPassword> returned_weak_credentials =
      provider().GetWeakCredentials();
  std::vector<CredentialWithPassword> returned_compromised_credentials =
      provider().GetCompromisedCredentials();

  ASSERT_EQ(returned_weak_credentials.size(), 1u);
  EXPECT_EQ(base::UTF16ToUTF8(returned_weak_credentials[0].password),
            kWeakPassword1);
  EXPECT_TRUE(IsWeak(returned_weak_credentials[0].insecure_type));

  ASSERT_EQ(returned_compromised_credentials.size(), 2u);
  EXPECT_TRUE(IsCompromised(returned_compromised_credentials[0].insecure_type));
  EXPECT_TRUE(IsCompromised(returned_compromised_credentials[1].insecure_type));

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
// getWeakCredentials and GetCompromisedCredentials will return this credential
// in one instance.
TEST_F(InsecureCredentialsManagerTest, SingleCredentialIsWeakAndCompromised) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1)};
  std::vector<CompromisedCredentials> compromised_credentials = {
      MakeCompromised(kExampleCom, kUsername1)};

  store().AddLogin(passwords[0]);
  store().AddCompromisedCredentials(compromised_credentials[0]);

  RunUntilIdle();
  provider().StartWeakCheck();
  AdvanceClock(base::TimeDelta::FromMilliseconds(kDelay));
  RunUntilIdle();

  std::vector<CredentialWithPassword> returned_weak_credentials =
      provider().GetWeakCredentials();
  std::vector<CredentialWithPassword> returned_compromised_credentials =
      provider().GetCompromisedCredentials();

  // Since the credential is weak and compromised, the |insecure_type| should be
  // weak and compromised for elements from |returned_weak_credentials| and
  // |returned_compromised_credentials|.
  ASSERT_EQ(returned_weak_credentials.size(), 1u);
  EXPECT_EQ(base::UTF16ToUTF8(returned_weak_credentials[0].password),
            kWeakPassword1);
  EXPECT_TRUE(IsWeak(returned_weak_credentials[0].insecure_type));
  EXPECT_TRUE(IsCompromised(returned_weak_credentials[0].insecure_type));

  ASSERT_EQ(returned_compromised_credentials.size(), 1u);
  EXPECT_EQ(base::UTF16ToUTF8(returned_compromised_credentials[0].password),
            kWeakPassword1);
  EXPECT_TRUE(IsWeak(returned_compromised_credentials[0].insecure_type));
  EXPECT_TRUE(IsCompromised(returned_compromised_credentials[0].insecure_type));

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.CheckedPasswords", 1, 1);
  histogram_tester().ExpectUniqueSample("PasswordManager.WeakCheck.Time",
                                        kDelay, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.WeakPasswords", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.WeakCheck.PasswordScore", 0, 1);
}

// Test verifies that saving LeakCheckCredential via provider adds expected
// compromised credential.
TEST_F(InsecureCredentialsManagerTest, SaveCompromisedPassword) {
  PasswordForm password_form =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  LeakCheckCredential credential = MakeLeakCredential(kUsername1, kPassword1);
  CompromisedCredentials compromised_credential =
      MakeCompromised(kExampleCom, kUsername1);

  store().AddLogin(password_form);
  RunUntilIdle();

  CredentialWithPassword expected =
      MakeCompromisedCredential(password_form, compromised_credential);
  expected.create_time = base::Time::Now();

  provider().SaveCompromisedCredential(credential);
  RunUntilIdle();

  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected));
}

// Test verifies that editing Compromised Credential via provider change the
// original password form.
TEST_F(InsecureCredentialsManagerTest, UpdateCompromisedPassword) {
  PasswordForm password_form =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);

  store().AddLogin(password_form);
  store().AddCompromisedCredentials(credential);

  RunUntilIdle();
  CredentialWithPassword expected =
      MakeCompromisedCredential(password_form, credential);

  EXPECT_TRUE(provider().UpdateCredential(expected, kPassword2));
  RunUntilIdle();
  expected.password = base::UTF8ToUTF16(kPassword2);

  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected));
}

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
            kStrongPassword1);
}

// Test verifies that editing credential that is weak and compromised via
// provider change the saved password.
TEST_F(InsecureCredentialsManagerTest, UpdateInsecurePassword) {
  PasswordForm password_form =
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1);
  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);

  store().AddLogin(password_form);
  store().AddCompromisedCredentials(credential);
  RunUntilIdle();
  provider().StartWeakCheck();
  RunUntilIdle();

  CredentialWithPassword expected =
      MakeWeakAndCompromisedCredential(password_form, credential);
  EXPECT_THAT(provider().GetWeakCredentials(), ElementsAre(expected));
  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected));

  EXPECT_TRUE(provider().UpdateCredential(expected, kStrongPassword1));
  RunUntilIdle();

  EXPECT_EQ(GetSavedPasswordForUsername(kExampleCom, kUsername1),
            kStrongPassword1);
}

TEST_F(InsecureCredentialsManagerTest, RemoveCompromisedCredential) {
  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);

  store().AddLogin(password);
  store().AddCompromisedCredentials(credential);
  RunUntilIdle();

  CredentialWithPassword expected =
      MakeCompromisedCredential(password, credential);
  expected.password = password.password_value;

  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected));

  EXPECT_TRUE(provider().RemoveCredential(expected));
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(), IsEmpty());
}

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
  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);

  store().AddLogin(password_form);
  store().AddCompromisedCredentials(credential);
  RunUntilIdle();
  provider().StartWeakCheck();
  RunUntilIdle();

  CredentialWithPassword expected =
      MakeWeakAndCompromisedCredential(password_form, credential);
  EXPECT_THAT(provider().GetWeakCredentials(), ElementsAre(expected));
  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected));

  EXPECT_TRUE(provider().RemoveCredential(expected));
  RunUntilIdle();
  EXPECT_THAT(GetSavedPasswordForUsername(kExampleCom, kUsername1), IsEmpty());
}

// Verifues that GetWeakCredentials() returns sorted weak credentials by using
// CreateSortKey.
TEST_F(InsecureCredentialsManagerTest, GetWeakCredentialsReturnsSortedData) {
  const std::vector<PasswordForm> password_forms = {
      MakeSavedPassword("http://example-a.com", "user_a1", "pwd"),
      MakeSavedPassword("http://example-a.com", "user_a2", "pwd"),
      MakeSavedPassword("http://example-b.com", "user_a", "pwd"),
      MakeSavedPassword("http://example-c.com", "user_a", "pwd")};
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

namespace {
class InsecureCredentialsManagerWithTwoStoresTest : public ::testing::Test {
 protected:
  InsecureCredentialsManagerWithTwoStoresTest() {
    profile_store_->Init(/*prefs=*/nullptr);
    account_store_->Init(/*prefs=*/nullptr);
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
      MakeSavedPassword(kExampleOrg, kUsername1, kPassword2)};
  for (const PasswordForm& account_password : account_store_passwords)
    account_store().AddLogin(account_password);

  // Mark `kPassword1` to be compromised in the profile store, and `kPassword2`
  // to be compromised in the account store.
  profile_store().AddCompromisedCredentials(
      MakeCompromised(kExampleCom, kUsername1));
  account_store().AddCompromisedCredentials(
      MakeCompromised(kExampleOrg, kUsername1));

  RunUntilIdle();

  // Each password should be joined only with compromised credential from
  // their store.
  EXPECT_THAT(
      provider().GetSavedPasswordsFor(
          CredentialView(kExampleCom, GURL(), base::ASCIIToUTF16(kUsername1),
                         base::ASCIIToUTF16(kPassword1))),
      ElementsAreArray(profile_store().stored_passwords().at(kExampleCom)));

  EXPECT_THAT(provider().GetSavedPasswordsFor(CredentialView(
                  kExampleOrg, GURL(), base::ASCIIToUTF16(kUsername1),
                  base::ASCIIToUTF16(kPassword3))),
              IsEmpty());

  EXPECT_THAT(provider().GetSavedPasswordsFor(CredentialView(
                  kExampleCom, GURL(), base::ASCIIToUTF16(kUsername1),
                  base::ASCIIToUTF16(kPassword3))),
              IsEmpty());

  EXPECT_THAT(
      provider().GetSavedPasswordsFor(
          CredentialView(kExampleOrg, GURL(), base::ASCIIToUTF16(kUsername1),
                         base::ASCIIToUTF16(kPassword2))),
      ElementsAreArray(account_store().stored_passwords().at(kExampleOrg)));
}

// Test verifies that saving LeakCheckCredential via provider adds expected
// compromised credential to the correct store.
TEST_F(InsecureCredentialsManagerWithTwoStoresTest, SaveCompromisedPassword) {
  ASSERT_TRUE(profile_store().compromised_credentials().empty());
  ASSERT_TRUE(account_store().compromised_credentials().empty());
  // Add `kUsername1`,`kPassword1` to both stores.
  // And add `kUsername1`,`kPassword2` to the account store only.
  profile_store().AddLogin(
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1));

  account_store().AddLogin(
      MakeSavedPassword(kExampleOrg, kUsername1, kPassword1));
  account_store().AddLogin(
      MakeSavedPassword(kExampleCom, kUsername1, kPassword2));

  RunUntilIdle();

  // Mark `kUsername1`, `kPassword1` as compromised, a new entry should be
  // added to both stores.
  provider().SaveCompromisedCredential(
      MakeLeakCredential(kUsername1, kPassword1));
  RunUntilIdle();

  EXPECT_EQ(1U, profile_store().compromised_credentials().size());
  EXPECT_EQ(1U, account_store().compromised_credentials().size());

  // Now, mark `kUsername1`, `kPassword2` as compromised, a new entry should be
  // added only to the account store.
  provider().SaveCompromisedCredential(
      MakeLeakCredential(kUsername1, kPassword2));
  RunUntilIdle();

  EXPECT_EQ(1U, profile_store().compromised_credentials().size());
  EXPECT_EQ(2U, account_store().compromised_credentials().size());
}

TEST_F(InsecureCredentialsManagerWithTwoStoresTest,
       RemoveCompromisedCredential) {
  // Add `kUsername1`,`kPassword1` to both stores.
  profile_store().AddLogin(
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  account_store().AddLogin(
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1));

  // Mark `kUsername1` and `kPassword1` to be compromised in both stores.
  profile_store().AddCompromisedCredentials(
      MakeCompromised(kExampleCom, kUsername1));
  account_store().AddCompromisedCredentials(
      MakeCompromised(kExampleCom, kUsername1));
  RunUntilIdle();

  // Now remove the compromised credentials
  EXPECT_TRUE(provider().RemoveCredential(
      CredentialView(kExampleCom, GURL(), base::ASCIIToUTF16(kUsername1),
                     base::ASCIIToUTF16(kPassword1))));
  RunUntilIdle();

  // It should have been removed from both stores.
  EXPECT_TRUE(profile_store().stored_passwords().at(kExampleCom).empty());
  EXPECT_TRUE(account_store().stored_passwords().at(kExampleCom).empty());
}

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
  EXPECT_TRUE(provider().RemoveCredential(
      CredentialView(kExampleCom, GURL(), base::ASCIIToUTF16(kUsername1),
                     base::ASCIIToUTF16(kWeakPassword1))));
  RunUntilIdle();

  // It should have been removed from both stores.
  EXPECT_THAT(profile_store().stored_passwords().at(kExampleCom), IsEmpty());
  EXPECT_THAT(account_store().stored_passwords().at(kExampleCom), IsEmpty());
}

}  // namespace password_manager
