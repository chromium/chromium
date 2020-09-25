// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/login_database.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/os_crypt/os_crypt.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using autofill::GaiaIdHash;
using autofill::ValueElementPair;
using autofill::ValueElementVector;
using base::ASCIIToUTF16;
using base::UTF16ToASCII;
using ::testing::Eq;
using ::testing::Ne;
using ::testing::Pointee;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

namespace password_manager {
namespace {

PasswordStoreChangeList AddChangeForForm(const PasswordForm& form) {
  return PasswordStoreChangeList(
      1, PasswordStoreChange(PasswordStoreChange::ADD, form));
}

PasswordStoreChangeList UpdateChangeForForm(const PasswordForm& form,
                                            const bool password_changed) {
  return PasswordStoreChangeList(
      1,
      PasswordStoreChange(PasswordStoreChange::UPDATE, form, password_changed));
}

PasswordStoreChangeList RemoveChangeForForm(const PasswordForm& form) {
  return PasswordStoreChangeList(
      1, PasswordStoreChange(PasswordStoreChange::REMOVE, form));
}

void GenerateExamplePasswordForm(PasswordForm* form) {
  form->url = GURL("http://accounts.google.com/LoginAuth");
  form->action = GURL("http://accounts.google.com/Login");
  form->username_element = ASCIIToUTF16("Email");
  form->username_value = ASCIIToUTF16("test@gmail.com");
  form->password_element = ASCIIToUTF16("Passwd");
  form->password_value = ASCIIToUTF16("test");
  form->submit_element = ASCIIToUTF16("signIn");
  form->signon_realm = "http://www.google.com/";
  form->scheme = PasswordForm::Scheme::kHtml;
  form->times_used = 1;
  form->form_data.name = ASCIIToUTF16("form_name");
  form->date_synced = base::Time::Now();
  form->date_last_used = base::Time::Now();
  form->display_name = ASCIIToUTF16("Mr. Smith");
  form->icon_url = GURL("https://accounts.google.com/Icon");
  form->federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));
  form->skip_zero_click = true;
  form->in_store = PasswordForm::Store::kProfileStore;
  form->moving_blocked_for_list.push_back(GaiaIdHash::FromGaiaId("user1"));
  form->moving_blocked_for_list.push_back(GaiaIdHash::FromGaiaId("user2"));
}

// Helper functions to read the value of the first column of an executed
// statement if we know its type. You must implement a specialization for
// every column type you use.
template <class T>
struct must_be_specialized {
  static const bool is_specialized = false;
};

template <class T>
T GetFirstColumn(const sql::Statement& s) {
  static_assert(must_be_specialized<T>::is_specialized,
                "Implement a specialization.");
}

template <>
int64_t GetFirstColumn(const sql::Statement& s) {
  return s.ColumnInt64(0);
}

template <>
std::string GetFirstColumn(const sql::Statement& s) {
  return s.ColumnString(0);
}

// Returns an empty vector on failure. Otherwise returns values in the column
// |column_name| of the logins table. The order of the
// returned rows is well-defined.
template <class T>
std::vector<T> GetColumnValuesFromDatabase(const base::FilePath& database_path,
                                           const std::string& column_name) {
  sql::Database db;
  std::vector<T> results;
  CHECK(db.Open(database_path));

  std::string statement = base::StringPrintf(
      "SELECT %s FROM logins ORDER BY username_value, %s DESC",
      column_name.c_str(), column_name.c_str());
  sql::Statement s(db.GetCachedStatement(SQL_FROM_HERE, statement.c_str()));
  EXPECT_TRUE(s.is_valid());

  while (s.Step())
    results.push_back(GetFirstColumn<T>(s));

  return results;
}

bool AddZeroClickableLogin(LoginDatabase* db,
                           const std::string& unique_string,
                           const GURL& origin) {
  // Example password form.
  PasswordForm form;
  form.url = origin;
  form.username_element = ASCIIToUTF16(unique_string);
  form.username_value = ASCIIToUTF16(unique_string);
  form.password_element = ASCIIToUTF16(unique_string);
  form.submit_element = ASCIIToUTF16("signIn");
  form.signon_realm = form.url.spec();
  form.display_name = ASCIIToUTF16(unique_string);
  form.icon_url = origin;
  form.federation_origin = url::Origin::Create(origin);
  form.date_created = base::Time::Now();

  form.skip_zero_click = false;

  return db->AddLogin(form) == AddChangeForForm(form);
}

MATCHER(IsGoogle1Account, "") {
  return arg.url.spec() == "https://accounts.google.com/ServiceLogin" &&
         arg.action.spec() == "https://accounts.google.com/ServiceLoginAuth" &&
         arg.username_value == ASCIIToUTF16("theerikchen") &&
         arg.scheme == PasswordForm::Scheme::kHtml;
}

MATCHER(IsGoogle2Account, "") {
  return arg.url.spec() == "https://accounts.google.com/ServiceLogin" &&
         arg.action.spec() == "https://accounts.google.com/ServiceLoginAuth" &&
         arg.username_value == ASCIIToUTF16("theerikchen2") &&
         arg.scheme == PasswordForm::Scheme::kHtml;
}

MATCHER(IsBasicAuthAccount, "") {
  return arg.scheme == PasswordForm::Scheme::kBasic;
}

}  // namespace

// Serialization routines for vectors implemented in login_database.cc.
base::Pickle SerializeValueElementPairs(const ValueElementVector& vec);
ValueElementVector DeserializeValueElementPairs(const base::Pickle& pickle);
base::Pickle SerializeGaiaIdHashVector(const std::vector<GaiaIdHash>& hashes);
std::vector<GaiaIdHash> DeserializeGaiaIdHashVector(const base::Pickle& p);

class LoginDatabaseTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestMetadataStoreMacDatabase");
    OSCryptMocker::SetUp();

    db_ = std::make_unique<LoginDatabase>(file_, IsAccountStore(false));
    ASSERT_TRUE(db_->Init());
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  LoginDatabase& db() { return *db_; }

  void TestNonHTMLFormPSLMatching(const PasswordForm::Scheme& scheme) {
    std::vector<std::unique_ptr<PasswordForm>> result;

    base::Time now = base::Time::Now();

    // Simple non-html auth form.
    PasswordForm non_html_auth;
    non_html_auth.url = GURL("http://example.com");
    non_html_auth.username_value = ASCIIToUTF16("test@gmail.com");
    non_html_auth.password_value = ASCIIToUTF16("test");
    non_html_auth.signon_realm = "http://example.com/Realm";
    non_html_auth.scheme = scheme;
    non_html_auth.date_created = now;

    // Simple password form.
    PasswordForm html_form(non_html_auth);
    html_form.action = GURL("http://example.com/login");
    html_form.username_element = ASCIIToUTF16("username");
    html_form.username_value = ASCIIToUTF16("test2@gmail.com");
    html_form.password_element = ASCIIToUTF16("password");
    html_form.submit_element = ASCIIToUTF16("");
    html_form.signon_realm = "http://example.com/";
    html_form.scheme = PasswordForm::Scheme::kHtml;
    html_form.date_created = now;

    // Add them and make sure they are there.
    EXPECT_EQ(AddChangeForForm(non_html_auth), db().AddLogin(non_html_auth));
    EXPECT_EQ(AddChangeForForm(html_form), db().AddLogin(html_form));
    EXPECT_TRUE(db().GetAutofillableLogins(&result));
    EXPECT_EQ(2U, result.size());
    result.clear();

    PasswordStore::FormDigest second_non_html_auth = {
        scheme, "http://second.example.com/Realm",
        GURL("http://second.example.com")};

    // This shouldn't match anything.
    EXPECT_TRUE(db().GetLogins(second_non_html_auth, &result));
    EXPECT_EQ(0U, result.size());

    // non-html auth still matches against itself.
    EXPECT_TRUE(
        db().GetLogins(PasswordStore::FormDigest(non_html_auth), &result));
    ASSERT_EQ(1U, result.size());
    EXPECT_EQ(result[0]->signon_realm, "http://example.com/Realm");

    // Clear state.
    db().RemoveLoginsCreatedBetween(now, base::Time(), /*changes=*/nullptr);
  }

  // Checks that a form of a given |scheme|, once stored, can be successfully
  // retrieved from the database.
  void TestRetrievingIPAddress(const PasswordForm::Scheme& scheme) {
    SCOPED_TRACE(testing::Message() << "scheme = " << scheme);
    std::vector<std::unique_ptr<PasswordForm>> result;

    base::Time now = base::Time::Now();
    std::string origin("http://56.7.8.90");

    PasswordForm ip_form;
    ip_form.url = GURL(origin);
    ip_form.username_value = ASCIIToUTF16("test@gmail.com");
    ip_form.password_value = ASCIIToUTF16("test");
    ip_form.signon_realm = origin;
    ip_form.scheme = scheme;
    ip_form.date_created = now;

    EXPECT_EQ(AddChangeForForm(ip_form), db().AddLogin(ip_form));
    EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(ip_form), &result));
    ASSERT_EQ(1U, result.size());
    EXPECT_EQ(result[0]->signon_realm, origin);

    // Clear state.
    db().RemoveLoginsCreatedBetween(now, base::Time(), /*changes=*/nullptr);
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath file_;
  std::unique_ptr<LoginDatabase> db_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(LoginDatabaseTest, Logins) {
  std::vector<std::unique_ptr<PasswordForm>> result;
  PrimaryKeyToFormMap key_to_form_map;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());
  EXPECT_TRUE(db().IsEmpty());

  EXPECT_EQ(db().GetAllLogins(&key_to_form_map), FormRetrievalResult::kSuccess);
  EXPECT_EQ(0U, key_to_form_map.size());

  // Example password form.
  PasswordForm form;
  GenerateExamplePasswordForm(&form);

  // Add it and make sure it is there and that all the fields were retrieved
  // correctly.
  PasswordStoreChangeList changes = db().AddLogin(form);
  ASSERT_EQ(AddChangeForForm(form), changes);
  EXPECT_EQ(1, changes[0].primary_key());
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form, *result[0]);
  EXPECT_FALSE(db().IsEmpty());
  result.clear();

  EXPECT_EQ(db().GetAllLogins(&key_to_form_map), FormRetrievalResult::kSuccess);
  EXPECT_EQ(1U, key_to_form_map.size());
  EXPECT_EQ(form, *key_to_form_map[1]);
  key_to_form_map.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form), &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form, *result[0]);
  result.clear();

  // The example site changes...
  PasswordForm form2(form);
  form2.url = GURL("http://www.google.com/new/accounts/LoginAuth");
  form2.submit_element = ASCIIToUTF16("reallySignIn");

  // Match against an inexact copy
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form2), &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // Uh oh, the site changed origin & action URLs all at once!
  PasswordForm form3(form2);
  form3.action = GURL("http://www.google.com/new/accounts/Login");

  // signon_realm is the same, should match.
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form3), &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // Imagine the site moves to a secure server for login.
  PasswordForm form4(form3);
  form4.signon_realm = "https://www.google.com/";

  // We have only an http record, so no match for this.
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form4), &result));
  EXPECT_EQ(0U, result.size());

  // Let's imagine the user logs into the secure site.
  changes = db().AddLogin(form4);
  ASSERT_EQ(AddChangeForForm(form4), changes);
  EXPECT_EQ(2, changes[0].primary_key());
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(2U, result.size());
  result.clear();

  // Now the match works
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form4), &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // The user chose to forget the original but not the new.
  EXPECT_TRUE(db().RemoveLogin(form, &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(1, changes[0].primary_key());
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // The old form wont match the new site (http vs https).
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form), &result));
  EXPECT_EQ(0U, result.size());

  // User changes their password.
  PasswordForm form5(form4);
  form5.password_value = ASCIIToUTF16("test6");
  const base::Time kNow = base::Time::Now();
  form5.date_last_used = kNow;

  // We update, and check to make sure it matches the
  // old form, and there is only one record.
  EXPECT_EQ(UpdateChangeForForm(form5, /*passwordchanged=*/true),
            db().UpdateLogin(form5));
  // matches
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form5), &result));
  EXPECT_EQ(1U, result.size());
  result.clear();
  // Only one record.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  // Password element was updated.
  EXPECT_EQ(form5.password_value, result[0]->password_value);
  // Date last used.
  EXPECT_EQ(kNow, form5.date_last_used);
  result.clear();

  // Make sure everything can disappear.
  EXPECT_TRUE(db().RemoveLogin(form4, &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(2, changes[0].primary_key());
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());
  EXPECT_TRUE(db().IsEmpty());
}

