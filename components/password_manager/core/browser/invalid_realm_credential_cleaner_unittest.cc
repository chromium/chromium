// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/invalid_realm_credential_cleaner.h"

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

struct MigrationFormsPair {
  autofill::PasswordForm http_form;
  autofill::PasswordForm migrated_https_form;
};

constexpr struct HttpMatchState {
  bool same_creation_time;
  bool same_origin;
  bool same_username;
  // This member is just the logical conjunction on the members above.
  bool is_matching;
} kHttpMatchStates[] = {
    {false, false, false, false}, {false, false, true, false},
    {false, true, false, false},  {false, true, true, false},
    {true, false, false, false},  {true, false, true, false},
    {true, true, false, false},   {true, true, true, true},
};

// |kDates|, |kOrigins|, and |kUsernames| are used to easily manipulate the
// fields of HTTP and HTTPS forms that should be equal. The HTTP forms will
// use the component [1] from these arrays. The HTTPS forms will use [1] if the
// field (given by array name) should be the same and [0] otherwise. Component
// [2] is used when we want to create a form with the field different from both
// ([0] and [1]).
const base::Time kDates[] = {base::Time::FromDoubleT(100),
                             base::Time::FromDoubleT(200),
                             base::Time::FromDoubleT(300)};

const GURL kOrigins[] = {GURL("https://example.org/path-0/"),
                         GURL("https://example.org/path-1/"),
                         GURL("https://google.com/path/")};

const base::string16 kUsernames[] = {base::ASCIIToUTF16("user0"),
                                     base::ASCIIToUTF16("user1")};

bool StoreContains(TestPasswordStore* store,
                   const autofill::PasswordForm& form) {
  const auto it = store->stored_passwords().find(form.signon_realm);
  return it != store->stored_passwords().end() &&
         base::ContainsValue(it->second, form);
}

// Returns the HTTP form for given parameters and the migrated version of that
// form.
MigrationFormsPair GetCredentialsFrom(bool is_blacklisted,
                                      bool invalid_signon_realm,
                                      const HttpMatchState& http_match_state) {
  autofill::PasswordForm http_form;
  http_form.origin = GURL("http://example.org/path-1/");
  http_form.signon_realm = "http://example.org/";
  http_form.date_created = kDates[1];
  http_form.username_value =
      (is_blacklisted ? base::string16() : kUsernames[1]);
  http_form.blacklisted_by_user = is_blacklisted;

  autofill::PasswordForm https_form;
  https_form.origin = kOrigins[http_match_state.same_origin];
  https_form.signon_realm = (invalid_signon_realm ? https_form.origin.spec()
                                                  : "https://example.org/");
  https_form.date_created = kDates[http_match_state.same_creation_time];
  https_form.username_value =
      (is_blacklisted ? base::string16()
                      : kUsernames[http_match_state.same_username]);
  https_form.blacklisted_by_user = is_blacklisted;
  return {http_form, https_form};
}

}  // namespace

class MockCredentialsCleanerObserver : public CredentialsCleaner::Observer {
 public:
  MockCredentialsCleanerObserver() = default;
  ~MockCredentialsCleanerObserver() override = default;
  MOCK_METHOD0(CleaningCompleted, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCredentialsCleanerObserver);
};

