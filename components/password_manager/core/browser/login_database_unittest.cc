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
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "components/os_crypt/os_crypt.h"
#include "components/os_crypt/os_crypt_mocker.h"
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

using autofill::PasswordForm;
using autofill::ValueElementPair;
using autofill::ValueElementVector;
using base::ASCIIToUTF16;
using ::testing::Eq;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

namespace password_manager {
namespace {

PasswordStoreChangeList AddChangeForForm(const PasswordForm& form) {
  return PasswordStoreChangeList(
      1, PasswordStoreChange(PasswordStoreChange::ADD, form));
}

PasswordStoreChangeList UpdateChangeForForm(const PasswordForm& form) {
  return PasswordStoreChangeList(
      1, PasswordStoreChange(PasswordStoreChange::UPDATE, form));
}

void GenerateExamplePasswordForm(PasswordForm* form) {
  form->origin = GURL("http://accounts.google.com/LoginAuth");
  form->action = GURL("http://accounts.google.com/Login");
  form->username_element = ASCIIToUTF16("Email");
  form->username_value = ASCIIToUTF16("test@gmail.com");
  form->password_element = ASCIIToUTF16("Passwd");
  form->password_value = ASCIIToUTF16("test");
  form->submit_element = ASCIIToUTF16("signIn");
  form->signon_realm = "http://www.google.com/";
  form->preferred = false;
  form->scheme = PasswordForm::SCHEME_HTML;
  form->times_used = 1;
  form->form_data.name = ASCIIToUTF16("form_name");
  form->date_synced = base::Time::Now();
  form->display_name = ASCIIToUTF16("Mr. Smith");
  form->icon_url = GURL("https://accounts.google.com/Icon");
  form->federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));
  form->skip_zero_click = true;
}

// Helper functions to read the value of the first column of an executed
// statement if we know its type. You must implement a specialization for
// every column type you use.
template<class T> struct must_be_specialized {
  static const bool is_specialized = false;
};

template<class T> T GetFirstColumn(const sql::Statement& s) {
  static_assert(must_be_specialized<T>::is_specialized,
                "Implement a specialization.");
}

template<> int64_t GetFirstColumn(const sql::Statement& s) {
  return s.ColumnInt64(0);
}

template<> std::string GetFirstColumn(const sql::Statement& s) {
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
  form.origin = origin;
  form.username_element = ASCIIToUTF16(unique_string);
  form.username_value = ASCIIToUTF16(unique_string);
  form.password_element = ASCIIToUTF16(unique_string);
  form.submit_element = ASCIIToUTF16("signIn");
  form.signon_realm = form.origin.spec();
  form.display_name = ASCIIToUTF16(unique_string);
  form.icon_url = origin;
  form.federation_origin = url::Origin::Create(origin);
  form.date_created = base::Time::Now();

  form.skip_zero_click = false;

  return db->AddLogin(form) == AddChangeForForm(form);
}

MATCHER(IsGoogle1Account, "") {
  return arg.origin.spec() == "https://accounts.google.com/ServiceLogin" &&
         arg.action.spec() == "https://accounts.google.com/ServiceLoginAuth" &&
         arg.username_value == ASCIIToUTF16("theerikchen") &&
         arg.scheme == PasswordForm::SCHEME_HTML;
}

MATCHER(IsGoogle2Account, "") {
  return arg.origin.spec() == "https://accounts.google.com/ServiceLogin" &&
         arg.action.spec() == "https://accounts.google.com/ServiceLoginAuth" &&
         arg.username_value == ASCIIToUTF16("theerikchen2") &&
         arg.scheme == PasswordForm::SCHEME_HTML;
}

MATCHER(IsBasicAuthAccount, "") {
  return arg.scheme == PasswordForm::SCHEME_BASIC;
}

}  // namespace

// Serialization routines for vectors implemented in login_database.cc.
base::Pickle SerializeValueElementPairs(const ValueElementVector& vec);
ValueElementVector DeserializeValueElementPairs(const base::Pickle& pickle);

class LoginDatabaseTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestMetadataStoreMacDatabase");
    OSCryptMocker::SetUp();

    db_.reset(new LoginDatabase(file_));
    ASSERT_TRUE(db_->Init());
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  LoginDatabase& db() { return *db_; }

  void TestNonHTMLFormPSLMatching(const PasswordForm::Scheme& scheme) {
    std::vector<std::unique_ptr<PasswordForm>> result;

    base::Time now = base::Time::Now();

    // Simple non-html auth form.
    PasswordForm non_html_auth;
    non_html_auth.origin = GURL("http://example.com");
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
    html_form.scheme = PasswordForm::SCHEME_HTML;
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
    db().RemoveLoginsCreatedBetween(now, base::Time());
  }

  // Checks that a form of a given |scheme|, once stored, can be successfully
  // retrieved from the database.
  void TestRetrievingIPAddress(const PasswordForm::Scheme& scheme) {
    SCOPED_TRACE(testing::Message() << "scheme = " << scheme);
    std::vector<std::unique_ptr<PasswordForm>> result;

    base::Time now = base::Time::Now();
    std::string origin("http://56.7.8.90");

    PasswordForm ip_form;
    ip_form.origin = GURL(origin);
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
    db().RemoveLoginsCreatedBetween(now, base::Time());
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath file_;
  std::unique_ptr<LoginDatabase> db_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

TEST_F(LoginDatabaseTest, Logins) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  GenerateExamplePasswordForm(&form);

  // Add it and make sure it is there and that all the fields were retrieved
  // correctly.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form, *result[0]);
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form), &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form, *result[0]);
  result.clear();

  // The example site changes...
  PasswordForm form2(form);
  form2.origin = GURL("http://www.google.com/new/accounts/LoginAuth");
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
  EXPECT_EQ(AddChangeForForm(form4), db().AddLogin(form4));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(2U, result.size());
  result.clear();

  // Now the match works
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form4), &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // The user chose to forget the original but not the new.
  EXPECT_TRUE(db().RemoveLogin(form));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // The old form wont match the new site (http vs https).
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form), &result));
  EXPECT_EQ(0U, result.size());

  // User changes their password.
  PasswordForm form5(form4);
  form5.password_value = ASCIIToUTF16("test6");
  form5.preferred = true;

  // We update, and check to make sure it matches the
  // old form, and there is only one record.
  EXPECT_EQ(UpdateChangeForForm(form5), db().UpdateLogin(form5));
  // matches
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form5), &result));
  EXPECT_EQ(1U, result.size());
  result.clear();
  // Only one record.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  // Password element was updated.
  EXPECT_EQ(form5.password_value, result[0]->password_value);
  // Preferred login.
  EXPECT_TRUE(form5.preferred);
  result.clear();

  // Make sure everything can disappear.
  EXPECT_TRUE(db().RemoveLogin(form4));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());
}