TEST_F(LoginDatabaseTest, AddLoginReturnsPrimaryKey) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  GenerateExamplePasswordForm(&form);

  // Add it and make sure the primary key is returned in the
  // PasswordStoreChange.
  PasswordStoreChangeList change_list = db().AddLogin(form);
  ASSERT_EQ(1U, change_list.size());
  EXPECT_EQ(AddChangeForForm(form), change_list);
  EXPECT_EQ(1, change_list[0].primary_key());
}

TEST_F(LoginDatabaseTest, RemoveLoginsByPrimaryKey) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  GenerateExamplePasswordForm(&form);

  // Add it and make sure it is there and that all the fields were retrieved
  // correctly.
  PasswordStoreChangeList change_list = db().AddLogin(form);
  ASSERT_EQ(1U, change_list.size());
  int primary_key = change_list[0].primary_key();
  EXPECT_EQ(AddChangeForForm(form), change_list);
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form, *result[0]);
  result.clear();

  // RemoveLoginByPrimaryKey() doesn't decrypt or fill the password value.
  form.password_value = ASCIIToUTF16("");

  EXPECT_TRUE(db().RemoveLoginByPrimaryKey(primary_key, &change_list));
  EXPECT_EQ(RemoveChangeForForm(form), change_list);
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());
}

TEST_F(LoginDatabaseTest, ShouldNotRecyclePrimaryKeys) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Example password form.
  PasswordForm form;
  GenerateExamplePasswordForm(&form);

  // Add the form.
  PasswordStoreChangeList change_list = db().AddLogin(form);
  ASSERT_EQ(1U, change_list.size());
  int primary_key1 = change_list[0].primary_key();
  change_list.clear();
  // Delete the form
  EXPECT_TRUE(db().RemoveLoginByPrimaryKey(primary_key1, &change_list));
  ASSERT_EQ(1U, change_list.size());
  // Add it again.
  change_list = db().AddLogin(form);
  ASSERT_EQ(1U, change_list.size());
  EXPECT_NE(primary_key1, change_list[0].primary_key());
}

TEST_F(LoginDatabaseTest, TestPublicSuffixDomainMatching) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  form.url = GURL("https://foo.com/");
  form.action = GURL("https://foo.com/login");
  form.username_element = ASCIIToUTF16("username");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("password");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("");
  form.signon_realm = "https://foo.com/";
  form.scheme = PasswordForm::Scheme::kHtml;

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form), &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // We go to the mobile site.
  PasswordForm form2(form);
  form2.url = GURL("https://mobile.foo.com/");
  form2.action = GURL("https://mobile.foo.com/login");
  form2.signon_realm = "https://mobile.foo.com/";

  // Match against the mobile site.
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form2), &result));
  EXPECT_EQ(1U, result.size());
  EXPECT_EQ("https://foo.com/", result[0]->signon_realm);
  EXPECT_TRUE(result[0]->is_public_suffix_match);
}

TEST_F(LoginDatabaseTest, TestFederatedMatching) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Example password form.
  PasswordForm form;
  form.url = GURL("https://foo.com/");
  form.action = GURL("https://foo.com/login");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_value = ASCIIToUTF16("test");
  form.signon_realm = "https://foo.com/";
  form.scheme = PasswordForm::Scheme::kHtml;

  // We go to the mobile site.
  PasswordForm form2(form);
  form2.url = GURL("https://mobile.foo.com/");
  form2.action = GURL("https://mobile.foo.com/login");
  form2.signon_realm = "federation://mobile.foo.com/accounts.google.com";
  form2.username_value = ASCIIToUTF16("test1@gmail.com");
  form2.type = PasswordForm::Type::kApi;
  form2.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_EQ(AddChangeForForm(form2), db().AddLogin(form2));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(2U, result.size());

  // When we retrieve the forms from the store, |in_store| should be set.
  form.in_store = PasswordForm::Store::kProfileStore;
  form2.in_store = PasswordForm::Store::kProfileStore;

  // Match against desktop.
  PasswordStore::FormDigest form_request = {PasswordForm::Scheme::kHtml,
                                            "https://foo.com/",
                                            GURL("https://foo.com/")};
  EXPECT_TRUE(db().GetLogins(form_request, &result));
  // Both forms are matched, only form2 is a PSL match.
  form.is_public_suffix_match = false;
  form2.is_public_suffix_match = true;
  EXPECT_THAT(result, UnorderedElementsAre(Pointee(form), Pointee(form2)));

  // Match against the mobile site.
  form_request.url = GURL("https://mobile.foo.com/");
  form_request.signon_realm = "https://mobile.foo.com/";
  EXPECT_TRUE(db().GetLogins(form_request, &result));
  // Both forms are matched, only form is a PSL match.
  form.is_public_suffix_match = true;
  form2.is_public_suffix_match = false;
  EXPECT_THAT(result, UnorderedElementsAre(Pointee(form), Pointee(form2)));
}

TEST_F(LoginDatabaseTest, TestFederatedMatchingLocalhost) {
  PasswordForm form;
  form.url = GURL("http://localhost/");
  form.signon_realm = "federation://localhost/accounts.google.com";
  form.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.type = PasswordForm::Type::kApi;
  form.scheme = PasswordForm::Scheme::kHtml;

  PasswordForm form_with_port(form);
  form_with_port.url = GURL("http://localhost:8080/");
  form_with_port.signon_realm = "federation://localhost/accounts.google.com";

  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_EQ(AddChangeForForm(form_with_port), db().AddLogin(form_with_port));

  // When we retrieve the forms from the store, |in_store| should be set.
  form.in_store = PasswordForm::Store::kProfileStore;
  form_with_port.in_store = PasswordForm::Store::kProfileStore;

  // Match localhost with and without port.
  PasswordStore::FormDigest form_request(PasswordForm::Scheme::kHtml,
                                         "http://localhost/",
                                         GURL("http://localhost/"));
  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_TRUE(db().GetLogins(form_request, &result));
  EXPECT_THAT(result, UnorderedElementsAre(Pointee(form)));

  form_request.url = GURL("http://localhost:8080/");
  form_request.signon_realm = "http://localhost:8080/";
  EXPECT_TRUE(db().GetLogins(form_request, &result));
  EXPECT_THAT(result, UnorderedElementsAre(Pointee(form_with_port)));
}

TEST_F(LoginDatabaseTest, TestPublicSuffixDisabledForNonHTMLForms) {
  TestNonHTMLFormPSLMatching(PasswordForm::Scheme::kBasic);
  TestNonHTMLFormPSLMatching(PasswordForm::Scheme::kDigest);
  TestNonHTMLFormPSLMatching(PasswordForm::Scheme::kOther);
}

TEST_F(LoginDatabaseTest, TestIPAddressMatches_HTML) {
  TestRetrievingIPAddress(PasswordForm::Scheme::kHtml);
}

TEST_F(LoginDatabaseTest, TestIPAddressMatches_basic) {
  TestRetrievingIPAddress(PasswordForm::Scheme::kBasic);
}

TEST_F(LoginDatabaseTest, TestIPAddressMatches_digest) {
  TestRetrievingIPAddress(PasswordForm::Scheme::kDigest);
}

TEST_F(LoginDatabaseTest, TestIPAddressMatches_other) {
  TestRetrievingIPAddress(PasswordForm::Scheme::kOther);
}

TEST_F(LoginDatabaseTest, TestPublicSuffixDomainMatchingShouldMatchingApply) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Saved password form on Google sign-in page.
  PasswordForm form;
  form.url = GURL("https://accounts.google.com/");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_value = ASCIIToUTF16("test");
  form.signon_realm = "https://accounts.google.com/";
  form.scheme = PasswordForm::Scheme::kHtml;

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form), &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form.signon_realm, result[0]->signon_realm);
  result.clear();

  // Google change password should match to the saved sign-in form.
  PasswordStore::FormDigest form2 = {PasswordForm::Scheme::kHtml,
                                     "https://myaccount.google.com/",
                                     GURL("https://myaccount.google.com/")};

  EXPECT_TRUE(db().GetLogins(form2, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form.signon_realm, result[0]->signon_realm);
  EXPECT_TRUE(result[0]->is_public_suffix_match);

  // There should be no PSL match on other subdomains.
  PasswordStore::FormDigest form3 = {PasswordForm::Scheme::kHtml,
                                     "https://some.other.google.com/",
                                     GURL("https://some.other.google.com/")};

  EXPECT_TRUE(db().GetLogins(form3, &result));
  EXPECT_EQ(0U, result.size());
}

TEST_F(LoginDatabaseTest, TestFederatedMatchingWithoutPSLMatching) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Example password form.
  PasswordForm form;
  form.url = GURL("https://accounts.google.com/");
  form.action = GURL("https://accounts.google.com/login");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_value = ASCIIToUTF16("test");
  form.signon_realm = "https://accounts.google.com/";
  form.scheme = PasswordForm::Scheme::kHtml;

  // We go to a different site on the same domain where PSL is disabled.
  PasswordForm form2(form);
  form2.url = GURL("https://some.other.google.com/");
  form2.action = GURL("https://some.other.google.com/login");
  form2.signon_realm = "federation://some.other.google.com/accounts.google.com";
  form2.username_value = ASCIIToUTF16("test1@gmail.com");
  form2.type = PasswordForm::Type::kApi;
  form2.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_EQ(AddChangeForForm(form2), db().AddLogin(form2));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(2U, result.size());

  // When we retrieve the forms from the store, |in_store| should be set.
  form.in_store = PasswordForm::Store::kProfileStore;
  form2.in_store = PasswordForm::Store::kProfileStore;

  // Match against the first one.
  PasswordStore::FormDigest form_request = {PasswordForm::Scheme::kHtml,
                                            form.signon_realm, form.url};
  EXPECT_TRUE(db().GetLogins(form_request, &result));
  EXPECT_THAT(result, testing::ElementsAre(Pointee(form)));

  // Match against the second one.
  form_request.url = form2.url;
  form_request.signon_realm = form2.signon_realm;
  EXPECT_TRUE(db().GetLogins(form_request, &result));
  form.is_public_suffix_match = true;
  EXPECT_THAT(result, testing::ElementsAre(Pointee(form2)));
}