// This test checks that HTML credentials are correctly removed for the case
// when both (HTTP and HTTPS credentials) are blacklisted, or both are not
// blacklsited.
TEST(InvalidRealmCredentialCleanerTest,
     RemoveHtmlCredentialsWithWrongSignonRealm) {
  enum class ExpectedOperation { kNothing, kFix, kDelete };

  static constexpr struct HttpsMatchState {
    bool same_signon_realm;
    bool same_username;
    // This member is just the logical conjunction on the members above.
    bool is_matching;
  } kHttpsMatchStates[] = {{false, false, false},
                           {false, true, false},
                           {true, false, false},
                           {true, true, true}};

  struct TestCase {
    bool is_blacklisted;
    bool invalid_signon_realm;
    HttpMatchState http_state;
    HttpsMatchState https_state;
    ExpectedOperation expected_operation;
  };

  std::vector<TestCase> cases;
  for (bool is_blacklisted : {true, false}) {
    for (bool invalid_signon_realm : {true, false}) {
      for (const HttpMatchState& http_state : kHttpMatchStates) {
        for (const HttpsMatchState& https_state : kHttpsMatchStates) {
          // This state is invalid since blacklisted credentials have username
          // cleared.
          if (is_blacklisted &&
              (!http_state.same_username || !https_state.same_username))
            continue;

          ExpectedOperation expected_operation = ExpectedOperation::kNothing;
          if (invalid_signon_realm) {
            if (is_blacklisted || http_state.is_matching ||
                https_state.is_matching)
              expected_operation = ExpectedOperation::kDelete;
            else
              expected_operation = ExpectedOperation::kFix;
          }
          cases.push_back({is_blacklisted, invalid_signon_realm, http_state,
                           https_state, expected_operation});
        }
      }
    }
  }

  for (const TestCase& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "is_blacklisted=" << test.is_blacklisted
                 << ", invalid_signon_realm=" << test.invalid_signon_realm
                 << ", http_match=" << test.http_state.is_matching
                 << ", http_same_username=" << test.http_state.same_username
                 << ", http_same_creation_time="
                 << test.http_state.same_creation_time
                 << ", http_same_origin=" << test.http_state.same_origin
                 << ", https_match=" << test.https_state.is_matching
                 << ", https_same_signon_realm="
                 << test.https_state.same_signon_realm
                 << ", https_ssame_username=" << test.https_state.same_username
                 << ", expected_operation="
                 << static_cast<int>(test.expected_operation));

    base::test::ScopedTaskEnvironment scoped_task_environment;
    auto password_store = base::MakeRefCounted<TestPasswordStore>();
    ASSERT_TRUE(password_store->Init(syncer::SyncableService::StartSyncFlare(),
                                     nullptr));

    const auto migration_pair = GetCredentialsFrom(
        test.is_blacklisted, test.invalid_signon_realm, test.http_state);

    // |https_form| is the form to be tested.
    const autofill::PasswordForm& http_form_to_match = migration_pair.http_form;
    const autofill::PasswordForm& https_form =
        migration_pair.migrated_https_form;

    autofill::PasswordForm https_form_to_match = https_form;
    // |https_form| have one of kOrigins[0] or kOrigins[1].
    // If we don't want to match |https_form| with |https_form_to_match| by
    // web origin then we can simply use kOrigins[2].
    if (!test.https_state.same_signon_realm) {
      https_form_to_match.origin = kOrigins[2];
    } else {
      // Make |https_form| and |https_form_to_match| to have different unique
      // key.
      https_form_to_match.origin =
          GURL(https_form_to_match.origin.spec() + "/different/");
    }

    // Make sure that HTTPS credentials we want to match with have valid
    // signon_realm.
    https_form_to_match.signon_realm =
        https_form_to_match.origin.GetOrigin().spec();

    if (!test.https_state.same_username)
      https_form_to_match.username_value += base::ASCIIToUTF16("different");
    // Even if |https_form| and |https_form_to_match| have the same signon_realm
    // and username, they have to be created at different date of creation to
    // simulate the real behavior when user manually adds credentials for the
    // website.
    https_form_to_match.date_created = kDates[2];

    password_store->AddLogin(http_form_to_match);
    password_store->AddLogin(https_form);
    // The store will save |https_form| twice if it is equals with
    // |https_form_to_match|.
    password_store->AddLogin(https_form_to_match);

    scoped_task_environment.RunUntilIdle();
    // Check that all credentials were successfully added.
    ASSERT_TRUE(StoreContains(password_store.get(), http_form_to_match));
    ASSERT_TRUE(StoreContains(password_store.get(), https_form));
    ASSERT_TRUE(StoreContains(password_store.get(), https_form_to_match));

    TestingPrefServiceSimple prefs;
    // Prevent cleaning of duplicated blacklist entries.
    prefs.registry()->RegisterBooleanPref(
        prefs::kDuplicatedBlacklistedCredentialsRemoved, true);
    prefs.registry()->RegisterBooleanPref(
        prefs::kCredentialsWithWrongSignonRealmRemoved, false);

    MockCredentialsCleanerObserver observer;
    InvalidRealmCredentialCleaner cleaner(password_store, &prefs);
    EXPECT_CALL(observer, CleaningCompleted);
    cleaner.StartCleaning(&observer);
    scoped_task_environment.RunUntilIdle();

    EXPECT_EQ(StoreContains(password_store.get(), https_form),
              (test.expected_operation == ExpectedOperation::kNothing));

    autofill::PasswordForm https_form_fixed = https_form;
    https_form_fixed.signon_realm = https_form_fixed.origin.GetOrigin().spec();
    // Check that they are fixed only when we expect them to be fixed.
    EXPECT_EQ(StoreContains(password_store.get(), https_form_fixed),
              !test.invalid_signon_realm ||
                  (test.expected_operation == ExpectedOperation::kFix));
    EXPECT_TRUE(
        prefs.GetBoolean(prefs::kCredentialsWithWrongSignonRealmRemoved));

    // HTTP form and valid HTTPS form have to stay untouched.
    EXPECT_TRUE(StoreContains(password_store.get(), http_form_to_match));
    EXPECT_TRUE(StoreContains(password_store.get(), https_form_to_match));

    password_store->ShutdownOnUIThread();
    scoped_task_environment.RunUntilIdle();
  }
}