TEST_F(LoginDatabaseTest, TestPublicSuffixDomainMatching) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  form.origin = GURL("https://foo.com/");
  form.action = GURL("https://foo.com/login");
  form.username_element = ASCIIToUTF16("username");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("password");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("");
  form.signon_realm = "https://foo.com/";
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

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
  form2.origin = GURL("https://mobile.foo.com/");
  form2.action = GURL("https://mobile.foo.com/login");
  form2.signon_realm = "https://mobile.foo.com/";

  // Match against the mobile site.
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form2), &result));
  EXPECT_EQ(1U, result.size());
  EXPECT_EQ("https://foo.com/", result[0]->signon_realm);
  EXPECT_TRUE(result[0]->is_public_suffix_match);

  // Try to remove PSL matched form
  EXPECT_FALSE(db().RemoveLogin(*result[0]));
  result.clear();
  // Ensure that the original form is still there
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form), &result));
  EXPECT_EQ(1U, result.size());
  result.clear();
}

TEST_F(LoginDatabaseTest, TestFederatedMatching) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Example password form.
  PasswordForm form;
  form.origin = GURL("https://foo.com/");
  form.action = GURL("https://foo.com/login");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_value = ASCIIToUTF16("test");
  form.signon_realm = "https://foo.com/";
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

  // We go to the mobile site.
  PasswordForm form2(form);
  form2.origin = GURL("https://mobile.foo.com/");
  form2.action = GURL("https://mobile.foo.com/login");
  form2.signon_realm = "federation://mobile.foo.com/accounts.google.com";
  form2.username_value = ASCIIToUTF16("test1@gmail.com");
  form2.type = PasswordForm::TYPE_API;
  form2.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_EQ(AddChangeForForm(form2), db().AddLogin(form2));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(2U, result.size());

  // Match against desktop.
  PasswordStore::FormDigest form_request = {
      PasswordForm::SCHEME_HTML, "https://foo.com/", GURL("https://foo.com/")};
  EXPECT_TRUE(db().GetLogins(form_request, &result));
  // Both forms are matched, only form2 is a PSL match.
  form.is_public_suffix_match = false;
  form2.is_public_suffix_match = true;
  EXPECT_THAT(result, UnorderedElementsAre(Pointee(form), Pointee(form2)));

  // Match against the mobile site.
  form_request.origin = GURL("https://mobile.foo.com/");
  form_request.signon_realm = "https://mobile.foo.com/";
  EXPECT_TRUE(db().GetLogins(form_request, &result));
  // Both forms are matched, only form is a PSL match.
  form.is_public_suffix_match = true;
  form2.is_public_suffix_match = false;
  EXPECT_THAT(result, UnorderedElementsAre(Pointee(form), Pointee(form2)));
}

TEST_F(LoginDatabaseTest, TestFederatedMatchingLocalhost) {
  PasswordForm form;
  form.origin = GURL("http://localhost/");
  form.signon_realm = "federation://localhost/accounts.google.com";
  form.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.type = PasswordForm::TYPE_API;
  form.scheme = PasswordForm::SCHEME_HTML;

  PasswordForm form_with_port(form);
  form_with_port.origin = GURL("http://localhost:8080/");
  form_with_port.signon_realm = "federation://localhost/accounts.google.com";

  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_EQ(AddChangeForForm(form_with_port), db().AddLogin(form_with_port));

  // Match localhost with and without port.
  PasswordStore::FormDigest form_request(PasswordForm::SCHEME_HTML,
                                         "http://localhost/",
                                         GURL("http://localhost/"));
  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_TRUE(db().GetLogins(form_request, &result));
  EXPECT_THAT(result, UnorderedElementsAre(Pointee(form)));

  form_request.origin = GURL("http://localhost:8080/");
  form_request.signon_realm = "http://localhost:8080/";
  EXPECT_TRUE(db().GetLogins(form_request, &result));
  EXPECT_THAT(result, UnorderedElementsAre(Pointee(form_with_port)));
}

TEST_F(LoginDatabaseTest, TestPublicSuffixDisabledForNonHTMLForms) {
  TestNonHTMLFormPSLMatching(PasswordForm::SCHEME_BASIC);
  TestNonHTMLFormPSLMatching(PasswordForm::SCHEME_DIGEST);
  TestNonHTMLFormPSLMatching(PasswordForm::SCHEME_OTHER);
}

TEST_F(LoginDatabaseTest, TestIPAddressMatches_HTML) {
  TestRetrievingIPAddress(PasswordForm::SCHEME_HTML);
}

TEST_F(LoginDatabaseTest, TestIPAddressMatches_basic) {
  TestRetrievingIPAddress(PasswordForm::SCHEME_BASIC);
}

TEST_F(LoginDatabaseTest, TestIPAddressMatches_digest) {
  TestRetrievingIPAddress(PasswordForm::SCHEME_DIGEST);
}

TEST_F(LoginDatabaseTest, TestIPAddressMatches_other) {
  TestRetrievingIPAddress(PasswordForm::SCHEME_OTHER);
}

TEST_F(LoginDatabaseTest, TestPublicSuffixDomainMatchingShouldMatchingApply) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  form.origin = GURL("https://accounts.google.com/");
  form.action = GURL("https://accounts.google.com/login");
  form.username_element = ASCIIToUTF16("username");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("password");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("");
  form.signon_realm = "https://accounts.google.com/";
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form), &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // We go to a different site on the same domain where feature is not needed.
  PasswordStore::FormDigest form2 = {PasswordForm::SCHEME_HTML,
                                     "https://some.other.google.com/",
                                     GURL("https://some.other.google.com/")};

  // Match against the other site. Should not match since feature should not be
  // enabled for this domain.
  ASSERT_FALSE(ShouldPSLDomainMatchingApply(
      GetRegistryControlledDomain(GURL(form2.signon_realm))));

  EXPECT_TRUE(db().GetLogins(form2, &result));
  EXPECT_EQ(0U, result.size());
}

TEST_F(LoginDatabaseTest, TestFederatedMatchingWithoutPSLMatching) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Example password form.
  PasswordForm form;
  form.origin = GURL("https://accounts.google.com/");
  form.action = GURL("https://accounts.google.com/login");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_value = ASCIIToUTF16("test");
  form.signon_realm = "https://accounts.google.com/";
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

  // We go to a different site on the same domain where PSL is disabled.
  PasswordForm form2(form);
  form2.origin = GURL("https://some.other.google.com/");
  form2.action = GURL("https://some.other.google.com/login");
  form2.signon_realm = "federation://some.other.google.com/accounts.google.com";
  form2.username_value = ASCIIToUTF16("test1@gmail.com");
  form2.type = PasswordForm::TYPE_API;
  form2.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_EQ(AddChangeForForm(form2), db().AddLogin(form2));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(2U, result.size());

  // Match against the first one.
  PasswordStore::FormDigest form_request = {PasswordForm::SCHEME_HTML,
                                            form.signon_realm, form.origin};
  EXPECT_TRUE(db().GetLogins(form_request, &result));
  EXPECT_THAT(result, testing::ElementsAre(Pointee(form)));

  // Match against the second one.
  ASSERT_FALSE(ShouldPSLDomainMatchingApply(
      GetRegistryControlledDomain(GURL(form2.signon_realm))));
  form_request.origin = form2.origin;
  form_request.signon_realm = form2.signon_realm;
  EXPECT_TRUE(db().GetLogins(form_request, &result));
  form.is_public_suffix_match = true;
  EXPECT_THAT(result, testing::ElementsAre(Pointee(form2)));
}