TEST_F(LoginDatabaseTest, TestFederatedPSLMatching) {
  // Save a federated credential for the PSL matched site.
  PasswordForm form;
  form.url = GURL("https://psl.example.com/");
  form.action = GURL("https://psl.example.com/login");
  form.signon_realm = "federation://psl.example.com/accounts.google.com";
  form.username_value = ASCIIToUTF16("test1@gmail.com");
  form.type = PasswordForm::Type::kApi;
  form.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));
  form.scheme = PasswordForm::Scheme::kHtml;
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  // When we retrieve the form from the store, it should have |in_store| set.
  form.in_store = PasswordForm::Store::kProfileStore;

  // Match against.
  PasswordStore::FormDigest form_request = {PasswordForm::Scheme::kHtml,
                                            "https://example.com/",
                                            GURL("https://example.com/login")};
  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_TRUE(db().GetLogins(form_request, &result));
  form.is_public_suffix_match = true;
  EXPECT_THAT(result, testing::ElementsAre(Pointee(form)));
}

// This test fails if the implementation of GetLogins uses GetCachedStatement
// instead of GetUniqueStatement, since REGEXP is in use. See
// http://crbug.com/248608.
TEST_F(LoginDatabaseTest, TestPublicSuffixDomainMatchingDifferentSites) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  form.url = GURL("https://foo.com/");
  form.action = GURL("https://foo.com/login");
  form.username_element = ASCIIToUTF16("username");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("password");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("");
  form.signon_realm = "https://foo.com/";
  form.scheme = PasswordForm::Scheme::kHtml;

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form), &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // We go to the mobile site.
  PasswordStore::FormDigest form2(form);
  form2.url = GURL("https://mobile.foo.com/");
  form2.signon_realm = "https://mobile.foo.com/";

  // Match against the mobile site.
  EXPECT_TRUE(db().GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  EXPECT_EQ("https://foo.com/", result[0]->signon_realm);
  EXPECT_TRUE(result[0]->is_public_suffix_match);
  result.clear();

  // Add baz.com desktop site.
  form.url = GURL("https://baz.com/login/");
  form.action = GURL("https://baz.com/login/");
  form.username_element = ASCIIToUTF16("email");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("password");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("");
  form.signon_realm = "https://baz.com/";
  form.scheme = PasswordForm::Scheme::kHtml;

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(2U, result.size());
  result.clear();

  // We go to the mobile site of baz.com.
  PasswordStore::FormDigest form3(form);
  form3.url = GURL("https://m.baz.com/login/");
  form3.signon_realm = "https://m.baz.com/";

  // Match against the mobile site of baz.com.
  EXPECT_TRUE(db().GetLogins(form3, &result));
  EXPECT_EQ(1U, result.size());
  EXPECT_EQ("https://baz.com/", result[0]->signon_realm);
  EXPECT_TRUE(result[0]->is_public_suffix_match);
  result.clear();
}

PasswordForm GetFormWithNewSignonRealm(PasswordForm form,
                                       std::string signon_realm) {
  PasswordForm form2(form);
  form2.url = GURL(signon_realm);
  form2.action = GURL(signon_realm);
  form2.signon_realm = signon_realm;
  return form2;
}

TEST_F(LoginDatabaseTest, TestPublicSuffixDomainMatchingRegexp) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  form.url = GURL("http://foo.com/");
  form.action = GURL("http://foo.com/login");
  form.username_element = ASCIIToUTF16("username");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("password");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("");
  form.signon_realm = "http://foo.com/";
  form.scheme = PasswordForm::Scheme::kHtml;

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // Example password form that has - in the domain name.
  PasswordForm form_dash =
      GetFormWithNewSignonRealm(form, "http://www.foo-bar.com/");

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form_dash), db().AddLogin(form_dash));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(2U, result.size());
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form), &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // www.foo.com should match.
  PasswordForm form2 = GetFormWithNewSignonRealm(form, "http://www.foo.com/");
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form2), &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // a.b.foo.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://a.b.foo.com/");
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form2), &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // a-b.foo.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://a-b.foo.com/");
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form2), &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // foo-bar.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://foo-bar.com/");
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form2), &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // www.foo-bar.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://www.foo-bar.com/");
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form2), &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // a.b.foo-bar.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://a.b.foo-bar.com/");
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form2), &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // a-b.foo-bar.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://a-b.foo-bar.com/");
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form2), &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // foo.com with port 1337 should not match.
  form2 = GetFormWithNewSignonRealm(form, "http://foo.com:1337/");
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form2), &result));
  EXPECT_EQ(0U, result.size());

  // http://foo.com should not match since the scheme is wrong.
  form2 = GetFormWithNewSignonRealm(form, "https://foo.com/");
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form2), &result));
  EXPECT_EQ(0U, result.size());

  // notfoo.com should not match.
  form2 = GetFormWithNewSignonRealm(form, "http://notfoo.com/");
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form2), &result));
  EXPECT_EQ(0U, result.size());

  // baz.com should not match.
  form2 = GetFormWithNewSignonRealm(form, "http://baz.com/");
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form2), &result));
  EXPECT_EQ(0U, result.size());

  // foo-baz.com should not match.
  form2 = GetFormWithNewSignonRealm(form, "http://foo-baz.com/");
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form2), &result));
  EXPECT_EQ(0U, result.size());
}

static bool AddTimestampedLogin(LoginDatabase* db,
                                std::string url,
                                const std::string& unique_string,
                                const base::Time& time,
                                bool date_is_creation) {
  // Example password form.
  PasswordForm form;
  form.url = GURL(url + std::string("/LoginAuth"));
  form.username_element = ASCIIToUTF16(unique_string);
  form.username_value = ASCIIToUTF16(unique_string);
  form.password_element = ASCIIToUTF16(unique_string);
  form.submit_element = ASCIIToUTF16("signIn");
  form.signon_realm = url;
  form.display_name = ASCIIToUTF16(unique_string);
  form.icon_url = GURL("https://accounts.google.com/Icon");
  form.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));
  form.skip_zero_click = true;

  if (date_is_creation)
    form.date_created = time;
  else
    form.date_synced = time;
  return db->AddLogin(form) == AddChangeForForm(form);
}

TEST_F(LoginDatabaseTest, ClearPrivateData_SavedPasswords) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  base::Time now = base::Time::Now();
  base::TimeDelta one_day = base::TimeDelta::FromDays(1);
  base::Time back_30_days = now - base::TimeDelta::FromDays(30);
  base::Time back_31_days = now - base::TimeDelta::FromDays(31);

  // Create one with a 0 time.
  EXPECT_TRUE(
      AddTimestampedLogin(&db(), "http://1.com", "foo1", base::Time(), true));
  // Create one for now and +/- 1 day.
  EXPECT_TRUE(
      AddTimestampedLogin(&db(), "http://2.com", "foo2", now - one_day, true));
  EXPECT_TRUE(AddTimestampedLogin(&db(), "http://3.com", "foo3", now, true));
  EXPECT_TRUE(
      AddTimestampedLogin(&db(), "http://4.com", "foo4", now + one_day, true));
  // Create one with 31 days old.
  EXPECT_TRUE(
      AddTimestampedLogin(&db(), "http://5.com", "foo5", back_31_days, true));

  // Verify inserts worked.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(5U, result.size());
  result.clear();

  // Get everything from today's date and on.
  PrimaryKeyToFormMap key_to_form_map;
  EXPECT_TRUE(
      db().GetLoginsCreatedBetween(now, base::Time(), &key_to_form_map));
  EXPECT_EQ(2U, key_to_form_map.size());
  key_to_form_map.clear();

  // Get all logins created more than 30 days back.
  EXPECT_TRUE(db().GetLoginsCreatedBetween(base::Time(), back_30_days,
                                           &key_to_form_map));
  EXPECT_EQ(2U, key_to_form_map.size());
  key_to_form_map.clear();

  // Delete everything from today's date and on.
  PasswordStoreChangeList changes;
  db().RemoveLoginsCreatedBetween(now, base::Time(), &changes);
  ASSERT_EQ(2U, changes.size());
  // The 3rd and the 4th should have been deleted.
  EXPECT_EQ(3, changes[0].primary_key());
  EXPECT_EQ(4, changes[1].primary_key());

  // Should have deleted two logins.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(3U, result.size());
  result.clear();

  // Delete all logins created more than 30 days back.
  db().RemoveLoginsCreatedBetween(base::Time(), back_30_days, &changes);
  ASSERT_EQ(2U, changes.size());
  // The 1st and the 5th should have been deleted.
  EXPECT_EQ(1, changes[0].primary_key());
  EXPECT_EQ(5, changes[1].primary_key());

  // Should have deleted two logins.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // Delete with 0 date (should delete all).
  db().RemoveLoginsCreatedBetween(base::Time(), base::Time(), &changes);
  ASSERT_EQ(1U, changes.size());
  // The 2nd should have been deleted.
  EXPECT_EQ(2, changes[0].primary_key());

  // Verify nothing is left.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());
}

TEST_F(LoginDatabaseTest, GetAutoSignInLogins) {
  PrimaryKeyToFormMap key_to_form_map;

  GURL origin("https://example.com");
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo1", origin));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo2", origin));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo3", origin));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo4", origin));

  EXPECT_TRUE(db().GetAutoSignInLogins(&key_to_form_map));
  EXPECT_EQ(4U, key_to_form_map.size());
  for (const auto& pair : key_to_form_map)
    EXPECT_FALSE(pair.second->skip_zero_click);

  EXPECT_TRUE(db().DisableAutoSignInForOrigin(origin));
  EXPECT_TRUE(db().GetAutoSignInLogins(&key_to_form_map));
  EXPECT_EQ(0U, key_to_form_map.size());
}

TEST_F(LoginDatabaseTest, DisableAutoSignInForOrigin) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  GURL origin1("https://google.com");
  GURL origin2("https://chrome.com");
  GURL origin3("http://example.com");
  GURL origin4("http://localhost");
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo1", origin1));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo2", origin2));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo3", origin3));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo4", origin4));

  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  for (const auto& form : result)
    EXPECT_FALSE(form->skip_zero_click);

  EXPECT_TRUE(db().DisableAutoSignInForOrigin(origin1));
  EXPECT_TRUE(db().DisableAutoSignInForOrigin(origin3));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  for (const auto& form : result) {
    if (form->url == origin1 || form->url == origin3)
      EXPECT_TRUE(form->skip_zero_click);
    else
      EXPECT_FALSE(form->skip_zero_click);
  }
}

TEST_F(LoginDatabaseTest, BlacklistedLogins) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetBlacklistLogins(&result));
  ASSERT_EQ(0U, result.size());

  // Save a form as blacklisted.
  PasswordForm form;
  form.url = GURL("http://accounts.google.com/LoginAuth");
  form.action = GURL("http://accounts.google.com/Login");
  form.username_element = ASCIIToUTF16("Email");
  form.password_element = ASCIIToUTF16("Passwd");
  form.submit_element = ASCIIToUTF16("signIn");
  form.signon_realm = "http://www.google.com/";
  form.blocked_by_user = true;
  form.scheme = PasswordForm::Scheme::kHtml;
  form.date_synced = base::Time::Now();
  form.date_last_used = base::Time::Now();
  form.display_name = ASCIIToUTF16("Mr. Smith");
  form.icon_url = GURL("https://accounts.google.com/Icon");
  form.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));
  form.skip_zero_click = true;
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  // Get all non-blacklisted logins (should be none).
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(0U, result.size());

  // When we retrieve the form from the store, it should have |in_store| set.
  form.in_store = PasswordForm::Store::kProfileStore;

  // GetLogins should give the blacklisted result.
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form), &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form, *result[0]);
  result.clear();

  // So should GetBlacklistedLogins.
  EXPECT_TRUE(db().GetBlacklistLogins(&result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form, *result[0]);
  result.clear();
}