// This test checks that non-HTML credentials with invalid signon_realm are
// correctly deleted.
TEST(InvalidRealmCredentialCleanerTest,
     RemoveNonHtmlCredentialsWithWrongSignonRealm) {
  static constexpr struct SignonRealmState {
    // Indicates whether HTTPS credentials were faulty migrated (the
    // signon_realm contains the whole URL).
    bool invalid_signon_realm;

    // Indicates whether HTTP credentials which will be matched with HTTPS
    // credentials had empty auth realm.
    bool http_empty_realm;

    // Indicates whether HTTPS credentials which were migrated have empty auth
    // realm. If |invalid_signon_realm| is true then |https_empty_realm| should
    // also be true.
    bool https_empty_realm;

    // Indicates whether signon_realms (excluding their protocol) of HTTP
    // credentials and migrated HTTPS credentials are matching.
    bool matching_signon_realm;
  } kSignonRealmStates[]{
      {false, false, false, true},
      {false, false, true, false},
      {false, true, false, false},
      {false, true, true, true},
      // For the next test matching_signon_realm is not
      // set true because invalid_signon_realm means that
      // HTTPS credentials will consists of the whole URL
      // which is different by HTTP signon_realm (excluding protocol).
      {true, true, true, false},
      {true, false, true, false}};

  struct TestCase {
    bool is_blacklisted;
    HttpMatchState http_state;
    SignonRealmState signon_realm_state;
    bool delete_expected;
  };

  std::vector<TestCase> cases;
  for (bool is_blacklisted : {true, false}) {
    for (const HttpMatchState& http_state : kHttpMatchStates) {
      for (const SignonRealmState& signon_realm_state : kSignonRealmStates) {
        // This state is invalid because blacklisted credentials has username
        // cleared.
        if (is_blacklisted && !http_state.same_username)
          continue;
        // This state is invalid because if the signon_realm is valid and there
        // is a matching HTTP credential means that the migration was
        // successful. So, signon_realms should be the same. If they are not,
        // then state is invalid.
        if (!signon_realm_state.invalid_signon_realm &&
            http_state.is_matching && !signon_realm_state.matching_signon_realm)
          continue;
        cases.push_back({is_blacklisted, http_state, signon_realm_state,
                         signon_realm_state.invalid_signon_realm &
                             http_state.is_matching &
                             !signon_realm_state.matching_signon_realm});
      }
    }
  }

  for (const TestCase& test : cases) {
    SCOPED_TRACE(
        testing::Message()
        << "is_blacklisted=" << test.is_blacklisted
        << ", http_empty_realm=" << test.signon_realm_state.http_empty_realm
        << ", https_empty_realm=" << test.signon_realm_state.https_empty_realm
        << ", invalid_signon_realm="
        << test.signon_realm_state.invalid_signon_realm
        << ", same_creation_time=" << test.http_state.same_creation_time
        << ", same_origin=" << test.http_state.same_origin
        << ", same_username=" << test.http_state.same_username
        << ", delete_expected=" << test.delete_expected);
    base::test::ScopedTaskEnvironment scoped_task_environment;
    auto password_store = base::MakeRefCounted<TestPasswordStore>();
    ASSERT_TRUE(password_store->Init(syncer::SyncableService::StartSyncFlare(),
                                     nullptr));

    auto migration_pair = GetCredentialsFrom(
        test.is_blacklisted, test.signon_realm_state.invalid_signon_realm,
        test.http_state);
    autofill::PasswordForm& http_form = migration_pair.http_form;
    autofill::PasswordForm& https_form = migration_pair.migrated_https_form;

    http_form.scheme = autofill::PasswordForm::SCHEME_BASIC;
    https_form.scheme = autofill::PasswordForm::SCHEME_BASIC;

    // If the signon_realm is invalid that means the auth realm is empty.
    if (test.signon_realm_state.invalid_signon_realm) {
      ASSERT_TRUE(test.signon_realm_state.https_empty_realm);
    }
    if (!test.signon_realm_state.http_empty_realm) {
      http_form.signon_realm += "realm";
    }
    if (!test.signon_realm_state.https_empty_realm) {
      https_form.signon_realm += "realm";
    }

    password_store->AddLogin(http_form);
    password_store->AddLogin(https_form);

    scoped_task_environment.RunUntilIdle();
    // Check that credentials were successfully added.
    ASSERT_TRUE(StoreContains(password_store.get(), http_form));
    ASSERT_TRUE(StoreContains(password_store.get(), https_form));

    TestingPrefServiceSimple prefs;
    // Prevent cleaning of duplicated blacklist entries.
    prefs.registry()->RegisterBooleanPref(
        prefs::kDuplicatedBlacklistedCredentialsRemoved, true);
    prefs.registry()->RegisterBooleanPref(
        prefs::kCredentialsWithWrongSignonRealmRemoved, false);

    MockCredentialsCleanerObserver observer;
    InvalidRealmCredentialCleaner cleaner(password_store, &prefs);
    EXPECT_CALL(observer, CleaningCompleted);
    cleaner.StartCleaning(&observer);
    scoped_task_environment.RunUntilIdle();

    EXPECT_NE(StoreContains(password_store.get(), https_form),
              test.delete_expected);
    EXPECT_TRUE(StoreContains(password_store.get(), http_form));
    EXPECT_TRUE(
        prefs.GetBoolean(prefs::kCredentialsWithWrongSignonRealmRemoved));

    password_store->ShutdownOnUIThread();
    scoped_task_environment.RunUntilIdle();
  }
}