TEST_F(LoginDatabaseTest, TestFederatedPSLMatching) {
  // Save a federated credential for the PSL matched site.
  PasswordForm form;
  form.origin = GURL("https://psl.example.com/");
  form.action = GURL("https://psl.example.com/login");
  form.signon_realm = "federation://psl.example.com/accounts.google.com";
  form.username_value = ASCIIToUTF16("test1@gmail.com");
  form.type = PasswordForm::TYPE_API;
  form.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));
  form.scheme = PasswordForm::SCHEME_HTML;
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  // Match against.
  PasswordStore::FormDigest form_request = {PasswordForm::SCHEME_HTML,
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
  form.origin = GURL("https://foo.com/");
  form.action = GURL("https://foo.com/login");
  form.username_element = ASCIIToUTF16("username");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("password");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("");
  form.signon_realm = "https://foo.com/";
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

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
  form2.origin = GURL("https://mobile.foo.com/");
  form2.signon_realm = "https://mobile.foo.com/";

  // Match against the mobile site.
  EXPECT_TRUE(db().GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  EXPECT_EQ("https://foo.com/", result[0]->signon_realm);
  EXPECT_TRUE(result[0]->is_public_suffix_match);
  result.clear();

  // Add baz.com desktop site.
  form.origin = GURL("https://baz.com/login/");
  form.action = GURL("https://baz.com/login/");
  form.username_element = ASCIIToUTF16("email");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("password");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("");
  form.signon_realm = "https://baz.com/";
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(2U, result.size());
  result.clear();

  // We go to the mobile site of baz.com.
  PasswordStore::FormDigest form3(form);
  form3.origin = GURL("https://m.baz.com/login/");
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
  form2.origin = GURL(signon_realm);
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
  form.origin = GURL("http://foo.com/");
  form.action = GURL("http://foo.com/login");
  form.username_element = ASCIIToUTF16("username");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("password");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("");
  form.signon_realm = "http://foo.com/";
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

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

TEST_F(LoginDatabaseTest,
       GetLoginsForSameOrganizationName_OnlyWebHTTPFormsAreConsidered) {
  static constexpr const struct {
    const PasswordFormData form_data;
    bool use_federated_login;
    const char* other_queried_signon_realm;
    bool expected_matches_itself;
    bool expected_matches_other_realm;
  } kTestCases[] = {
      {{PasswordForm::SCHEME_HTML, "https://example.com/",
        "https://example.com/origin", "", L"", L"", L"", L"u", L"p", false, 1},
       false,
       nullptr,
       true,
       true},
      {{PasswordForm::SCHEME_BASIC, "http://example.com/realm",
        "http://example.com/", "", L"", L"", L"", L"u", L"p", false, 1},
       false,
       nullptr,
       false,
       false},
      {{PasswordForm::SCHEME_OTHER, "ftp://example.com/realm",
        "ftp://example.com/", "", L"", L"", L"", L"u", L"p", false, 1},
       false,
       "http://example.com/realm",
       false,
       false},
      {{PasswordForm::SCHEME_HTML,
        "federation://example.com/accounts.google.com",
        "https://example.com/orgin", "", L"", L"", L"", L"u", L"", false, 1},
       true,
       "http://example.com/",
       false,
       false},
      {{PasswordForm::SCHEME_HTML, "android://hash@example.com/",
        "android://hash@example.com/", "", L"", L"", L"", L"u", L"p", false, 1},
       false,
       "http://example.com/",
       false,
       false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.form_data.signon_realm);

    std::unique_ptr<PasswordForm> form = FillPasswordFormWithData(
        test_case.form_data, test_case.use_federated_login);
    ASSERT_EQ(AddChangeForForm(*form), db().AddLogin(*form));

    std::vector<std::unique_ptr<PasswordForm>> same_organization_forms;
    EXPECT_TRUE(db().GetLoginsForSameOrganizationName(
        form->signon_realm, &same_organization_forms));
    EXPECT_EQ(test_case.expected_matches_itself ? 1u : 0u,
              same_organization_forms.size());

    if (test_case.other_queried_signon_realm) {
      same_organization_forms.clear();
      EXPECT_TRUE(db().GetLoginsForSameOrganizationName(
          test_case.other_queried_signon_realm, &same_organization_forms));
      EXPECT_EQ(test_case.expected_matches_other_realm ? 1u : 0u,
                same_organization_forms.size());
    }

    ASSERT_TRUE(db().RemoveLogin(*form));
  }
}

TEST_F(LoginDatabaseTest, GetLoginsForSameOrganizationName_DetailsOfMatching) {
  const struct {
    const char* saved_signon_realm;
    const char* queried_signon_realm;
    bool expected_matches;
  } kTestCases[] = {
      // PSL matches are also same-organization-name matches.
      {"http://psl.example.com/", "http://example.com/", true},
      {"http://example.com/", "http://sub.example.com/", true},
      {"https://a.b.example.co.uk/", "https://c.d.e.example.co.uk/", true},

      // Non-PSL but same-organization-name matches. Also an illustration why it
      // would be unsafe to offer these credentials for filling.
      {"https://example.com/", "https://example.co.uk/", true},
      {"https://example.co.uk/", "https://example.com/", true},
      {"https://a.example.appspot.com/", "https://b.example.co.uk/", true},

      // Same-organization-name matches are HTTP/HTTPS-agnostic.
      {"https://example.com/", "http://example.com/", true},
      {"http://example.com/", "https://example.com/", true},

      {"http://www.foo-bar.com/", "http://sub.foo-bar.com", true},
      {"http://www.foo_bar.com/", "http://sub.foo_bar.com", true},
      {"http://www.foo-bar.com/", "http://sub.foo%2Dbar.com", true},
      {"http://www.foo%21bar.com/", "http://sub.foo!bar.com", true},
      {"http://a.xn--sztr-7na0i.co.uk/", "http://xn--sztr-7na0i.com/", true},
      {"http://a.xn--sztr-7na0i.co.uk/", "http://www.sz\xc3\xb3t\xc3\xa1r.com/",
       true},

      {"http://www.foo+bar.com/", "http://sub.foo+bar.com", true},
      {"http://www.foooobar.com/", "http://sub.foo+bar.com", false},
      {"http://www.fobar.com/", "http://sub.foo?bar.com", false},
      {"http://www.foozbar.com/", "http://sub.foo.bar.com", false},
      {"http://www.foozbar.com/", "http://sub.foo[a-z]bar.com", false},

      {"https://notexample.com/", "https://example.com/", false},
      {"https://a.notexample.com/", "https://example.com/", false},
      {"https://example.com/", "https://notexample.com/", false},
      {"https://example.com/", "https://example.bar.com/", false},
      {"https://example.foo.com/", "https://example.com/", false},
      {"https://example.foo.com/", "https://example.bar.com/", false},

      // URLs without host portions, hosts without registry controlled domains
      // or hosts consisting of a registry.
      {"http://localhost/", "http://localhost/", false},
      {"https://example/", "https://example/", false},
      {"https://co.uk/", "https://co.uk/", false},
      {"https://example/", "https://example.com/", false},
      {"https://a.example/", "https://example.com/", false},
      {"https://example.com/", "https://example/", false},
      {"https://127.0.0.1/", "https://127.0.0.1/", false},
      {"https:/[3ffe:2a00:100:7031::1]/", "https:/[3ffe:2a00:100:7031::1]/",
       false},

      // Queried |signon-realms| are invalid URIs.
      {"https://example.com/", "", false},
      {"https://example.com/", "bad url", false},
      {"https://example.com/", "https://", false},
      {"https://example.com/", "http://www.foo;bar.com", false},
      {"https://example.com/", "example", false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.saved_signon_realm);
    SCOPED_TRACE(test_case.queried_signon_realm);

    std::unique_ptr<PasswordForm> form = FillPasswordFormWithData(
        {PasswordForm::SCHEME_HTML, test_case.saved_signon_realm,
         test_case.saved_signon_realm, "", L"", L"", L"", L"u", L"p", true, 1});
    std::vector<std::unique_ptr<PasswordForm>> result;
    ASSERT_EQ(AddChangeForForm(*form), db().AddLogin(*form));
    EXPECT_TRUE(db().GetLoginsForSameOrganizationName(
        test_case.queried_signon_realm, &result));
    EXPECT_EQ(test_case.expected_matches ? 1u : 0u, result.size());
    ASSERT_TRUE(db().RemoveLogin(*form));
  }
}

static bool AddTimestampedLogin(LoginDatabase* db,
                                std::string url,
                                const std::string& unique_string,
                                const base::Time& time,
                                bool date_is_creation) {
  // Example password form.
  PasswordForm form;
  form.origin = GURL(url + std::string("/LoginAuth"));
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
  EXPECT_TRUE(db().GetLoginsCreatedBetween(now, base::Time(), &result));
  EXPECT_EQ(2U, result.size());
  result.clear();

  // Get all logins created more than 30 days back.
  EXPECT_TRUE(
      db().GetLoginsCreatedBetween(base::Time(), back_30_days, &result));
  EXPECT_EQ(2U, result.size());
  result.clear();

  // Delete everything from today's date and on.
  db().RemoveLoginsCreatedBetween(now, base::Time());

  // Should have deleted two logins.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(3U, result.size());
  result.clear();

  // Delete all logins created more than 30 days back.
  db().RemoveLoginsCreatedBetween(base::Time(), back_30_days);

  // Should have deleted two logins.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // Delete with 0 date (should delete all).
  db().RemoveLoginsCreatedBetween(base::Time(), base::Time());

  // Verify nothing is left.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());
}

TEST_F(LoginDatabaseTest, RemoveLoginsSyncedBetween) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  base::Time now = base::Time::Now();
  base::TimeDelta one_day = base::TimeDelta::FromDays(1);

  // Create one with a 0 time.
  EXPECT_TRUE(
      AddTimestampedLogin(&db(), "http://1.com", "foo1", base::Time(), false));
  // Create one for now and +/- 1 day.
  EXPECT_TRUE(
      AddTimestampedLogin(&db(), "http://2.com", "foo2", now - one_day, false));
  EXPECT_TRUE(AddTimestampedLogin(&db(), "http://3.com", "foo3", now, false));
  EXPECT_TRUE(
      AddTimestampedLogin(&db(), "http://4.com", "foo4", now + one_day, false));

  // Verify inserts worked.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(4U, result.size());
  result.clear();

  // Get everything from today's date and on.
  EXPECT_TRUE(db().GetLoginsSyncedBetween(now, base::Time(), &result));
  ASSERT_EQ(2U, result.size());
  EXPECT_EQ("http://3.com", result[0]->signon_realm);
  EXPECT_EQ("http://4.com", result[1]->signon_realm);
  result.clear();

  // Delete everything from today's date and on.
  db().RemoveLoginsSyncedBetween(now, base::Time());

  // Should have deleted half of what we inserted.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(2U, result.size());
  EXPECT_EQ("http://1.com", result[0]->signon_realm);
  EXPECT_EQ("http://2.com", result[1]->signon_realm);
  result.clear();

  // Delete with 0 date (should delete all).
  db().RemoveLoginsSyncedBetween(base::Time(), now);

  // Verify nothing is left.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());
}

TEST_F(LoginDatabaseTest, GetAutoSignInLogins) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  GURL origin("https://example.com");
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo1", origin));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo2", origin));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo3", origin));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo4", origin));

  EXPECT_TRUE(db().GetAutoSignInLogins(&result));
  EXPECT_EQ(4U, result.size());
  for (const auto& form : result)
    EXPECT_FALSE(form->skip_zero_click);

  EXPECT_TRUE(db().DisableAutoSignInForOrigin(origin));
  EXPECT_TRUE(db().GetAutoSignInLogins(&result));
  EXPECT_EQ(0U, result.size());
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
    if (form->origin == origin1 || form->origin == origin3)
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
  form.origin = GURL("http://accounts.google.com/LoginAuth");
  form.action = GURL("http://accounts.google.com/Login");
  form.username_element = ASCIIToUTF16("Email");
  form.password_element = ASCIIToUTF16("Passwd");
  form.submit_element = ASCIIToUTF16("signIn");
  form.signon_realm = "http://www.google.com/";
  form.preferred = true;
  form.blacklisted_by_user = true;
  form.scheme = PasswordForm::SCHEME_HTML;
  form.date_synced = base::Time::Now();
  form.display_name = ASCIIToUTF16("Mr. Smith");
  form.icon_url = GURL("https://accounts.google.com/Icon");
  form.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));
  form.skip_zero_click = true;
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  // Get all non-blacklisted logins (should be none).
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(0U, result.size());

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
  incomplete_form.origin = GURL("http://accounts.google.com/LoginAuth");
  incomplete_form.signon_realm = "http://accounts.google.com/";
  incomplete_form.username_value = ASCIIToUTF16("my_username");
  incomplete_form.password_value = ASCIIToUTF16("my_password");
  incomplete_form.preferred = true;
  incomplete_form.blacklisted_by_user = false;
  incomplete_form.scheme = PasswordForm::SCHEME_HTML;
  EXPECT_EQ(AddChangeForForm(incomplete_form), db().AddLogin(incomplete_form));

  // A form on some website. It should trigger a match with the stored one.
  PasswordForm encountered_form;
  encountered_form.origin = GURL("http://accounts.google.com/LoginAuth");
  encountered_form.signon_realm = "http://accounts.google.com/";
  encountered_form.action = GURL("http://accounts.google.com/Login");
  encountered_form.username_element = ASCIIToUTF16("Email");
  encountered_form.password_element = ASCIIToUTF16("Passwd");
  encountered_form.submit_element = ASCIIToUTF16("signIn");

  // Get matches for encountered_form.
  EXPECT_TRUE(
      db().GetLogins(PasswordStore::FormDigest(encountered_form), &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(incomplete_form.origin, result[0]->origin);
  EXPECT_EQ(incomplete_form.signon_realm, result[0]->signon_realm);
  EXPECT_EQ(incomplete_form.username_value, result[0]->username_value);
  EXPECT_EQ(incomplete_form.password_value, result[0]->password_value);
  EXPECT_TRUE(result[0]->preferred);

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
  EXPECT_TRUE(db().RemoveLogin(incomplete_form));

  // Get matches for encountered_form again.
  EXPECT_TRUE(
      db().GetLogins(PasswordStore::FormDigest(encountered_form), &result));
  ASSERT_EQ(1U, result.size());

  // This time we should have all the info available.
  PasswordForm expected_form(completed_form);
  EXPECT_EQ(expected_form, *result[0]);
  result.clear();
}

TEST_F(LoginDatabaseTest, UpdateOverlappingCredentials) {
  // Save an incomplete form. Note that it only has a few fields set, ex. it's
  // missing 'action', 'username_element' and 'password_element'. Such forms
  // are sometimes inserted during import from other browsers (which may not
  // store this info).
  PasswordForm incomplete_form;
  incomplete_form.origin = GURL("http://accounts.google.com/LoginAuth");
  incomplete_form.signon_realm = "http://accounts.google.com/";
  incomplete_form.username_value = ASCIIToUTF16("my_username");
  incomplete_form.password_value = ASCIIToUTF16("my_password");
  incomplete_form.preferred = true;
  incomplete_form.blacklisted_by_user = false;
  incomplete_form.scheme = PasswordForm::SCHEME_HTML;
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
  EXPECT_EQ(UpdateChangeForForm(complete_form),
            db().UpdateLogin(complete_form));

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
  form.origin = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = ASCIIToUTF16("my_username");
  form.password_value = ASCIIToUTF16("my_password");
  form.preferred = true;
  form.blacklisted_by_user = false;
  form.scheme = PasswordForm::SCHEME_HTML;
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
  form.origin = GURL();
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = ASCIIToUTF16("my_username");
  form.password_value = ASCIIToUTF16("my_password");
  form.preferred = true;
  form.blacklisted_by_user = false;
  form.scheme = PasswordForm::SCHEME_HTML;
  EXPECT_EQ(PasswordStoreChangeList(), db().AddLogin(form));

  // |signon_realm| shouldn't be empty.
  form.origin = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm.clear();
  EXPECT_EQ(PasswordStoreChangeList(), db().AddLogin(form));
}

TEST_F(LoginDatabaseTest, UpdateLogin) {
  PasswordForm form;
  form.origin = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = ASCIIToUTF16("my_username");
  form.password_value = ASCIIToUTF16("my_password");
  form.preferred = true;
  form.blacklisted_by_user = false;
  form.scheme = PasswordForm::SCHEME_HTML;
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  form.action = GURL("http://accounts.google.com/login");
  form.password_value = ASCIIToUTF16("my_new_password");
  form.preferred = false;
  form.other_possible_usernames.push_back(autofill::ValueElementPair(
      ASCIIToUTF16("my_new_username"), ASCIIToUTF16("new_username_id")));
  form.times_used = 20;
  form.submit_element = ASCIIToUTF16("submit_element");
  form.date_synced = base::Time::Now();
  form.date_created = base::Time::Now() - base::TimeDelta::FromDays(1);
  form.blacklisted_by_user = true;
  form.scheme = PasswordForm::SCHEME_BASIC;
  form.type = PasswordForm::TYPE_GENERATED;
  form.display_name = ASCIIToUTF16("Mr. Smith");
  form.icon_url = GURL("https://accounts.google.com/Icon");
  form.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));
  form.skip_zero_click = true;
  EXPECT_EQ(UpdateChangeForForm(form), db().UpdateLogin(form));

  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_TRUE(db().GetLogins(PasswordStore::FormDigest(form), &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form, *result[0]);
}