TEST_F(LoginDatabaseTest, VectorSerialization) {
  // Empty vector.
  ValueElementVector vec;
  base::Pickle temp = SerializeValueElementPairs(vec);
  ValueElementVector output = DeserializeValueElementPairs(temp);
  EXPECT_THAT(output, Eq(vec));

  // Normal data.
  vec.push_back({ASCIIToUTF16("first"), ASCIIToUTF16("id1")});
  vec.push_back({ASCIIToUTF16("second"), ASCIIToUTF16("id2")});
  vec.push_back({ASCIIToUTF16("third"), ASCIIToUTF16("id3")});

  temp = SerializeValueElementPairs(vec);
  output = DeserializeValueElementPairs(temp);
  EXPECT_THAT(output, Eq(vec));
}

TEST_F(LoginDatabaseTest, GaiaIdHashVectorSerialization) {
  // Empty vector.
  std::vector<GaiaIdHash> vec;
  base::Pickle temp = SerializeGaiaIdHashVector(vec);
  std::vector<GaiaIdHash> output = DeserializeGaiaIdHashVector(temp);
  EXPECT_THAT(output, Eq(vec));

  // Normal data.
  vec.push_back(GaiaIdHash::FromGaiaId("first"));
  vec.push_back(GaiaIdHash::FromGaiaId("second"));
  vec.push_back(GaiaIdHash::FromGaiaId("third"));

  temp = SerializeGaiaIdHashVector(vec);
  output = DeserializeGaiaIdHashVector(temp);
  EXPECT_THAT(output, Eq(vec));
}

TEST_F(LoginDatabaseTest, UpdateIncompleteCredentials) {
  std::vector<std::unique_ptr<PasswordForm>> result;
  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(0U, result.size());

  // Save an incomplete form. Note that it only has a few fields set, ex. it's
  // missing 'action', 'username_element' and 'password_element'. Such forms
  // are sometimes inserted during import from other browsers (which may not
  // store this info).
  PasswordForm incomplete_form;
  incomplete_form.url = GURL("http://accounts.google.com/LoginAuth");
  incomplete_form.signon_realm = "http://accounts.google.com/";
  incomplete_form.username_value = ASCIIToUTF16("my_username");
  incomplete_form.password_value = ASCIIToUTF16("my_password");
  incomplete_form.date_last_used = base::Time::Now();
  incomplete_form.blocked_by_user = false;
  incomplete_form.scheme = PasswordForm::Scheme::kHtml;
  EXPECT_EQ(AddChangeForForm(incomplete_form), db().AddLogin(incomplete_form));

  // A form on some website. It should trigger a match with the stored one.
  PasswordForm encountered_form;
  encountered_form.url = GURL("http://accounts.google.com/LoginAuth");
  encountered_form.signon_realm = "http://accounts.google.com/";
  encountered_form.action = GURL("http://accounts.google.com/Login");
  encountered_form.username_element = ASCIIToUTF16("Email");
  encountered_form.password_element = ASCIIToUTF16("Passwd");
  encountered_form.submit_element = ASCIIToUTF16("signIn");

  // Get matches for encountered_form.
  EXPECT_TRUE(
      db().GetLogins(PasswordStore::FormDigest(encountered_form), &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(incomplete_form.url, result[0]->url);
  EXPECT_EQ(incomplete_form.signon_realm, result[0]->signon_realm);
  EXPECT_EQ(incomplete_form.username_value, result[0]->username_value);
  EXPECT_EQ(incomplete_form.password_value, result[0]->password_value);
  EXPECT_EQ(incomplete_form.date_last_used, result[0]->date_last_used);

  // We should return empty 'action', 'username_element', 'password_element'
  // and 'submit_element' as we can't be sure if the credentials were entered
  // in this particular form on the page.
  EXPECT_EQ(GURL(), result[0]->action);
  EXPECT_TRUE(result[0]->username_element.empty());
  EXPECT_TRUE(result[0]->password_element.empty());
  EXPECT_TRUE(result[0]->submit_element.empty());
  result.clear();

  // Let's say this login form worked. Now update the stored credentials with
  // 'action', 'username_element', 'password_element' and 'submit_element' from
  // the encountered form.
  PasswordForm completed_form(incomplete_form);
  completed_form.action = encountered_form.action;
  completed_form.username_element = encountered_form.username_element;
  completed_form.password_element = encountered_form.password_element;
  completed_form.submit_element = encountered_form.submit_element;
  EXPECT_EQ(AddChangeForForm(completed_form), db().AddLogin(completed_form));
  EXPECT_TRUE(db().RemoveLogin(incomplete_form, /*changes=*/nullptr));

  // Get matches for encountered_form again.
  EXPECT_TRUE(
      db().GetLogins(PasswordStore::FormDigest(encountered_form), &result));
  ASSERT_EQ(1U, result.size());

  // This time we should have all the info available.
  PasswordForm expected_form(completed_form);
  // When we retrieve the form from the store, it should have |in_store| set.
  expected_form.in_store = PasswordForm::Store::kProfileStore;
  EXPECT_EQ(expected_form, *result[0]);
  result.clear();
}

TEST_F(LoginDatabaseTest, UpdateOverlappingCredentials) {
  // Save an incomplete form. Note that it only has a few fields set, ex. it's
  // missing 'action', 'username_element' and 'password_element'. Such forms
  // are sometimes inserted during import from other browsers (which may not
  // store this info).
  PasswordForm incomplete_form;
  incomplete_form.url = GURL("http://accounts.google.com/LoginAuth");
  incomplete_form.signon_realm = "http://accounts.google.com/";
  incomplete_form.username_value = ASCIIToUTF16("my_username");
  incomplete_form.password_value = ASCIIToUTF16("my_password");
  incomplete_form.date_last_used = base::Time::Now();
  incomplete_form.blocked_by_user = false;
  incomplete_form.scheme = PasswordForm::Scheme::kHtml;
  EXPECT_EQ(AddChangeForForm(incomplete_form), db().AddLogin(incomplete_form));

  // Save a complete version of the previous form. Both forms could exist if
  // the user created the complete version before importing the incomplete
  // version from a different browser.
  PasswordForm complete_form = incomplete_form;
  complete_form.action = GURL("http://accounts.google.com/Login");
  complete_form.username_element = ASCIIToUTF16("username_element");
  complete_form.password_element = ASCIIToUTF16("password_element");
  complete_form.submit_element = ASCIIToUTF16("submit");

  // An update fails because the primary key for |complete_form| is different.
  EXPECT_EQ(PasswordStoreChangeList(), db().UpdateLogin(complete_form));
  EXPECT_EQ(AddChangeForForm(complete_form), db().AddLogin(complete_form));

  // Make sure both passwords exist.
  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(2U, result.size());
  result.clear();

  // Simulate the user changing their password.
  complete_form.password_value = ASCIIToUTF16("new_password");
  complete_form.date_synced = base::Time::Now();
  EXPECT_EQ(UpdateChangeForForm(complete_form, /*passwordchanged=*/true),
            db().UpdateLogin(complete_form));

  // When we retrieve the forms from the store, |in_store| should be set.
  complete_form.in_store = PasswordForm::Store::kProfileStore;
  incomplete_form.in_store = PasswordForm::Store::kProfileStore;

  // Both still exist now.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(2U, result.size());

  if (result[0]->username_element.empty())
    std::swap(result[0], result[1]);
  EXPECT_EQ(complete_form, *result[0]);
  EXPECT_EQ(incomplete_form, *result[1]);
}

TEST_F(LoginDatabaseTest, DoubleAdd) {
  PasswordForm form;
  form.url = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = ASCIIToUTF16("my_username");
  form.password_value = ASCIIToUTF16("my_password");
  form.blocked_by_user = false;
  form.scheme = PasswordForm::Scheme::kHtml;
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  // Add almost the same form again.
  form.times_used++;
  PasswordStoreChangeList list;
  list.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form));
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
  EXPECT_EQ(list, db().AddLogin(form));
}

TEST_F(LoginDatabaseTest, AddWrongForm) {
  PasswordForm form;
  // |origin| shouldn't be empty.
  form.url = GURL();
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = ASCIIToUTF16("my_username");
  form.password_value = ASCIIToUTF16("my_password");
  form.blocked_by_user = false;
  form.scheme = PasswordForm::Scheme::kHtml;
  EXPECT_EQ(PasswordStoreChangeList(), db().AddLogin(form));

  // |signon_realm| shouldn't be empty.
  form.url = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm.clear();
  EXPECT_EQ(PasswordStoreChangeList(), db().AddLogin(form));
}

TEST_F(LoginDatabaseTest, UpdateLogin) {
  PasswordForm form;
  form.url = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = ASCIIToUTF16("my_username");
  form.password_value = ASCIIToUTF16("my_password");
  form.blocked_by_user = false;
  form.scheme = PasswordForm::Scheme::kHtml;
  form.date_last_used = base::Time::Now();
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  form.action = GURL("http://accounts.google.com/login");
  form.password_value = ASCIIToUTF16("my_new_password");
  form.all_possible_usernames.push_back(autofill::ValueElementPair(
      ASCIIToUTF16("my_new_username"), ASCIIToUTF16("new_username_id")));
  form.times_used = 20;
  form.submit_element = ASCIIToUTF16("submit_element");
  form.date_synced = base::Time::Now();
  form.date_created = base::Time::Now() - base::TimeDelta::FromDays(1);
  form.date_last_used = base::Time::Now() + base::TimeDelta::FromDays(1);
  form.blocked_by_user = true;
  form.scheme = PasswordForm::Scheme::kBasic;
  form.type = PasswordForm::Type::kGenerated;
  form.display_name = ASCIIToUTF16("Mr. Smith");
  form.icon_url = GURL("https://accounts.google.com/Icon");
  form.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));
  form.skip_zero_click = true;
  form.moving_blocked_for_list.push_back(GaiaIdHash::FromGaiaId("gaia_id"));

  PasswordStoreChangeList changes = db().UpdateLogin(form);
  EXPECT_EQ(UpdateChangeForForm(form, /*passwordchanged=*/true), changes);
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(1, changes[0].primary_key());

  // When we retrieve the form from the store, it should have |in_store| set.
  form.in_store = PasswordForm::Store::kProfileStore;

  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form), &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form, *result[0]);
}