// This test checks that credentials that are not HTTP or HTTPS will be
// untouched by the cleaning.
TEST(InvalidRealmCredentialCleanerTest,
     NotHttpAndHttpsCredentialsAreNotRemoved) {
  base::test::ScopedTaskEnvironment scoped_task_environment;
  auto password_store = base::MakeRefCounted<TestPasswordStore>();
  ASSERT_TRUE(
      password_store->Init(syncer::SyncableService::StartSyncFlare(), nullptr));

  autofill::PasswordForm file_url;
  file_url.origin = GURL("file://something.html");
  password_store->AddLogin(file_url);

  autofill::PasswordForm ftp_url;
  ftp_url.origin = GURL("ftp://ftp.funet.fi/fc959.txt");
  password_store->AddLogin(ftp_url);

  scoped_task_environment.RunUntilIdle();
  TestingPrefServiceSimple prefs;
  // Prevent cleaning of duplicated blacklist entries.
  prefs.registry()->RegisterBooleanPref(
      prefs::kDuplicatedBlacklistedCredentialsRemoved, true);
  prefs.registry()->RegisterBooleanPref(
      prefs::kCredentialsWithWrongSignonRealmRemoved, false);

  MockCredentialsCleanerObserver observer;
  InvalidRealmCredentialCleaner cleaner(password_store, &prefs);
  EXPECT_CALL(observer, CleaningCompleted);
  cleaner.StartCleaning(&observer);
  scoped_task_environment.RunUntilIdle();

  // Check that credentials were not deleted.
  ASSERT_TRUE(StoreContains(password_store.get(), file_url));
  ASSERT_TRUE(StoreContains(password_store.get(), ftp_url));

  password_store->ShutdownOnUIThread();
  scoped_task_environment.RunUntilIdle();
}

}  // namespace password_manager