TEST_F(LoginDatabaseTest, RemoveWrongForm) {
  PasswordForm form;
  // |origin| shouldn't be empty.
  form.origin = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = ASCIIToUTF16("my_username");
  form.password_value = ASCIIToUTF16("my_password");
  form.preferred = true;
  form.blacklisted_by_user = false;
  form.scheme = PasswordForm::SCHEME_HTML;
  // The form isn't in the database.
  EXPECT_FALSE(db().RemoveLogin(form));

  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_TRUE(db().RemoveLogin(form));
  EXPECT_FALSE(db().RemoveLogin(form));
}

TEST_F(LoginDatabaseTest, ReportMetricsTest) {
  PasswordForm password_form;
  password_form.origin = GURL("http://example.com");
  password_form.username_value = ASCIIToUTF16("test1@gmail.com");
  password_form.password_value = ASCIIToUTF16("test");
  password_form.signon_realm = "http://example.com/";
  password_form.times_used = 0;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.username_value = ASCIIToUTF16("test2@gmail.com");
  password_form.times_used = 1;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.origin = GURL("http://second.example.com");
  password_form.signon_realm = "http://second.example.com";
  password_form.times_used = 3;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.username_value = ASCIIToUTF16("test3@gmail.com");
  password_form.type = PasswordForm::TYPE_GENERATED;
  password_form.times_used = 2;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.origin = GURL("ftp://third.example.com/");
  password_form.signon_realm = "ftp://third.example.com/";
  password_form.times_used = 4;
  password_form.scheme = PasswordForm::SCHEME_OTHER;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.origin = GURL("http://fourth.example.com/");
  password_form.signon_realm = "http://fourth.example.com/";
  password_form.type = PasswordForm::TYPE_MANUAL;
  password_form.username_value = ASCIIToUTF16("");
  password_form.times_used = 10;
  password_form.scheme = PasswordForm::SCHEME_HTML;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.origin = GURL("https://fifth.example.com/");
  password_form.signon_realm = "https://fifth.example.com/";
  password_form.password_value = ASCIIToUTF16("");
  password_form.blacklisted_by_user = true;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.origin = GURL("https://sixth.example.com/");
  password_form.signon_realm = "https://sixth.example.com/";
  password_form.username_value = ASCIIToUTF16("");
  password_form.password_value = ASCIIToUTF16("my_password");
  password_form.blacklisted_by_user = false;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.username_element = ASCIIToUTF16("some_other_input");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.username_value = ASCIIToUTF16("my_username");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.origin = GURL();
  password_form.signon_realm = "android://hash@com.example.android/";
  password_form.username_value = ASCIIToUTF16("JohnDoe");
  password_form.password_value = ASCIIToUTF16("my_password");
  password_form.blacklisted_by_user = false;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.username_value = ASCIIToUTF16("JaneDoe");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.origin = GURL("http://rsolomakhin.github.io/autofill/");
  password_form.signon_realm = "http://rsolomakhin.github.io/";
  password_form.blacklisted_by_user = true;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.origin = GURL("https://rsolomakhin.github.io/autofill/");
  password_form.signon_realm = "https://rsolomakhin.github.io/";
  password_form.blacklisted_by_user = true;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.origin = GURL("http://rsolomakhin.github.io/autofill/123");
  password_form.signon_realm = "http://rsolomakhin.github.io/";
  password_form.blacklisted_by_user = true;
  EXPECT_EQ(AddChangeForForm(password_form),
            db().AddBlacklistedLoginForTesting(password_form));

  password_form.origin = GURL("https://rsolomakhin.github.io/autofill/1234");
  password_form.signon_realm = "https://rsolomakhin.github.io/";
  password_form.blacklisted_by_user = true;
  EXPECT_EQ(AddChangeForForm(password_form),
            db().AddBlacklistedLoginForTesting(password_form));

  base::HistogramTester histogram_tester;
  db().ReportMetrics("", false);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccounts.UserCreated.WithoutCustomPassphrase", 9,
      1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountsPerSite.UserCreated.WithoutCustomPassphrase",
      1,
      2);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountsPerSite.UserCreated.WithoutCustomPassphrase", 2,
      3);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.UserCreated.WithoutCustomPassphrase",
      0,
      1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.UserCreated.WithoutCustomPassphrase",
      1,
      1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.UserCreated.WithoutCustomPassphrase",
      3,
      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccounts.AutoGenerated.WithoutCustomPassphrase",
      2,
      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Android", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Ftp", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Http", 5, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Https", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Other", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountsPerSite.AutoGenerated.WithoutCustomPassphrase",
      1, 2);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.AutoGenerated.WithoutCustomPassphrase",
      2,
      1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.AutoGenerated.WithoutCustomPassphrase",
      4,
      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.EmptyUsernames.CountInDatabase",
      3,
      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.EmptyUsernames.WithoutCorrespondingNonempty",
      1,
      1);
  histogram_tester.ExpectUniqueSample("PasswordManager.InaccessiblePasswords",
                                      0, 1);
  histogram_tester.ExpectUniqueSample("PasswordManager.BlacklistedDuplicates",
                                      2, 1);
}