TEST_F(LoginDatabaseTest, UpdateLoginWithoutPassword) {
  PasswordForm form;
  form.url = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = ASCIIToUTF16("my_username");
  form.password_value = ASCIIToUTF16("my_password");
  form.blocked_by_user = false;
  form.scheme = PasswordForm::Scheme::kHtml;
  form.date_last_used = base::Time::Now();
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  form.action = GURL("http://accounts.google.com/login");
  form.all_possible_usernames.push_back(autofill::ValueElementPair(
      ASCIIToUTF16("my_new_username"), ASCIIToUTF16("new_username_id")));
  form.times_used = 20;
  form.submit_element = ASCIIToUTF16("submit_element");
  form.date_synced = base::Time::Now();
  form.date_created = base::Time::Now() - base::TimeDelta::FromDays(1);
  form.date_last_used = base::Time::Now() + base::TimeDelta::FromDays(1);
  form.display_name = ASCIIToUTF16("Mr. Smith");
  form.icon_url = GURL("https://accounts.google.com/Icon");
  form.skip_zero_click = true;
  form.moving_blocked_for_list.push_back(GaiaIdHash::FromGaiaId("gaia_id"));

  PasswordStoreChangeList changes = db().UpdateLogin(form);
  EXPECT_EQ(UpdateChangeForForm(form, /*passwordchanged=*/false), changes);
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(1, changes[0].primary_key());

  // When we retrieve the form from the store, it should have |in_store| set.
  form.in_store = PasswordForm::Store::kProfileStore;

  std::vector<std::unique_ptr<PasswordForm>> result;
  ASSERT_TRUE(db().GetLogins(PasswordStore::FormDigest(form), &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form, *result[0]);
}

TEST_F(LoginDatabaseTest, RemoveWrongForm) {
  PasswordForm form;
  // |origin| shouldn't be empty.
  form.url = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = ASCIIToUTF16("my_username");
  form.password_value = ASCIIToUTF16("my_password");
  form.blocked_by_user = false;
  form.scheme = PasswordForm::Scheme::kHtml;
  // The form isn't in the database.
  EXPECT_FALSE(db().RemoveLogin(form, /*changes=*/nullptr));

  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_TRUE(db().RemoveLogin(form, /*changes=*/nullptr));
  EXPECT_FALSE(db().RemoveLogin(form, /*changes=*/nullptr));
}

namespace {

void AddMetricsTestData(LoginDatabase* db) {
  PasswordForm password_form;
  password_form.url = GURL("http://example.com");
  password_form.username_value = ASCIIToUTF16("test1@gmail.com");
  password_form.password_value = ASCIIToUTF16("test");
  password_form.signon_realm = "http://example.com/";
  password_form.times_used = 0;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.username_value = ASCIIToUTF16("test2@gmail.com");
  password_form.times_used = 1;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.url = GURL("http://second.example.com");
  password_form.signon_realm = "http://second.example.com";
  password_form.times_used = 3;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.username_value = ASCIIToUTF16("test3@gmail.com");
  password_form.type = PasswordForm::Type::kGenerated;
  password_form.times_used = 2;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.url = GURL("ftp://third.example.com/");
  password_form.signon_realm = "ftp://third.example.com/";
  password_form.times_used = 4;
  password_form.scheme = PasswordForm::Scheme::kOther;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.url = GURL("http://fourth.example.com/");
  password_form.signon_realm = "http://fourth.example.com/";
  password_form.type = PasswordForm::Type::kManual;
  password_form.username_value = ASCIIToUTF16("");
  password_form.times_used = 10;
  password_form.scheme = PasswordForm::Scheme::kHtml;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.url = GURL("https://fifth.example.com/");
  password_form.signon_realm = "https://fifth.example.com/";
  password_form.password_value = ASCIIToUTF16("");
  password_form.blocked_by_user = true;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.url = GURL("https://sixth.example.com/");
  password_form.signon_realm = "https://sixth.example.com/";
  password_form.username_value = ASCIIToUTF16("my_username");
  password_form.password_value = ASCIIToUTF16("my_password");
  password_form.blocked_by_user = false;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.url = GURL();
  password_form.signon_realm = "android://hash@com.example.android/";
  password_form.username_value = ASCIIToUTF16("JohnDoe");
  password_form.password_value = ASCIIToUTF16("my_password");
  password_form.blocked_by_user = false;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.username_value = ASCIIToUTF16("JaneDoe");
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.url = GURL("http://rsolomakhin.github.io/autofill/");
  password_form.signon_realm = "http://rsolomakhin.github.io/";
  password_form.blocked_by_user = true;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.url = GURL("https://rsolomakhin.github.io/autofill/");
  password_form.signon_realm = "https://rsolomakhin.github.io/";
  password_form.blocked_by_user = true;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.url = GURL("http://rsolomakhin.github.io/autofill/123");
  password_form.signon_realm = "http://rsolomakhin.github.io/";
  password_form.blocked_by_user = true;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.url = GURL("https://rsolomakhin.github.io/autofill/1234");
  password_form.signon_realm = "https://rsolomakhin.github.io/";
  password_form.blocked_by_user = true;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  StatisticsTable& stats_table = db->stats_table();
  InteractionsStats stats;
  stats.origin_domain = GURL("https://example.com");
  stats.username_value = base::ASCIIToUTF16("user1");
  stats.dismissal_count = 10;
  stats.update_time = base::Time::FromTimeT(1);
  EXPECT_TRUE(stats_table.AddRow(stats));
  stats.username_value = base::ASCIIToUTF16("user2");
  stats.dismissal_count = 1;
  EXPECT_TRUE(stats_table.AddRow(stats));
  stats.username_value = base::ASCIIToUTF16("user3");
  stats.dismissal_count = 10;
  EXPECT_TRUE(stats_table.AddRow(stats));
  stats.origin_domain = GURL("https://foo.com");
  stats.dismissal_count = 10;
  EXPECT_TRUE(stats_table.AddRow(stats));
}

}  // namespace

TEST_F(LoginDatabaseTest, ReportMetricsTest) {
  AddMetricsTestData(&db());

  // Note: We also create and populate an account DB here and instruct it to
  // report metrics, even though all the checks below only test the profile DB.
  // This is to make sure that the account DB doesn't write to any of the same
  // histograms.
  base::FilePath account_db_file =
      temp_dir_.GetPath().AppendASCII("TestAccountStoreDatabase");
  LoginDatabase account_db(account_db_file, IsAccountStore(true));
  ASSERT_TRUE(account_db.Init());
  AddMetricsTestData(&account_db);

  base::HistogramTester histogram_tester;
  db().ReportMetrics("", false, BulkCheckDone(false));
  account_db.ReportMetrics("", false, BulkCheckDone(false));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountsPerSiteHiRes.AutoGenerated."
      "WithoutCustomPassphrase",
      1, 2);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountsPerSiteHiRes.UserCreated."
      "WithoutCustomPassphrase",
      1, 3);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountsPerSiteHiRes.UserCreated."
      "WithoutCustomPassphrase",
      2, 2);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountsPerSiteHiRes.Overall.WithoutCustomPassphrase", 1,
      5);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountsPerSiteHiRes.Overall.WithoutCustomPassphrase", 2,
      2);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.ByType.AutoGenerated."
      "WithoutCustomPassphrase",
      2, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.ByType.UserCreated."
      "WithoutCustomPassphrase",
      7, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.ByType.Overall."
      "WithoutCustomPassphrase",
      9, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Android", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Ftp", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Http", 5, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Https", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Other", 0, 1);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.AutoGenerated.WithoutCustomPassphrase",
      2, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.AutoGenerated.WithoutCustomPassphrase",
      4, 1);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.UserCreated.WithoutCustomPassphrase",
      0, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.UserCreated.WithoutCustomPassphrase",
      1, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.UserCreated.WithoutCustomPassphrase",
      3, 1);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.Overall.WithoutCustomPassphrase", 0,
      1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.Overall.WithoutCustomPassphrase", 1,
      1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.Overall.WithoutCustomPassphrase", 2,
      1);
  // The bucket for 3 and 4 is the same. Thus we expect two samples here.
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.Overall.WithoutCustomPassphrase", 3,
      2);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.EmptyUsernames.CountInDatabase", 1, 1);
  histogram_tester.ExpectUniqueSample("PasswordManager.InaccessiblePasswords",
                                      0, 1);
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.BubbleSuppression.AccountsInStatisticsTable", 4, 1);
#endif  // !defined(OS_IOS) && !defined(OS_ANDROID)
}

// This test is mostly a copy of ReportMetricsTest, but covering the account
// store instead of the profile store. Some metrics are not recorded for the
// account store (e.g. BubbleSuppression ones) so these are missing here; all
// the metrics that *are* covered have ".AccountStore" in their names.
TEST_F(LoginDatabaseTest, ReportAccountStoreMetricsTest) {
  // Note: We also populate the profile DB here and instruct it to report
  // metrics, even though all the checks below only test the account DB. This is
  // to make sure that the profile DB doesn't write to any of the same
  // histograms.
  AddMetricsTestData(&db());

  base::FilePath account_db_file =
      temp_dir_.GetPath().AppendASCII("TestAccountStoreDatabase");
  LoginDatabase account_db(account_db_file, IsAccountStore(true));
  ASSERT_TRUE(account_db.Init());
  AddMetricsTestData(&account_db);

  base::HistogramTester histogram_tester;
  db().ReportMetrics("", false, BulkCheckDone(false));
  account_db.ReportMetrics("", false, BulkCheckDone(false));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.AccountsPerSiteHiRes.AutoGenerated."
      "WithoutCustomPassphrase",
      1, 2);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.AccountsPerSiteHiRes.UserCreated."
      "WithoutCustomPassphrase",
      1, 3);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.AccountsPerSiteHiRes.UserCreated."
      "WithoutCustomPassphrase",
      2, 2);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.AccountsPerSiteHiRes.Overall."
      "WithoutCustomPassphrase",
      1, 5);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.AccountsPerSiteHiRes.Overall."
      "WithoutCustomPassphrase",
      2, 2);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes.ByType.AutoGenerated."
      "WithoutCustomPassphrase",
      2, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes.ByType.UserCreated."
      "WithoutCustomPassphrase",
      7, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes.ByType.Overall."
      "WithoutCustomPassphrase",
      9, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes.WithScheme.Android", 2,
      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes.WithScheme.Ftp", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes.WithScheme.Http", 5, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes.WithScheme.Https", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes.WithScheme.Other", 0, 1);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed.AutoGenerated."
      "WithoutCustomPassphrase",
      2, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed.AutoGenerated."
      "WithoutCustomPassphrase",
      4, 1);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed.UserCreated."
      "WithoutCustomPassphrase",
      0, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed.UserCreated."
      "WithoutCustomPassphrase",
      1, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed.UserCreated."
      "WithoutCustomPassphrase",
      3, 1);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed.Overall."
      "WithoutCustomPassphrase",
      0, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed.Overall."
      "WithoutCustomPassphrase",
      1, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed.Overall."
      "WithoutCustomPassphrase",
      2, 1);
  // The bucket for 3 and 4 is the same. Thus we expect two samples here.
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed.Overall."
      "WithoutCustomPassphrase",
      3, 2);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.EmptyUsernames.CountInDatabase", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.InaccessiblePasswords", 0, 1);
}

TEST_F(LoginDatabaseTest, DuplicatesMetrics_NoDuplicates) {
  // No duplicate.
  PasswordForm password_form;
  password_form.signon_realm = "http://example1.com/";
  password_form.url = GURL("http://example1.com/");
  password_form.username_element = ASCIIToUTF16("userelem_1");
  password_form.username_value = ASCIIToUTF16("username_1");
  password_form.password_value = ASCIIToUTF16("password_1");
  ASSERT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  // Different username -> no duplicate.
  password_form.signon_realm = "http://example2.com/";
  password_form.url = GURL("http://example2.com/");
  password_form.username_value = ASCIIToUTF16("username_1");
  ASSERT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));
  password_form.username_value = ASCIIToUTF16("username_2");
  ASSERT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  // Blacklisted forms don't count as duplicates (neither against other
  // blacklisted forms nor against actual saved credentials).
  password_form.signon_realm = "http://example3.com/";
  password_form.url = GURL("http://example3.com/");
  password_form.username_value = ASCIIToUTF16("username_1");
  ASSERT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));
  password_form.blocked_by_user = true;
  password_form.username_value = ASCIIToUTF16("username_2");
  ASSERT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));
  password_form.username_value = ASCIIToUTF16("username_3");
  ASSERT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  base::HistogramTester histogram_tester;
  db().ReportMetrics("", false, BulkCheckDone(false));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "PasswordManager.CredentialsWithDuplicates"),
              testing::ElementsAre(base::Bucket(0, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "PasswordManager.CredentialsWithMismatchedDuplicates"),
              testing::ElementsAre(base::Bucket(0, 1)));
}