// This test will check that adding a blacklist entry is prevented due to an
// already existing entry.
TEST_F(LoginDatabaseTest, AddBlacklistedDuplicates) {
  PasswordForm password_form;
  password_form.origin = GURL("http://rsolomakhin.github.io/autofill/");
  password_form.signon_realm = "http://rsolomakhin.github.io/";
  password_form.blacklisted_by_user = true;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  PasswordForm password_form_duplicated;
  password_form_duplicated.origin =
      GURL("http://rsolomakhin.github.io/autofill/123");
  password_form_duplicated.signon_realm = "http://rsolomakhin.github.io/";
  password_form_duplicated.blacklisted_by_user = true;
  EXPECT_EQ(PasswordStoreChangeList(), db().AddLogin(password_form_duplicated));

  PasswordForm password_form_example;
  password_form_example.origin = GURL("http://example.com/");
  password_form_example.signon_realm = "http://example.com/";
  password_form_example.blacklisted_by_user = false;
  EXPECT_EQ(AddChangeForForm(password_form_example),
            db().AddLogin(password_form_example));

  PasswordForm password_form_example_blacklisted;
  password_form_example_blacklisted.origin = GURL("http://example.com/1");
  password_form_example_blacklisted.signon_realm = "http://example.com/";
  password_form_example_blacklisted.blacklisted_by_user = true;
  EXPECT_EQ(AddChangeForForm(password_form_example_blacklisted),
            db().AddLogin(password_form_example_blacklisted));

  PasswordForm password_form_example_blacklisted_duplicated;
  password_form_example_blacklisted_duplicated.origin =
      GURL("http://example.com/123");
  password_form_example_blacklisted_duplicated.signon_realm =
      "http://example.com/";
  password_form_example_blacklisted_duplicated.blacklisted_by_user = true;
  EXPECT_EQ(PasswordStoreChangeList(),
            db().AddLogin(password_form_example_blacklisted_duplicated));

  std::vector<std::unique_ptr<PasswordForm>> forms;
  ASSERT_TRUE(db().GetAutofillableLogins(&forms));
  EXPECT_THAT(forms,
              UnorderedElementsAre(::testing::Pointee(password_form_example)));

  std::vector<std::unique_ptr<PasswordForm>> blacklisted_forms;
  ASSERT_TRUE(db().GetBlacklistLogins(&blacklisted_forms));
  EXPECT_THAT(blacklisted_forms,
              UnorderedElementsAre(
                  ::testing::Pointee(password_form),
                  ::testing::Pointee(password_form_example_blacklisted)));
}