TEST_F(LoginDatabaseTest, DuplicatesMetrics_ExactDuplicates) {
  // Add some PasswordForms that are "exact" duplicates (only the
  // username_element is different, which doesn't matter).
  PasswordForm password_form;
  password_form.signon_realm = "http://example1.com/";
  password_form.url = GURL("http://example1.com/");
  password_form.username_element = ASCIIToUTF16("userelem_1");
  password_form.username_value = ASCIIToUTF16("username_1");
  ASSERT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));
  password_form.username_element = ASCIIToUTF16("userelem_2");
  ASSERT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));
  // The number of "identical" credentials doesn't matter; we count the *sets*
  // of duplicates.
  password_form.username_element = ASCIIToUTF16("userelem_3");
  ASSERT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  // Similarly, origin doesn't make forms "different" either.
  password_form.signon_realm = "http://example2.com/";
  password_form.url = GURL("http://example2.com/path1");
  ASSERT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));
  password_form.url = GURL("http://example2.com/path2");
  ASSERT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  base::HistogramTester histogram_tester;
  db().ReportMetrics("", false, BulkCheckDone(false));

  // There should be 2 groups of "exact" duplicates.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "PasswordManager.CredentialsWithDuplicates"),
              testing::ElementsAre(base::Bucket(2, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "PasswordManager.CredentialsWithMismatchedDuplicates"),
              testing::ElementsAre(base::Bucket(0, 1)));
}

TEST_F(LoginDatabaseTest, DuplicatesMetrics_MismatchedDuplicates) {
  // Mismatched duplicates: Identical except for the password.
  PasswordForm password_form;
  password_form.signon_realm = "http://example1.com/";
  password_form.url = GURL("http://example1.com/");
  password_form.username_element = ASCIIToUTF16("userelem_1");
  password_form.username_value = ASCIIToUTF16("username_1");
  password_form.password_element = ASCIIToUTF16("passelem_1");
  password_form.password_value = ASCIIToUTF16("password_1");
  ASSERT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));
  // Note: password_value is not part of the unique key, so we need to change
  // some other value to be able to insert the duplicate into the DB.
  password_form.password_element = ASCIIToUTF16("passelem_2");
  password_form.password_value = ASCIIToUTF16("password_2");
  ASSERT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));
  // The number of "identical" credentials doesn't matter; we count the *sets*
  // of duplicates.
  password_form.password_element = ASCIIToUTF16("passelem_3");
  password_form.password_value = ASCIIToUTF16("password_3");
  ASSERT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  base::HistogramTester histogram_tester;
  db().ReportMetrics("", false, BulkCheckDone(false));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "PasswordManager.CredentialsWithDuplicates"),
              testing::ElementsAre(base::Bucket(0, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "PasswordManager.CredentialsWithMismatchedDuplicates"),
              testing::ElementsAre(base::Bucket(1, 1)));
}

TEST_F(LoginDatabaseTest, NoMetadata) {
  std::unique_ptr<syncer::MetadataBatch> metadata_batch =
      db().GetAllSyncMetadata();
  ASSERT_THAT(metadata_batch, testing::NotNull());
  EXPECT_EQ(0u, metadata_batch->TakeAllMetadata().size());
  EXPECT_EQ(sync_pb::ModelTypeState().SerializeAsString(),
            metadata_batch->GetModelTypeState().SerializeAsString());
}

TEST_F(LoginDatabaseTest, GetAllSyncMetadata) {
  sync_pb::EntityMetadata metadata;
  // Storage keys must be integers.
  const std::string kStorageKey1 = "1";
  const std::string kStorageKey2 = "2";
  metadata.set_sequence_number(1);

  EXPECT_TRUE(
      db().UpdateSyncMetadata(syncer::PASSWORDS, kStorageKey1, metadata));

  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_initial_sync_done(true);

  EXPECT_TRUE(db().UpdateModelTypeState(syncer::PASSWORDS, model_type_state));

  metadata.set_sequence_number(2);
  EXPECT_TRUE(
      db().UpdateSyncMetadata(syncer::PASSWORDS, kStorageKey2, metadata));

  std::unique_ptr<syncer::MetadataBatch> metadata_batch =
      db().GetAllSyncMetadata();
  ASSERT_THAT(metadata_batch, testing::NotNull());

  EXPECT_TRUE(metadata_batch->GetModelTypeState().initial_sync_done());

  syncer::EntityMetadataMap metadata_records =
      metadata_batch->TakeAllMetadata();

  EXPECT_EQ(metadata_records.size(), 2u);
  EXPECT_EQ(metadata_records[kStorageKey1]->sequence_number(), 1);
  EXPECT_EQ(metadata_records[kStorageKey2]->sequence_number(), 2);

  // Now check that a model type state update replaces the old value
  model_type_state.set_initial_sync_done(false);
  EXPECT_TRUE(db().UpdateModelTypeState(syncer::PASSWORDS, model_type_state));

  metadata_batch = db().GetAllSyncMetadata();
  ASSERT_THAT(metadata_batch, testing::NotNull());
  EXPECT_FALSE(metadata_batch->GetModelTypeState().initial_sync_done());
}

TEST_F(LoginDatabaseTest, DeleteAllSyncMetadata) {
  sync_pb::EntityMetadata metadata;
  // Storage keys must be integers.
  const std::string kStorageKey1 = "1";
  const std::string kStorageKey2 = "2";
  metadata.set_sequence_number(1);

  EXPECT_TRUE(
      db().UpdateSyncMetadata(syncer::PASSWORDS, kStorageKey1, metadata));

  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_initial_sync_done(true);

  EXPECT_TRUE(db().UpdateModelTypeState(syncer::PASSWORDS, model_type_state));

  metadata.set_sequence_number(2);
  EXPECT_TRUE(
      db().UpdateSyncMetadata(syncer::PASSWORDS, kStorageKey2, metadata));

  std::unique_ptr<syncer::MetadataBatch> metadata_batch =
      db().GetAllSyncMetadata();
  ASSERT_THAT(metadata_batch, testing::NotNull());
  ASSERT_EQ(metadata_batch->TakeAllMetadata().size(), 2u);

  db().DeleteAllSyncMetadata();

  std::unique_ptr<syncer::MetadataBatch> empty_metadata_batch =
      db().GetAllSyncMetadata();
  ASSERT_THAT(empty_metadata_batch, testing::NotNull());
  EXPECT_EQ(empty_metadata_batch->TakeAllMetadata().size(), 0u);
}

TEST_F(LoginDatabaseTest, WriteThenDeleteSyncMetadata) {
  sync_pb::EntityMetadata metadata;
  const std::string kStorageKey = "1";
  sync_pb::ModelTypeState model_type_state;

  model_type_state.set_initial_sync_done(true);

  metadata.set_client_tag_hash("client_hash");

  // Write the data into the store.
  EXPECT_TRUE(
      db().UpdateSyncMetadata(syncer::PASSWORDS, kStorageKey, metadata));
  EXPECT_TRUE(db().UpdateModelTypeState(syncer::PASSWORDS, model_type_state));
  // Delete the data we just wrote.
  EXPECT_TRUE(db().ClearSyncMetadata(syncer::PASSWORDS, kStorageKey));

  std::unique_ptr<syncer::MetadataBatch> metadata_batch =
      db().GetAllSyncMetadata();
  ASSERT_THAT(metadata_batch, testing::NotNull());

  // It shouldn't be there any more.
  syncer::EntityMetadataMap metadata_records =
      metadata_batch->TakeAllMetadata();
  EXPECT_EQ(metadata_records.size(), 0u);

  // Now delete the model type state.
  EXPECT_TRUE(db().ClearModelTypeState(syncer::PASSWORDS));
  metadata_batch = db().GetAllSyncMetadata();
  ASSERT_THAT(metadata_batch, testing::NotNull());

  EXPECT_EQ(sync_pb::ModelTypeState().SerializeAsString(),
            metadata_batch->GetModelTypeState().SerializeAsString());
}

#if defined(OS_POSIX)
// Only the current user has permission to read the database.
//
// Only POSIX because GetPosixFilePermissions() only exists on POSIX.
// This tests that sql::Database::set_restrict_to_user() was called,
// and that function is a noop on non-POSIX platforms in any case.
TEST_F(LoginDatabaseTest, FilePermissions) {
  int mode = base::FILE_PERMISSION_MASK;
  EXPECT_TRUE(base::GetPosixFilePermissions(file_, &mode));
  EXPECT_EQ((mode & base::FILE_PERMISSION_USER_MASK), mode);
}
#endif  // defined(OS_POSIX)

#if !defined(OS_IOS)
// Test that LoginDatabase encrypts the password values that it stores.
TEST_F(LoginDatabaseTest, EncryptionEnabled) {
  PasswordForm password_form;
  GenerateExamplePasswordForm(&password_form);
  base::FilePath file = temp_dir_.GetPath().AppendASCII("TestUnencryptedDB");
  {
    LoginDatabase db(file, IsAccountStore(false));
    ASSERT_TRUE(db.Init());
    EXPECT_EQ(AddChangeForForm(password_form), db.AddLogin(password_form));
  }
  base::string16 decrypted_pw;
  ASSERT_TRUE(OSCrypt::DecryptString16(
      GetColumnValuesFromDatabase<std::string>(file, "password_value").at(0),
      &decrypted_pw));
  EXPECT_EQ(decrypted_pw, password_form.password_value);
}
#endif  // !defined(OS_IOS)

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
// Test that LoginDatabase does not encrypt values when encryption is disabled.
// TODO(crbug.com/829857) This is supported only for Linux, while transitioning
// into LoginDB with full encryption.
TEST_F(LoginDatabaseTest, EncryptionDisabled) {
  PasswordForm password_form;
  GenerateExamplePasswordForm(&password_form);
  base::FilePath file = temp_dir_.GetPath().AppendASCII("TestUnencryptedDB");
  {
    LoginDatabase db(file, IsAccountStore(false));
    db.disable_encryption();
    ASSERT_TRUE(db.Init());
    EXPECT_EQ(AddChangeForForm(password_form), db.AddLogin(password_form));
  }
  EXPECT_EQ(
      GetColumnValuesFromDatabase<std::string>(file, "password_value").at(0),
      base::UTF16ToUTF8(password_form.password_value));
}
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
// On Android and ChromeOS there is a mix of plain-text and obfuscated
// passwords. Verify that they can both be accessed. Obfuscated passwords start
// with "v10". Some password values also start with "v10". Test that both are
// accessible (this doesn't work for any plain-text value).
TEST_F(LoginDatabaseTest, HandleObfuscationMix) {
  const char k_obfuscated_pw[] = "v10pass1";
  const char k_plain_text_pw1[] = "v10pass2";
  const char k_plain_text_pw2[] = "v11pass3";

  base::FilePath file = temp_dir_.GetPath().AppendASCII("TestUnencryptedDB");
  {
    LoginDatabase db(file, IsAccountStore(false));
    ASSERT_TRUE(db.Init());
    // Add obfuscated (new) entries.
    PasswordForm password_form;
    GenerateExamplePasswordForm(&password_form);
    password_form.password_value = ASCIIToUTF16(k_obfuscated_pw);
    EXPECT_EQ(AddChangeForForm(password_form), db.AddLogin(password_form));
    // Add plain-text (old) entries.
    db.disable_encryption();
    GenerateExamplePasswordForm(&password_form);
    password_form.username_value = ASCIIToUTF16("other_username");
    password_form.password_value = ASCIIToUTF16(k_plain_text_pw1);
    EXPECT_EQ(AddChangeForForm(password_form), db.AddLogin(password_form));
    GenerateExamplePasswordForm(&password_form);
    password_form.username_value = ASCIIToUTF16("other_username2");
    password_form.password_value = ASCIIToUTF16(k_plain_text_pw2);
    EXPECT_EQ(AddChangeForForm(password_form), db.AddLogin(password_form));
  }

  std::vector<std::unique_ptr<PasswordForm>> forms;
  {
    LoginDatabase db(file, IsAccountStore(false));
    ASSERT_TRUE(db.Init());
    ASSERT_TRUE(db.GetAutofillableLogins(&forms));
  }

  // On disk, unobfuscated passwords are as-is, while obfuscated passwords have
  // been changed (obfuscated).
  EXPECT_THAT(GetColumnValuesFromDatabase<std::string>(file, "password_value"),
              UnorderedElementsAre(Ne(k_obfuscated_pw), k_plain_text_pw1,
                                   k_plain_text_pw2));
  // LoginDatabase serves the original values.
  ASSERT_THAT(forms, SizeIs(3));
  EXPECT_EQ(k_obfuscated_pw, UTF16ToASCII(forms[0]->password_value));
  EXPECT_EQ(k_plain_text_pw1, UTF16ToASCII(forms[1]->password_value));
  EXPECT_EQ(k_plain_text_pw2, UTF16ToASCII(forms[2]->password_value));
}
#endif  // defined(OS_ANDROID) || defined(OS_CHROMEOS)

// If the database initialisation fails, the initialisation transaction should
// roll back without crashing.
TEST(LoginDatabaseFailureTest, Init_NoCrashOnFailedRollback) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath database_path = temp_dir.GetPath().AppendASCII("test.db");

  // To cause an init failure, set the compatible version to be higher than the
  // current version (in reality, this could happen if, e.g., someone opened a
  // Canary-created profile with Chrome Stable.
  {
    sql::Database connection;
    sql::MetaTable meta_table;
    ASSERT_TRUE(connection.Open(database_path));
    ASSERT_TRUE(meta_table.Init(&connection, kCurrentVersionNumber + 1,
                                kCurrentVersionNumber + 1));
  }

  // Now try to init the database with the file. The test succeeds if it does
  // not crash.
  LoginDatabase db(database_path, IsAccountStore(false));
  EXPECT_FALSE(db.Init());
}

// If the database version is from the future, it shouldn't be downgraded.
TEST(LoginDatabaseFutureLoginDatabase, ShouldNotDowngradeDatabaseVersion) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath database_path = temp_dir.GetPath().AppendASCII("test.db");

  const int kDBFutureVersion = kCurrentVersionNumber + 1000;

  {
    // Open a database with the current version.
    LoginDatabase db(database_path, IsAccountStore(false));
    EXPECT_TRUE(db.Init());
  }
  {
    // Overwrite the current version to be |kDBFutureVersion|
    sql::Database connection;
    sql::MetaTable meta_table;
    ASSERT_TRUE(connection.Open(database_path));
    // Set the DB version to be coming from the future.
    ASSERT_TRUE(meta_table.Init(&connection, kDBFutureVersion,
                                kCompatibleVersionNumber));
    meta_table.SetVersionNumber(kDBFutureVersion);
  }
  {
    // Open the database again.
    LoginDatabase db(database_path, IsAccountStore(false));
    EXPECT_TRUE(db.Init());
  }
  {
    // The DB version should remain the same.
    sql::Database connection;
    sql::MetaTable meta_table;
    ASSERT_TRUE(connection.Open(database_path));
    ASSERT_TRUE(meta_table.Init(&connection, kDBFutureVersion,
                                kCompatibleVersionNumber));
    EXPECT_EQ(kDBFutureVersion, meta_table.GetVersionNumber());
  }
}

// Test the migration from GetParam() version to kCurrentVersionNumber.
class LoginDatabaseMigrationTest : public testing::TestWithParam<int> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    database_dump_location_ = database_dump_location_.AppendASCII("components")
                                  .AppendASCII("test")
                                  .AppendASCII("data")
                                  .AppendASCII("password_manager");
    database_path_ = temp_dir_.GetPath().AppendASCII("test.db");
    OSCryptMocker::SetUp();
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  // Creates the database from |sql_file|.
  void CreateDatabase(base::StringPiece sql_file) {
    base::FilePath database_dump;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &database_dump));
    database_dump =
        database_dump.Append(database_dump_location_).AppendASCII(sql_file);
    ASSERT_TRUE(
        sql::test::CreateDatabaseFromSQL(database_path_, database_dump));
  }

  void DestroyDatabase() {
    if (!database_path_.empty())
      sql::Database::Delete(database_path_);
  }

  // Returns the database version for the test.
  int version() const { return GetParam(); }

  // Actual test body.
  void MigrationToVCurrent(base::StringPiece sql_file);

  base::FilePath database_path_;

 private:
  base::FilePath database_dump_location_;
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
};

void LoginDatabaseMigrationTest::MigrationToVCurrent(
    base::StringPiece sql_file) {
  SCOPED_TRACE(testing::Message("Version file = ") << sql_file);
  CreateDatabase(sql_file);
  // Original date, in seconds since UTC epoch.
  std::vector<int64_t> date_created(
      GetColumnValuesFromDatabase<int64_t>(database_path_, "date_created"));
  if (version() == 10)  // Version 10 has a duplicate entry.
    ASSERT_EQ(4U, date_created.size());
  else
    ASSERT_EQ(3U, date_created.size());
  // Migration to version 8 performs changes dates to the new format.
  // So for versions less of equal to 8 create date should be in old
  // format before migration and in new format after.
  if (version() <= 8) {
    ASSERT_EQ(1402955745, date_created[0]);
    ASSERT_EQ(1402950000, date_created[1]);
    ASSERT_EQ(1402950000, date_created[2]);
  } else {
    ASSERT_EQ(13047429345000000, date_created[0]);
    ASSERT_EQ(13047423600000000, date_created[1]);
    ASSERT_EQ(13047423600000000, date_created[2]);
  }

  {
    // Assert that the database was successfully opened and updated
    // to current version.
    LoginDatabase db(database_path_, IsAccountStore(false));
    ASSERT_TRUE(db.Init());

    // Check that the contents was preserved.
    std::vector<std::unique_ptr<PasswordForm>> result;
    EXPECT_TRUE(db.GetAutofillableLogins(&result));
    EXPECT_THAT(result, UnorderedElementsAre(Pointee(IsGoogle1Account()),
                                             Pointee(IsGoogle2Account()),
                                             Pointee(IsBasicAuthAccount())));

    // Verifies that the final version can save all the appropriate fields.
    PasswordForm form;
    GenerateExamplePasswordForm(&form);
    // Add the same form twice to test the constraints in the database.
    EXPECT_EQ(AddChangeForForm(form), db.AddLogin(form));
    PasswordStoreChangeList list;
    list.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form));
    list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
    EXPECT_EQ(list, db.AddLogin(form));

    result.clear();
    EXPECT_TRUE(db.GetLogins(PasswordStore::FormDigest(form), &result));
    ASSERT_EQ(1U, result.size());
    EXPECT_EQ(form, *result[0]);
    EXPECT_TRUE(db.RemoveLogin(form, /*changes=*/nullptr));
  }
  // New date, in microseconds since platform independent epoch.
  std::vector<int64_t> new_date_created(
      GetColumnValuesFromDatabase<int64_t>(database_path_, "date_created"));
  ASSERT_EQ(3U, new_date_created.size());
  if (version() <= 8) {
    // Check that the two dates match up.
    for (size_t i = 0; i < date_created.size(); ++i) {
      EXPECT_EQ(base::Time::FromInternalValue(new_date_created[i]),
                base::Time::FromTimeT(date_created[i]));
    }
  } else {
    ASSERT_EQ(13047429345000000, new_date_created[0]);
    ASSERT_EQ(13047423600000000, new_date_created[1]);
    ASSERT_EQ(13047423600000000, new_date_created[2]);
  }

  if (version() >= 7 && version() <= 13) {
    // The "avatar_url" column first appeared in version 7. In version 14,
    // it was renamed to "icon_url". Migration from a version <= 13
    // to >= 14 should not break theses URLs.
    std::vector<std::string> urls(
        GetColumnValuesFromDatabase<std::string>(database_path_, "icon_url"));

    EXPECT_THAT(urls, UnorderedElementsAre("", "https://www.google.com/icon",
                                           "https://www.google.com/icon"));
  }

  {
    // On versions < 15 |kCompatibleVersionNumber| was set to 1, but
    // the migration should bring it to the correct value.
    sql::Database db;
    sql::MetaTable meta_table;
    ASSERT_TRUE(db.Open(database_path_));
    ASSERT_TRUE(
        meta_table.Init(&db, kCurrentVersionNumber, kCompatibleVersionNumber));
    EXPECT_EQ(password_manager::kCompatibleVersionNumber,
              meta_table.GetCompatibleVersionNumber());
  }
  DestroyDatabase();
}

// Tests the migration of the login database from version() to
// kCurrentVersionNumber.
TEST_P(LoginDatabaseMigrationTest, MigrationToVCurrent) {
  MigrationToVCurrent(base::StringPrintf("login_db_v%d.sql", version()));
}

class LoginDatabaseMigrationTestV9 : public LoginDatabaseMigrationTest {};

// Tests migration from the alternative version #9, see crbug.com/423716.
TEST_P(LoginDatabaseMigrationTestV9, V9WithoutUseAdditionalAuthField) {
  ASSERT_EQ(9, version());
  MigrationToVCurrent("login_db_v9_without_use_additional_auth_field.sql");
}

class LoginDatabaseMigrationTestBroken : public LoginDatabaseMigrationTest {};

// Test migrating certain databases with incorrect version.
// http://crbug.com/295851
TEST_P(LoginDatabaseMigrationTestBroken, Broken) {
  MigrationToVCurrent(base::StringPrintf("login_db_v%d_broken.sql", version()));
}

INSTANTIATE_TEST_SUITE_P(MigrationToVCurrent,
                         LoginDatabaseMigrationTest,
                         testing::Range(1, kCurrentVersionNumber + 1));
INSTANTIATE_TEST_SUITE_P(MigrationToVCurrent,
                         LoginDatabaseMigrationTestV9,
                         testing::Values(9));
INSTANTIATE_TEST_SUITE_P(MigrationToVCurrent,
                         LoginDatabaseMigrationTestBroken,
                         testing::Values(1, 2, 3, 24));

class LoginDatabaseUndecryptableLoginsTest : public testing::Test {
 protected:
  LoginDatabaseUndecryptableLoginsTest() = default;

 public:
  LoginDatabaseUndecryptableLoginsTest(
      const LoginDatabaseUndecryptableLoginsTest&) = delete;
  LoginDatabaseUndecryptableLoginsTest& operator=(
      const LoginDatabaseUndecryptableLoginsTest&) = delete;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    database_path_ = temp_dir_.GetPath().AppendASCII("test.db");
    OSCryptMocker::SetUp();
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  // Generates login depending on |unique_string| and |origin| parameters and
  // adds it to the database. Changes encrypted password in the database if the
  // |should_be_corrupted| flag is active.
  PasswordForm AddDummyLogin(const std::string& unique_string,
                             const GURL& origin,
                             bool should_be_corrupted);

  base::FilePath database_path() const { return database_path_; }