TEST_F(LoginDatabaseTest, PasswordReuseMetrics) {
  // -- Group of accounts that are reusing password #1.
  //
  //                                     Destination account
  // +-----------------+-------+-------+-------+-------+-------+-------+-------+
  // |                 |   1   |   2   |   3   |   4   |   5   |   6   |   7   |
  // +-----------------+-------+-------+-------+-------+-------+-------+-------+
  // | Scheme?         | HTTP  | HTTP  | HTTP  | HTTP  | HTTPS | HTTPS | HTTPS |
  // +-----------------+-------+-------+-------+-------+-------+-------+-------+
  // |           |  1  |   -   | Same  |  PSL  | Diff. | Same  | Diff. | Diff. |
  // |           |  2  | Same  |   -   |  PSL  | Diff. | Same  | Diff. | Diff. |
  // | Relation  |  3  |  PSL  |  PSL  |   -   | Diff. | Diff. | Same  | Diff. |
  // | to host   |  4  | Diff. | Diff. | Diff. |   -   | Diff. | Diff. | Same  |
  // | of source |  5  | Same  | Same  | Diff. | Diff. |   -   |  PSL  | Diff. |
  // | account:  |  6  | Diff. | Diff. | Same  | Diff. |  PSL  |   -   | Diff. |
  // |           |  7  | Diff. | Diff. | Diff. | Same  | Diff. | Diff. |   -   |
  // +-----------------+-------+-------+-------+-------+-------+-------+-------+

  PasswordForm password_form;
  password_form.signon_realm = "http://example.com/";
  password_form.origin = GURL("http://example.com/");
  password_form.username_value = ASCIIToUTF16("username_1");
  password_form.password_value = ASCIIToUTF16("password_1");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.origin = GURL("http://example.com/");
  password_form.username_value = ASCIIToUTF16("username_2");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  // Note: This PSL matches http://example.com, but not https://example.com.
  password_form.signon_realm = "http://www.example.com/";
  password_form.origin = GURL("http://www.example.com/");
  password_form.username_value = ASCIIToUTF16("username_3");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.signon_realm = "http://not-example.com/";
  password_form.origin = GURL("http://not-example.com/");
  password_form.username_value = ASCIIToUTF16("username_4");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.signon_realm = "https://example.com/";
  password_form.origin = GURL("https://example.com/");
  password_form.username_value = ASCIIToUTF16("username_5");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  // Note: This PSL matches https://example.com, but not http://example.com.
  password_form.signon_realm = "https://www.example.com/";
  password_form.origin = GURL("https://www.example.com/");
  password_form.username_value = ASCIIToUTF16("username_6");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.signon_realm = "https://not-example.com/";
  password_form.origin = GURL("https://not-example.com/");
  password_form.username_value = ASCIIToUTF16("username_7");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  // -- Group of accounts that are reusing password #2.
  // Both HTTP, different host.
  password_form.signon_realm = "http://example.com/";
  password_form.origin = GURL("http://example.com/");
  password_form.username_value = ASCIIToUTF16("username_8");
  password_form.password_value = ASCIIToUTF16("password_2");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.signon_realm = "http://not-example.com/";
  password_form.origin = GURL("http://not-example.com/");
  password_form.username_value = ASCIIToUTF16("username_9");
  password_form.password_value = ASCIIToUTF16("password_2");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  // -- Group of accounts that are reusing password #3.
  // HTTP sites identified by different IP addresses, so they should not be
  // considered a public suffix match.
  password_form.signon_realm = "http://1.2.3.4/";
  password_form.origin = GURL("http://1.2.3.4/");
  password_form.username_value = ASCIIToUTF16("username_10");
  password_form.password_value = ASCIIToUTF16("password_3");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.signon_realm = "http://2.2.3.4/";
  password_form.origin = GURL("http://2.2.3.4/");
  password_form.username_value = ASCIIToUTF16("username_11");
  password_form.password_value = ASCIIToUTF16("password_3");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  // -- Not HTML form based logins or blacklisted logins. Should be ignored.
  PasswordForm ignored_form;
  ignored_form.scheme = PasswordForm::SCHEME_HTML;
  ignored_form.signon_realm = "http://example.org/";
  ignored_form.origin = GURL("http://example.org/blacklist");
  ignored_form.blacklisted_by_user = true;
  ignored_form.username_value = ASCIIToUTF16("username_x");
  ignored_form.password_value = ASCIIToUTF16("password_y");
  EXPECT_EQ(AddChangeForForm(ignored_form), db().AddLogin(ignored_form));

  ignored_form.scheme = PasswordForm::SCHEME_BASIC;
  ignored_form.signon_realm = "http://example.org/HTTP Auth Realm";
  ignored_form.origin = GURL("http://example.org/");
  ignored_form.blacklisted_by_user = false;
  EXPECT_EQ(AddChangeForForm(ignored_form), db().AddLogin(ignored_form));

  ignored_form.scheme = PasswordForm::SCHEME_HTML;
  ignored_form.signon_realm = "android://hash@com.example/";
  ignored_form.origin = GURL();
  ignored_form.blacklisted_by_user = false;
  EXPECT_EQ(AddChangeForForm(ignored_form), db().AddLogin(ignored_form));

  ignored_form.scheme = PasswordForm::SCHEME_HTML;
  ignored_form.signon_realm = "federation://example.com/federation.com";
  ignored_form.origin = GURL("https://example.com/");
  ignored_form.blacklisted_by_user = false;
  EXPECT_EQ(AddChangeForForm(ignored_form), db().AddLogin(ignored_form));

  base::HistogramTester histogram_tester;
  db().ReportMetrics("", false);

  const std::string kPrefix("PasswordManager.AccountsReusingPassword.");
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpRealm.OnHttpRealmWithSameHost"),
              testing::ElementsAre(base::Bucket(0, 6), base::Bucket(1, 2)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpRealm.OnHttpsRealmWithSameHost"),
              testing::ElementsAre(base::Bucket(0, 4), base::Bucket(1, 4)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpRealm.OnPSLMatchingRealm"),
              testing::ElementsAre(base::Bucket(0, 5), base::Bucket(1, 2),
                                   base::Bucket(2, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpRealm.OnHttpsRealmWithDifferentHost"),
              testing::ElementsAre(base::Bucket(0, 4), base::Bucket(2, 4)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpRealm.OnHttpRealmWithDifferentHost"),
              testing::ElementsAre(base::Bucket(1, 7), base::Bucket(3, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpRealm.OnAnyRealmWithDifferentHost"),
              testing::ElementsAre(base::Bucket(1, 4), base::Bucket(3, 3),
                                   base::Bucket(5, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpsRealm.OnHttpRealmWithSameHost"),
              testing::ElementsAre(base::Bucket(1, 2), base::Bucket(2, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpsRealm.OnHttpsRealmWithSameHost"),
              testing::ElementsAre(base::Bucket(0, 3)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpsRealm.OnPSLMatchingRealm"),
              testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 2)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpsRealm.OnHttpRealmWithDifferentHost"),
              testing::ElementsAre(base::Bucket(2, 1), base::Bucket(3, 2)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpsRealm.OnHttpsRealmWithDifferentHost"),
              testing::ElementsAre(base::Bucket(1, 2), base::Bucket(2, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpsRealm.OnAnyRealmWithDifferentHost"),
              testing::ElementsAre(base::Bucket(3, 1), base::Bucket(4, 1),
                                   base::Bucket(5, 1)));
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
    LoginDatabase db(file);
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

#if defined(OS_LINUX)
// Test that LoginDatabase does not encrypt values when encryption is disabled.
// TODO(crbug.com/829857) This is supported only for Linux, while transitioning
// into LoginDB with full encryption.
TEST_F(LoginDatabaseTest, EncryptionDisabled) {
  PasswordForm password_form;
  GenerateExamplePasswordForm(&password_form);
  base::FilePath file = temp_dir_.GetPath().AppendASCII("TestUnencryptedDB");
  {
    LoginDatabase db(file);
    db.disable_encryption();
    ASSERT_TRUE(db.Init());
    EXPECT_EQ(AddChangeForForm(password_form), db.AddLogin(password_form));
  }
  EXPECT_EQ(
      GetColumnValuesFromDatabase<std::string>(file, "password_value").at(0),
      base::UTF16ToUTF8(password_form.password_value));
}
#endif  // defined(OS_LINUX)

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
                                kCompatibleVersionNumber + 1));
  }

  // Now try to init the database with the file. The test succeeds if it does
  // not crash.
  LoginDatabase db(database_path);
  EXPECT_FALSE(db.Init());
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
  base::test::ScopedTaskEnvironment scoped_task_environment_;
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
    LoginDatabase db(database_path_);
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
    EXPECT_TRUE(db.RemoveLogin(form));
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

class LoginDatabaseMigrationTestV9 : public LoginDatabaseMigrationTest {
};

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

INSTANTIATE_TEST_CASE_P(MigrationToVCurrent,
                        LoginDatabaseMigrationTest,
                        testing::Range(1, kCurrentVersionNumber + 1));
INSTANTIATE_TEST_CASE_P(MigrationToVCurrent,
                        LoginDatabaseMigrationTestV9,
                        testing::Values(9));
INSTANTIATE_TEST_CASE_P(MigrationToVCurrent,
                        LoginDatabaseMigrationTestBroken,
                        testing::Range(1, 4));

class LoginDatabaseUndecryptableLoginsTest : public testing::Test {
 protected:
  LoginDatabaseUndecryptableLoginsTest() {}

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

  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

 private:
  base::FilePath database_path_;
  base::ScopedTempDir temp_dir_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  TestingPrefServiceSimple testing_local_state_;

  DISALLOW_COPY_AND_ASSIGN(LoginDatabaseUndecryptableLoginsTest);
};

PasswordForm LoginDatabaseUndecryptableLoginsTest::AddDummyLogin(
    const std::string& unique_string,
    const GURL& origin,
    bool should_be_corrupted) {
  // Create a dummy password form.
  const base::string16 unique_string16 = ASCIIToUTF16(unique_string);
  PasswordForm form;
  form.origin = origin;
  form.username_element = unique_string16;
  form.username_value = unique_string16;
  form.password_element = unique_string16;
  form.password_value = unique_string16;
  form.signon_realm = origin.GetOrigin().spec();

  {
    LoginDatabase db(database_path());
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

  return form;
}

TEST_F(LoginDatabaseUndecryptableLoginsTest, DeleteUndecryptableLoginsTest) {
  // Disable feature for deleting corrupted passwords, so GetAutofillableLogins
  // doesn't remove any passwords.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kDeleteCorruptedPasswords);

  auto form1 = AddDummyLogin("foo1", GURL("https://foo1.com/"), false);
  auto form2 = AddDummyLogin("foo2", GURL("https://foo2.com/"), true);
  auto form3 = AddDummyLogin("foo3", GURL("https://foo3.com/"), false);

  LoginDatabase db(database_path());
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(db.Init());

#if defined(OS_MACOSX) && !defined(OS_IOS)
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
#if defined(OS_MACOSX) && !defined(OS_IOS)
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

#if defined(OS_MACOSX) && !defined(OS_IOS)
TEST_F(LoginDatabaseUndecryptableLoginsTest, PasswordRecoveryEnabledGetLogins) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kDeleteCorruptedPasswords);

  auto form1 = AddDummyLogin("foo1", GURL("https://foo1.com/"), false);
  auto form2 = AddDummyLogin("foo2", GURL("https://foo2.com/"), true);
  auto form3 = AddDummyLogin("foo3", GURL("https://foo3.com/"), false);

  LoginDatabase db(database_path());
  ASSERT_TRUE(db.Init());

  testing_local_state().registry()->RegisterTimePref(prefs::kPasswordRecovery,
                                                     base::Time());
  db.InitPasswordRecoveryUtil(std::make_unique<PasswordRecoveryUtilMac>(
      &testing_local_state(), base::ThreadTaskRunnerHandle::Get()));

  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_TRUE(db.GetAutofillableLogins(&result));
  EXPECT_THAT(result, UnorderedElementsAre(Pointee(form1), Pointee(form3)));

  RunUntilIdle();
  EXPECT_TRUE(testing_local_state().HasPrefPath(prefs::kPasswordRecovery));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.RemovedCorruptedPasswords", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.DeleteCorruptedPasswordsResult",
      metrics_util::DeleteCorruptedPasswordsResult::kSuccessPasswordsDeleted,
      1);
}

TEST_F(LoginDatabaseUndecryptableLoginsTest,
       PasswordRecoveryDisabledGetLogins) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kDeleteCorruptedPasswords);

  AddDummyLogin("foo1", GURL("https://foo1.com/"), false);
  AddDummyLogin("foo2", GURL("https://foo2.com/"), true);

  LoginDatabase db(database_path());
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

  EXPECT_TRUE(histogram_tester
                  .GetAllSamples("PasswordManager.RemovedCorruptedPasswords")
                  .empty());
  EXPECT_TRUE(
      histogram_tester
          .GetAllSamples("PasswordManager.DeleteCorruptedPasswordsResult")
          .empty());
}

TEST_F(LoginDatabaseUndecryptableLoginsTest,
       PasswordRecoveryEnabledKeychainLocked) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kDeleteCorruptedPasswords);

  // This is a valid entry.
  auto form = AddDummyLogin("foo", GURL("https://foo.com/"), false);

  OSCryptMocker::SetBackendLocked(true);

  LoginDatabase db(database_path());
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

  EXPECT_TRUE(histogram_tester
                  .GetAllSamples("PasswordManager.RemovedCorruptedPasswords")
                  .empty());
  EXPECT_TRUE(
      histogram_tester
          .GetAllSamples("PasswordManager.DeleteCorruptedPasswordsResult")
          .empty());

  // Note: it's not possible that encryption suddenly becomes available. This is
  // only used to check that the form is not removed from the database.
  OSCryptMocker::SetBackendLocked(false);

  EXPECT_TRUE(db.GetAutofillableLogins(&result));
  EXPECT_THAT(result, UnorderedElementsAre(Pointee(form)));
}

TEST_F(LoginDatabaseUndecryptableLoginsTest, KeychainLockedTest) {
  AddDummyLogin("foo1", GURL("https://foo1.com/"), false);
  AddDummyLogin("foo2", GURL("https://foo2.com/"), true);

  OSCryptMocker::SetBackendLocked(true);
  LoginDatabase db(database_path());
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
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

}  // namespace password_manager