  TestingPrefServiceSimple& testing_local_state() {
    return testing_local_state_;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::FilePath database_path_;
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple testing_local_state_;
};

PasswordForm LoginDatabaseUndecryptableLoginsTest::AddDummyLogin(
    const std::string& unique_string,
    const GURL& origin,
    bool should_be_corrupted) {
  // Create a dummy password form.
  const base::string16 unique_string16 = ASCIIToUTF16(unique_string);
  PasswordForm form;
  form.url = origin;
  form.username_element = unique_string16;
  form.username_value = unique_string16;
  form.password_element = unique_string16;
  form.password_value = unique_string16;
  form.signon_realm = origin.GetOrigin().spec();

  {
    LoginDatabase db(database_path(), IsAccountStore(false));
    EXPECT_TRUE(db.Init());
    EXPECT_EQ(db.AddLogin(form), AddChangeForForm(form));
  }

  if (should_be_corrupted) {
    sql::Database db;
    EXPECT_TRUE(db.Open(database_path()));

    // Change encrypted password in the database if the login should be
    // corrupted.
    std::string statement =
        "UPDATE logins SET password_value = password_value || 'trash' "
        "WHERE signon_realm = ? AND username_value = ?";
    sql::Statement s(db.GetCachedStatement(SQL_FROM_HERE, statement.c_str()));
    s.BindString(0, form.signon_realm);
    s.BindString(1, base::UTF16ToUTF8(form.username_value));

    EXPECT_TRUE(s.is_valid());
    EXPECT_TRUE(s.Run());
    EXPECT_EQ(db.GetLastChangeCount(), 1);
  }

  // When we retrieve the form from the store, |in_store| should be set.
  form.in_store = PasswordForm::Store::kProfileStore;

  return form;
}

TEST_F(LoginDatabaseUndecryptableLoginsTest, DeleteUndecryptableLoginsTest) {
  auto form1 = AddDummyLogin("foo1", GURL("https://foo1.com/"), false);
  auto form2 = AddDummyLogin("foo2", GURL("https://foo2.com/"), true);
  auto form3 = AddDummyLogin("foo3", GURL("https://foo3.com/"), false);

  LoginDatabase db(database_path(), IsAccountStore(false));
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(db.Init());

#if defined(OS_MAC)
  testing_local_state().registry()->RegisterTimePref(prefs::kPasswordRecovery,
                                                     base::Time());
  db.InitPasswordRecoveryUtil(std::make_unique<PasswordRecoveryUtilMac>(
      &testing_local_state(), base::ThreadTaskRunnerHandle::Get()));

  // Make sure that we can't get any logins when database is corrupted.
  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_FALSE(db.GetAutofillableLogins(&result));
  EXPECT_TRUE(result.empty());

  // Delete undecryptable logins and make sure we can get valid logins.
  EXPECT_EQ(DatabaseCleanupResult::kSuccess, db.DeleteUndecryptableLogins());
  EXPECT_TRUE(db.GetAutofillableLogins(&result));
  EXPECT_THAT(result, UnorderedElementsAre(Pointee(form1), Pointee(form3)));

  RunUntilIdle();

  // Make sure that password recovery pref is set.
  ASSERT_TRUE(testing_local_state().HasPrefPath(prefs::kPasswordRecovery));
#else
  EXPECT_EQ(DatabaseCleanupResult::kSuccess, db.DeleteUndecryptableLogins());
#endif

// Check histograms.
#if defined(OS_MAC)
  histogram_tester.ExpectUniqueSample("PasswordManager.CleanedUpPasswords", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.DeleteUndecryptableLoginsReturnValue",
      metrics_util::DeleteCorruptedPasswordsResult::kSuccessPasswordsDeleted,
      1);
#else
  EXPECT_TRUE(
      histogram_tester.GetAllSamples("PasswordManager.CleanedUpPasswords")
          .empty());
#endif
}

#if defined(OS_MAC)
TEST_F(LoginDatabaseUndecryptableLoginsTest,
       PasswordRecoveryDisabledGetLogins) {
  AddDummyLogin("foo1", GURL("https://foo1.com/"), false);
  AddDummyLogin("foo2", GURL("https://foo2.com/"), true);

  LoginDatabase db(database_path(), IsAccountStore(false));
  ASSERT_TRUE(db.Init());

  testing_local_state().registry()->RegisterTimePref(prefs::kPasswordRecovery,
                                                     base::Time());
  db.InitPasswordRecoveryUtil(std::make_unique<PasswordRecoveryUtilMac>(
      &testing_local_state(), base::ThreadTaskRunnerHandle::Get()));

  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_FALSE(db.GetAutofillableLogins(&result));
  EXPECT_TRUE(result.empty());

  RunUntilIdle();
  EXPECT_FALSE(testing_local_state().HasPrefPath(prefs::kPasswordRecovery));
}

TEST_F(LoginDatabaseUndecryptableLoginsTest, KeychainLockedTest) {
  AddDummyLogin("foo1", GURL("https://foo1.com/"), false);
  AddDummyLogin("foo2", GURL("https://foo2.com/"), true);

  OSCryptMocker::SetBackendLocked(true);
  LoginDatabase db(database_path(), IsAccountStore(false));
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(db.Init());
  EXPECT_EQ(DatabaseCleanupResult::kEncryptionUnavailable,
            db.DeleteUndecryptableLogins());

  EXPECT_TRUE(
      histogram_tester.GetAllSamples("PasswordManager.CleanedUpPasswords")
          .empty());
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.DeleteUndecryptableLoginsReturnValue",
      metrics_util::DeleteCorruptedPasswordsResult::kEncryptionUnavailable, 1);
}
#endif  // defined(OS_MAC)

// Test retrieving password forms by supplied password.
TEST_F(LoginDatabaseTest, GetLoginsByPassword) {
  std::vector<std::unique_ptr<PasswordForm>> result;
  PrimaryKeyToFormMap key_to_form_map;

  const base::string16 duplicated_password =
      base::ASCIIToUTF16("duplicated_password");

  // Insert first logins.
  PasswordForm form1;
  GenerateExamplePasswordForm(&form1);
  form1.password_value = duplicated_password;
  PasswordStoreChangeList changes = db().AddLogin(form1);
  ASSERT_EQ(AddChangeForForm(form1), changes);

  // Check if there is exactly one form with this password.
  std::vector<std::unique_ptr<PasswordForm>> forms;
  EXPECT_TRUE(db().GetLoginsByPassword(duplicated_password, &forms));
  EXPECT_THAT(forms, UnorderedElementsAre(Pointee(form1)));

  // Insert another form with a different password for a different origin.
  PasswordForm form2;
  GenerateExamplePasswordForm(&form2);
  form2.url = GURL("https://myrandomsite.com/login.php");
  form2.signon_realm = form2.url.GetOrigin().spec();
  form2.password_value = base::ASCIIToUTF16("my-unique-random-password");
  changes = db().AddLogin(form2);
  ASSERT_EQ(AddChangeForForm(form2), changes);

  // Check if there is still exactly one form with the duplicated_password.
  EXPECT_TRUE(db().GetLoginsByPassword(duplicated_password, &forms));
  EXPECT_THAT(forms, UnorderedElementsAre(Pointee(form1)));

  // Insert another form with the target password for a different origin.
  PasswordForm form3;
  GenerateExamplePasswordForm(&form3);
  form3.url = GURL("https://myrandomsite1.com/login.php");
  form3.signon_realm = form3.url.GetOrigin().spec();
  form3.password_value = duplicated_password;
  changes = db().AddLogin(form3);
  ASSERT_EQ(AddChangeForForm(form3), changes);

  // Check if there are exactly two forms with the duplicated_password.
  EXPECT_TRUE(db().GetLoginsByPassword(duplicated_password, &forms));
  EXPECT_THAT(forms, UnorderedElementsAre(Pointee(form1), Pointee(form3)));
}

// Test encrypted passwords are present in add change lists.
TEST_F(LoginDatabaseTest, EncryptedPasswordAdd) {
  PasswordForm form;
  form.url = GURL("http://0.com");
  form.signon_realm = "http://www.example.com/";
  form.action = GURL("http://www.example.com/action");
  form.password_element = base::ASCIIToUTF16("pwd");
  form.password_value = base::ASCIIToUTF16("example");
  password_manager::PasswordStoreChangeList changes = db().AddLogin(form);
  ASSERT_EQ(1u, changes.size());
  ASSERT_FALSE(changes[0].form().encrypted_password.empty());
}

// Test encrypted passwords are present in add change lists, when the password
// is already in the DB.
TEST_F(LoginDatabaseTest, EncryptedPasswordAddWithReplaceSemantics) {
  PasswordForm form;
  form.url = GURL("http://0.com");
  form.signon_realm = "http://www.example.com/";
  form.action = GURL("http://www.example.com/action");
  form.password_element = base::ASCIIToUTF16("pwd");
  form.password_value = base::ASCIIToUTF16("example");

  ignore_result(db().AddLogin(form));

  form.password_value = base::ASCIIToUTF16("secret");

  password_manager::PasswordStoreChangeList changes = db().AddLogin(form);
  ASSERT_EQ(2u, changes.size());
  ASSERT_EQ(password_manager::PasswordStoreChange::Type::ADD,
            changes[1].type());
  ASSERT_FALSE(changes[1].form().encrypted_password.empty());
}

// Test encrypted passwords are present in update change lists.
TEST_F(LoginDatabaseTest, EncryptedPasswordUpdate) {
  PasswordForm form;
  form.url = GURL("http://0.com");
  form.signon_realm = "http://www.example.com/";
  form.action = GURL("http://www.example.com/action");
  form.password_element = base::ASCIIToUTF16("pwd");
  form.password_value = base::ASCIIToUTF16("example");

  ignore_result(db().AddLogin(form));

  form.password_value = base::ASCIIToUTF16("secret");

  password_manager::PasswordStoreChangeList changes = db().UpdateLogin(form);
  ASSERT_EQ(1u, changes.size());
  ASSERT_FALSE(changes[0].form().encrypted_password.empty());
}

// Test encrypted passwords are present when retrieving from DB.
TEST_F(LoginDatabaseTest, GetLoginsEncryptedPassword) {
  PasswordForm form;
  form.url = GURL("http://0.com");
  form.signon_realm = "http://www.example.com/";
  form.action = GURL("http://www.example.com/action");
  form.password_element = base::ASCIIToUTF16("pwd");
  form.password_value = base::ASCIIToUTF16("example");
  password_manager::PasswordStoreChangeList changes = db().AddLogin(form);
  ASSERT_EQ(1u, changes.size());
  ASSERT_FALSE(changes[0].form().encrypted_password.empty());

  std::vector<std::unique_ptr<PasswordForm>> forms;
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form), &forms));

  ASSERT_EQ(1U, forms.size());
  ASSERT_FALSE(forms[0]->encrypted_password.empty());
}

class LoginDatabaseForAccountStoreTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestMetadataStoreMacDatabase");
    OSCryptMocker::SetUp();

    db_ = std::make_unique<LoginDatabase>(file_, IsAccountStore(true));
    ASSERT_TRUE(db_->Init());
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  LoginDatabase& db() { return *db_; }

  base::ScopedTempDir temp_dir_;
  base::FilePath file_;
  std::unique_ptr<LoginDatabase> db_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(LoginDatabaseForAccountStoreTest, AddLogins) {
  PasswordForm form;
  GenerateExamplePasswordForm(&form);

  PasswordStoreChangeList changes = db().AddLogin(form);
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(PasswordForm::Store::kAccountStore, changes[0].form().in_store);
}

}  // namespace password_manager
