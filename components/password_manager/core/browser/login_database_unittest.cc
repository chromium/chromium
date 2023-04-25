// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/login_database.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/features.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

using autofill::GaiaIdHash;
using base::ASCIIToUTF16;
using base::UTF16ToASCII;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Ne;
using ::testing::Pair;
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
                                            bool password_changed) {
  return PasswordStoreChangeList(
      1,
      PasswordStoreChange(PasswordStoreChange::UPDATE, form, password_changed));
}

PasswordStoreChangeList UpdateChangeForForm(const PasswordForm& form,
                                            bool password_changed,
                                            bool insecure_changed) {
  return PasswordStoreChangeList(
      1,
      PasswordStoreChange(PasswordStoreChange::UPDATE, form, password_changed,
                          InsecureCredentialsChanged(insecure_changed)));
}

PasswordStoreChangeList RemoveChangeForForm(const PasswordForm& form) {
  return PasswordStoreChangeList(
      1, PasswordStoreChange(PasswordStoreChange::REMOVE, form));
}

PasswordForm GenerateExamplePasswordForm() {
  PasswordForm form;
  form.url = GURL("http://accounts.google.com/LoginAuth");
  form.action = GURL("http://accounts.google.com/Login");
  form.username_element = u"Email";
  form.username_value = u"test@gmail.com";
  form.password_element = u"Passwd";
  form.password_value = u"test";
  form.submit_element = u"signIn";
  form.signon_realm = "http://www.google.com/";
  form.scheme = PasswordForm::Scheme::kHtml;
  form.times_used_in_html_form = 1;
  form.form_data.name = u"form_name";
  form.date_last_used = base::Time::Now();
  form.date_password_modified = base::Time::Now() - base::Days(1);
  form.display_name = u"Mr. Smith";
  form.icon_url = GURL("https://accounts.google.com/Icon");
  form.skip_zero_click = true;
  form.in_store = PasswordForm::Store::kProfileStore;
  form.moving_blocked_for_list.push_back(GaiaIdHash::FromGaiaId("user1"));
  form.moving_blocked_for_list.push_back(GaiaIdHash::FromGaiaId("user2"));

  return form;
}

// Helper functions to read the value of the first column of an executed
// statement if we know its type. You must implement a specialization for
// every column type you use.
template <class T>
struct must_be_specialized {
  static const bool is_specialized = false;
};

template <class T>
T GetFirstColumn(sql::Statement& s) {
  static_assert(must_be_specialized<T>::is_specialized,
                "Implement a specialization.");
}

template <>
int64_t GetFirstColumn(sql::Statement& s) {
  return s.ColumnInt64(0);
}

template <>
[[maybe_unused]] std::string GetFirstColumn(sql::Statement& s) {
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

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
// Set the new password value for all the rows with the specified username.
void UpdatePasswordValueForUsername(const base::FilePath& database_path,
                                    const std::u16string& username,
                                    const std::u16string& password) {
  sql::Database db;
  CHECK(db.Open(database_path));

  std::string statement =
      "UPDATE logins SET password_value = ? WHERE username_value = ?";
  sql::Statement s(db.GetCachedStatement(SQL_FROM_HERE, statement.c_str()));
  EXPECT_TRUE(s.is_valid());
  s.BindString16(0, password);
  s.BindString16(1, username);

  CHECK(s.Run());
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)

bool AddZeroClickableLogin(LoginDatabase* db,
                           const std::string& unique_string,
                           const GURL& origin) {
  // Example password form.
  PasswordForm form;
  form.url = origin;
  form.username_element = ASCIIToUTF16(unique_string);
  form.username_value = ASCIIToUTF16(unique_string);
  form.password_element = ASCIIToUTF16(unique_string);
  form.submit_element = u"signIn";
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
         arg.username_value == u"theerikchen" &&
         arg.scheme == PasswordForm::Scheme::kHtml;
}

MATCHER(IsGoogle2Account, "") {
  return arg.url.spec() == "https://accounts.google.com/ServiceLogin" &&
         arg.action.spec() == "https://accounts.google.com/ServiceLoginAuth" &&
         arg.username_value == u"theerikchen2" &&
         arg.scheme == PasswordForm::Scheme::kHtml;
}

MATCHER(IsBasicAuthAccount, "") {
  return arg.scheme == PasswordForm::Scheme::kBasic;
}

// Matcher that matches all a password form that has the primary_key field set,
// and that other fields are the same as in |expected_form|.
auto HasPrimaryKeyAndEquals(PasswordForm expected_form) {
  return AllOf(Field(&PasswordForm::primary_key, testing::Optional(_)),
               Eq(expected_form));
}

}  // namespace

// Serialization routines for vectors implemented in login_database.cc.
base::Pickle SerializeAlternativeElementVector(
    const AlternativeElementVector& vector);
AlternativeElementVector DeserializeAlternativeElementVector(
    const base::Pickle& pickle);
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

  base::ScopedTempDir temp_dir_;
  base::FilePath file_;
  std::unique_ptr<LoginDatabase> db_;
  // A full TaskEnvironment is required instead of only
  // SingleThreadTaskEnvironment because on iOS,
  // password_manager::DeletePasswordsDirectory() which calls
  // base::ThreadPool::PostTask().
  base::test::TaskEnvironment task_environment_;
};

TEST_F(LoginDatabaseTest, GetAllLogins) {
  // Example password form.
  PasswordForm form = GenerateExamplePasswordForm();
  ASSERT_EQ(AddChangeForForm(form), db().AddLogin(form));
  PasswordForm blocklisted;
  blocklisted.signon_realm = "http://example3.com/";
  blocklisted.url = GURL("http://example3.com/path");
  blocklisted.blocked_by_user = true;
  blocklisted.in_store = PasswordForm::Store::kProfileStore;
  ASSERT_EQ(AddChangeForForm(blocklisted), db().AddLogin(blocklisted));

  std::vector<std::unique_ptr<PasswordForm>> forms;
  EXPECT_EQ(db().GetAllLogins(&forms), FormRetrievalResult::kSuccess);
  EXPECT_THAT(forms, UnorderedElementsAre(
                         Pointee(HasPrimaryKeyAndEquals(form)),
                         Pointee(HasPrimaryKeyAndEquals(blocklisted))));
}

TEST_F(LoginDatabaseTest, GetLogins_Self) {
  PasswordForm form = GenerateExamplePasswordForm();
  ASSERT_EQ(AddChangeForForm(form), db().AddLogin(form));

  // Match against an exact copy.
  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form),
                             /*should_PSL_matching_apply=*/false, &result));
  EXPECT_THAT(result, ElementsAre(Pointee(HasPrimaryKeyAndEquals(form))));
}

TEST_F(LoginDatabaseTest, GetLogins_InexactCopy) {
  PasswordForm form = GenerateExamplePasswordForm();
  ASSERT_EQ(AddChangeForForm(form), db().AddLogin(form));

  PasswordFormDigest digest(
      PasswordForm::Scheme::kHtml, "http://www.google.com/",
      GURL("http://www.google.com/new/accounts/LoginAuth"));

  // Match against an inexact copy
  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_TRUE(
      db().GetLogins(digest, /*should_PSL_matching_apply=*/true, &result));
  EXPECT_THAT(result, ElementsAre(Pointee(HasPrimaryKeyAndEquals(form))));
}

TEST_F(LoginDatabaseTest, GetLogins_ProtocolMismatch_HTTP) {
  PasswordForm form = GenerateExamplePasswordForm();
  ASSERT_TRUE(base::StartsWith(form.signon_realm, "http://"));
  ASSERT_EQ(AddChangeForForm(form), db().AddLogin(form));

  PasswordFormDigest digest(
      PasswordForm::Scheme::kHtml, "https://www.google.com/",
      GURL("https://www.google.com/new/accounts/LoginAuth"));

  // We have only an http record, so no match for this.
  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_TRUE(
      db().GetLogins(digest, /*should_PSL_matching_apply=*/true, &result));
  EXPECT_THAT(result, IsEmpty());
}

TEST_F(LoginDatabaseTest, GetLogins_ProtocolMismatch_HTTPS) {
  PasswordForm form = GenerateExamplePasswordForm();
  form.url = GURL("https://accounts.google.com/LoginAuth");
  form.signon_realm = "https://accounts.google.com/";
  ASSERT_EQ(AddChangeForForm(form), db().AddLogin(form));

  PasswordFormDigest digest(
      PasswordForm::Scheme::kHtml, "http://accounts.google.com/",
      GURL("http://accounts.google.com/new/accounts/LoginAuth"));

  // We have only an https record, so no match for this.
  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_TRUE(
      db().GetLogins(digest, /*should_PSL_matching_apply=*/true, &result));
  EXPECT_THAT(result, IsEmpty());
}

TEST_F(LoginDatabaseTest, AddLoginReturnsPrimaryKey) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form = GenerateExamplePasswordForm();

  // Add it and make sure the primary key is returned in the
  // PasswordStoreChange.
  PasswordStoreChangeList change_list = db().AddLogin(form);
  ASSERT_EQ(1U, change_list.size());
  EXPECT_EQ(AddChangeForForm(form), change_list);
  EXPECT_EQ(1, change_list[0].form().primary_key.value().value());
}

TEST_F(LoginDatabaseTest, RemoveLoginsByPrimaryKey) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form = GenerateExamplePasswordForm();

  // Add it and make sure it is there and that all the fields were retrieved
  // correctly.
  PasswordStoreChangeList change_list = db().AddLogin(form);
  ASSERT_EQ(1U, change_list.size());
  FormPrimaryKey primary_key = change_list[0].form().primary_key.value();
  EXPECT_EQ(AddChangeForForm(form), change_list);
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(1U, result.size());
  EXPECT_THAT(result[0], Pointee(HasPrimaryKeyAndEquals(form)));
  result.clear();

  // RemoveLoginByPrimaryKey() doesn't decrypt or fill the password value.
  form.password_value = u"";

  EXPECT_TRUE(db().RemoveLoginByPrimaryKey(primary_key, &change_list));
  EXPECT_EQ(RemoveChangeForForm(form), change_list);
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());
}

TEST_F(LoginDatabaseTest, ShouldNotRecyclePrimaryKeys) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Example password form.
  PasswordForm form = GenerateExamplePasswordForm();

  // Add the form.
  PasswordStoreChangeList change_list = db().AddLogin(form);
  ASSERT_EQ(1U, change_list.size());
  FormPrimaryKey primary_key1 = change_list[0].form().primary_key.value();
  change_list.clear();
  // Delete the form
  EXPECT_TRUE(db().RemoveLoginByPrimaryKey(primary_key1, &change_list));
  ASSERT_EQ(1U, change_list.size());
  // Add it again.
  change_list = db().AddLogin(form);
  ASSERT_EQ(1U, change_list.size());
  EXPECT_NE(primary_key1, change_list[0].form().primary_key.value());
}

TEST_F(LoginDatabaseTest, TestPublicSuffixDomainMatching) {
  // Example password form.
  PasswordForm form;
  form.url = GURL("https://foo.com/");
  form.action = GURL("https://foo.com/login");
  form.username_element = u"username";
  form.username_value = u"test@gmail.com";
  form.password_element = u"password";
  form.password_value = u"test";
  form.submit_element = u"";
  form.signon_realm = "https://foo.com/";
  form.scheme = PasswordForm::Scheme::kHtml;

  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  // We go to the mobile site.
  PasswordFormDigest form2(PasswordForm::Scheme::kHtml,
                           "https://mobile.foo.com/",
                           GURL("https://mobile.foo.com/login"));

  // Match against the mobile site.
  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form2),
                             /*should_PSL_matching_apply=*/true, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ("https://foo.com/", result[0]->signon_realm);
  EXPECT_TRUE(result[0]->is_public_suffix_match);

  // Do an exact match by excluding psl matches.
  result.clear();
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form2),
                             /*should_PSL_matching_apply=*/false, &result));
  EXPECT_THAT(result, IsEmpty());
}

TEST_F(LoginDatabaseTest, TestFederatedMatching) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Example password form.
  PasswordForm form;
  form.url = GURL("https://foo.com/");
  form.action = GURL("https://foo.com/login");
  form.username_value = u"test@gmail.com";
  form.password_value = u"test";
  form.signon_realm = "https://foo.com/";
  form.scheme = PasswordForm::Scheme::kHtml;

  // We go to the mobile site.
  PasswordForm form2(form);
  form2.url = GURL("https://mobile.foo.com/");
  form2.action = GURL("https://mobile.foo.com/login");
  form2.signon_realm = "federation://mobile.foo.com/accounts.google.com";
  form2.username_value = u"test1@gmail.com";
  form2.password_value = u"";
  form2.type = PasswordForm::Type::kApi;
  form2.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_EQ(AddChangeForForm(form2), db().AddLogin(form2));

  // When we retrieve the forms from the store, |in_store| should be set.
  form.in_store = PasswordForm::Store::kProfileStore;
  form2.in_store = PasswordForm::Store::kProfileStore;

  // Match against desktop.
  PasswordFormDigest form_request = {PasswordForm::Scheme::kHtml,
                                     "https://foo.com/",
                                     GURL("https://foo.com/")};
  EXPECT_TRUE(db().GetLogins(form_request, /*should_PSL_matching_apply=*/true,
                             &result));
  // Both forms are matched, only form2 is a PSL match.
  form.is_public_suffix_match = false;
  form2.is_public_suffix_match = true;
  EXPECT_THAT(result,
              UnorderedElementsAre(Pointee(HasPrimaryKeyAndEquals(form)),
                                   Pointee(HasPrimaryKeyAndEquals(form2))));

  // Match against the mobile site.
  form_request.url = GURL("https://mobile.foo.com/");
  form_request.signon_realm = "https://mobile.foo.com/";
  EXPECT_TRUE(db().GetLogins(form_request, /*should_PSL_matching_apply=*/true,
                             &result));
  // Both forms are matched, only form is a PSL match.
  form.is_public_suffix_match = true;
  form2.is_public_suffix_match = false;
  EXPECT_THAT(result,
              UnorderedElementsAre(Pointee(HasPrimaryKeyAndEquals(form)),
                                   Pointee(HasPrimaryKeyAndEquals(form2))));
}

TEST_F(LoginDatabaseTest, TestFederatedMatchingLocalhost) {
  PasswordForm form;
  form.url = GURL("http://localhost/");
  form.signon_realm = "federation://localhost/accounts.google.com";
  form.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));
  form.username_value = u"test@gmail.com";
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
  PasswordFormDigest form_request(PasswordForm::Scheme::kHtml,
                                  "http://localhost/",
                                  GURL("http://localhost/"));
  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_TRUE(db().GetLogins(form_request, /*should_PSL_matching_apply=*/false,
                             &result));
  EXPECT_THAT(result,
              UnorderedElementsAre(Pointee(HasPrimaryKeyAndEquals(form))));

  form_request.url = GURL("http://localhost:8080/");
  form_request.signon_realm = "http://localhost:8080/";
  EXPECT_TRUE(db().GetLogins(form_request, /*should_PSL_matching_apply=*/false,
                             &result));
  EXPECT_THAT(result, UnorderedElementsAre(
                          Pointee(HasPrimaryKeyAndEquals(form_with_port))));
}

class LoginDatabaseSchemesTest
    : public LoginDatabaseTest,
      public testing::WithParamInterface<PasswordForm::Scheme> {};

TEST_P(LoginDatabaseSchemesTest, TestPublicSuffixDisabled) {
  // The test is based on the different treatment for kHtml vs. non kHtml
  // schemes.
  if (GetParam() == PasswordForm::Scheme::kHtml)
    return;
  // Simple non-html auth form.
  PasswordForm non_html_auth;
  non_html_auth.in_store = PasswordForm::Store::kProfileStore;
  non_html_auth.url = GURL("http://example.com");
  non_html_auth.username_value = u"test@gmail.com";
  non_html_auth.password_value = u"test";
  non_html_auth.signon_realm = "http://example.com/Realm";
  non_html_auth.scheme = GetParam();

  // Simple password form.
  PasswordForm html_form(non_html_auth);
  html_form.in_store = PasswordForm::Store::kProfileStore;
  html_form.username_value = u"test2@gmail.com";
  html_form.password_element = u"password";
  html_form.signon_realm = "http://example.com/";
  html_form.scheme = PasswordForm::Scheme::kHtml;

  EXPECT_EQ(AddChangeForForm(non_html_auth), db().AddLogin(non_html_auth));
  EXPECT_EQ(AddChangeForForm(html_form), db().AddLogin(html_form));

  PasswordFormDigest second_non_html_auth = {GetParam(),
                                             "http://second.example.com/Realm",
                                             GURL("http://second.example.com")};

  // This shouldn't match anything.
  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_TRUE(db().GetLogins(second_non_html_auth,
                             /*should_PSL_matching_apply=*/false, &result));
  EXPECT_EQ(0U, result.size());

  // non-html auth still matches against itself.
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(non_html_auth),
                             /*should_PSL_matching_apply=*/false, &result));
  EXPECT_THAT(result,
              ElementsAre(Pointee(HasPrimaryKeyAndEquals(non_html_auth))));
}

TEST_P(LoginDatabaseSchemesTest, TestIPAddressMatches) {
  std::string origin("http://56.7.8.90/");

  PasswordForm ip_form;
  ip_form.in_store = PasswordForm::Store::kProfileStore;
  ip_form.url = GURL(origin);
  ip_form.username_value = u"test@gmail.com";
  ip_form.password_value = u"test";
  ip_form.signon_realm = origin;
  ip_form.scheme = GetParam();

  EXPECT_EQ(AddChangeForForm(ip_form), db().AddLogin(ip_form));
  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(ip_form),
                             /*should_PSL_matching_apply=*/false, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_THAT(result[0], Pointee(HasPrimaryKeyAndEquals(ip_form)));
}

INSTANTIATE_TEST_SUITE_P(Schemes,
                         LoginDatabaseSchemesTest,
                         testing::Values(PasswordForm::Scheme::kHtml,
                                         PasswordForm::Scheme::kBasic,
                                         PasswordForm::Scheme::kDigest,
                                         PasswordForm::Scheme::kOther));

TEST_F(LoginDatabaseTest, TestPublicSuffixDomainGoogle) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Saved password form on Google sign-in page.
  PasswordForm form;
  form.url = GURL("https://accounts.google.com/");
  form.username_value = u"test@gmail.com";
  form.password_value = u"test";
  form.signon_realm = "https://accounts.google.com/";
  form.scheme = PasswordForm::Scheme::kHtml;

  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  // Google change password should match to the saved sign-in form.
  PasswordFormDigest form2 = {PasswordForm::Scheme::kHtml,
                              "https://myaccount.google.com/",
                              GURL("https://myaccount.google.com/")};

  EXPECT_TRUE(
      db().GetLogins(form2, /*should_PSL_matching_apply=*/true, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form.signon_realm, result[0]->signon_realm);
  EXPECT_TRUE(result[0]->is_public_suffix_match);

  // There should be no PSL match on other subdomains.
  PasswordFormDigest form3 = {PasswordForm::Scheme::kHtml,
                              "https://some.other.google.com/",
                              GURL("https://some.other.google.com/")};

  EXPECT_TRUE(
      db().GetLogins(form3, /*should_PSL_matching_apply=*/true, &result));
  EXPECT_THAT(result, IsEmpty());
}

TEST_F(LoginDatabaseTest, TestFederatedMatchingWithoutPSLMatching) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Example password form.
  PasswordForm form;
  form.url = GURL("https://accounts.google.com/");
  form.action = GURL("https://accounts.google.com/login");
  form.username_value = u"test@gmail.com";
  form.password_value = u"test";
  form.signon_realm = "https://accounts.google.com/";
  form.scheme = PasswordForm::Scheme::kHtml;

  // We go to a different site on the same domain where PSL is disabled.
  PasswordForm form2(form);
  form2.url = GURL("https://some.other.google.com/");
  form2.action = GURL("https://some.other.google.com/login");
  form2.signon_realm = "federation://some.other.google.com/accounts.google.com";
  form2.username_value = u"test1@gmail.com";
  form2.type = PasswordForm::Type::kApi;
  form2.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));

  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_EQ(AddChangeForForm(form2), db().AddLogin(form2));

  // When we retrieve the forms from the store, |in_store| should be set.
  form.in_store = PasswordForm::Store::kProfileStore;
  form2.in_store = PasswordForm::Store::kProfileStore;

  // Match against the first one.
  PasswordFormDigest form_request = {PasswordForm::Scheme::kHtml,
                                     form.signon_realm, form.url};
  EXPECT_TRUE(db().GetLogins(form_request, /*should_PSL_matching_apply=*/false,
                             &result));
  EXPECT_THAT(result, ElementsAre(Pointee(HasPrimaryKeyAndEquals(form))));

  // Match against the second one.
  form_request.url = form2.url;
  form_request.signon_realm = form2.signon_realm;
  EXPECT_TRUE(db().GetLogins(form_request, /*should_PSL_matching_apply=*/false,
                             &result));
  form.is_public_suffix_match = true;
  EXPECT_THAT(result, ElementsAre(Pointee(HasPrimaryKeyAndEquals(form2))));
}

TEST_F(LoginDatabaseTest, TestFederatedPSLMatching) {
  // Save a federated credential for the PSL matched site.
  PasswordForm form;
  form.url = GURL("https://psl.example.com/");
  form.action = GURL("https://psl.example.com/login");
  form.signon_realm = "federation://psl.example.com/accounts.google.com";
  form.username_value = u"test1@gmail.com";
  form.type = PasswordForm::Type::kApi;
  form.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));
  form.scheme = PasswordForm::Scheme::kHtml;
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  // When we retrieve the form from the store, it should have |in_store| set.
  form.in_store = PasswordForm::Store::kProfileStore;

  // Match against.
  PasswordFormDigest form_request = {PasswordForm::Scheme::kHtml,
                                     "https://example.com/",
                                     GURL("https://example.com/login")};
  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_TRUE(db().GetLogins(form_request, /*should_PSL_matching_apply=*/true,
                             &result));
  form.is_public_suffix_match = true;
  EXPECT_THAT(result, ElementsAre(Pointee(HasPrimaryKeyAndEquals(form))));
}

// This test fails if the implementation of GetLogins uses GetCachedStatement
// instead of GetUniqueStatement, since REGEXP is in use. See
// http://crbug.com/248608.
TEST_F(LoginDatabaseTest, TestPublicSuffixDomainMatchingDifferentSites) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Example password form.
  PasswordForm form;
  form.url = GURL("https://foo.com/");
  form.action = GURL("https://foo.com/login");
  form.username_element = u"username";
  form.username_value = u"test@gmail.com";
  form.password_element = u"password";
  form.password_value = u"test";
  form.submit_element = u"";
  form.signon_realm = "https://foo.com/";
  form.scheme = PasswordForm::Scheme::kHtml;

  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  // We go to the mobile site.
  PasswordFormDigest form2(form);
  form2.url = GURL("https://mobile.foo.com/");
  form2.signon_realm = "https://mobile.foo.com/";

  // Match against the mobile site.
  EXPECT_TRUE(
      db().GetLogins(form2, /*should_PSL_matching_apply=*/true, &result));
  EXPECT_EQ(1U, result.size());
  EXPECT_EQ("https://foo.com/", result[0]->signon_realm);
  EXPECT_TRUE(result[0]->is_public_suffix_match);
  result.clear();

  // Add baz.com desktop site.
  form.url = GURL("https://baz.com/login/");
  form.action = GURL("https://baz.com/login/");
  form.username_element = u"email";
  form.username_value = u"test@gmail.com";
  form.password_element = u"password";
  form.password_value = u"test";
  form.submit_element = u"";
  form.signon_realm = "https://baz.com/";
  form.scheme = PasswordForm::Scheme::kHtml;

  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  // We go to the mobile site of baz.com.
  PasswordFormDigest form3(form);
  form3.url = GURL("https://m.baz.com/login/");
  form3.signon_realm = "https://m.baz.com/";

  // Match against the mobile site of baz.com.
  EXPECT_TRUE(
      db().GetLogins(form3, /*should_PSL_matching_apply=*/true, &result));
  EXPECT_EQ(1U, result.size());
  EXPECT_EQ("https://baz.com/", result[0]->signon_realm);
  EXPECT_TRUE(result[0]->is_public_suffix_match);
}

PasswordForm GetFormWithNewSignonRealm(PasswordForm form,
                                       const std::string& signon_realm) {
  PasswordForm form2(form);
  form2.url = GURL(signon_realm);
  form2.action = GURL(signon_realm);
  form2.signon_realm = signon_realm;
  return form2;
}

TEST_F(LoginDatabaseTest, TestPublicSuffixDomainMatchingRegexp) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Example password form.
  PasswordForm form;
  form.url = GURL("http://foo.com/");
  form.action = GURL("http://foo.com/login");
  form.username_element = u"username";
  form.username_value = u"test@gmail.com";
  form.password_element = u"password";
  form.password_value = u"test";
  form.submit_element = u"";
  form.signon_realm = "http://foo.com/";
  form.scheme = PasswordForm::Scheme::kHtml;

  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  // Example password form that has - in the domain name.
  PasswordForm form_dash =
      GetFormWithNewSignonRealm(form, "http://www.foo-bar.com/");

  EXPECT_EQ(AddChangeForForm(form_dash), db().AddLogin(form_dash));

  // www.foo.com should match.
  PasswordForm form2 = GetFormWithNewSignonRealm(form, "http://www.foo.com/");
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form2),
                             /*should_PSL_matching_apply=*/true, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // a.b.foo.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://a.b.foo.com/");
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form2),
                             /*should_PSL_matching_apply=*/true, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // a-b.foo.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://a-b.foo.com/");
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form2),
                             /*should_PSL_matching_apply=*/true, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // foo-bar.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://foo-bar.com/");
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form2),
                             /*should_PSL_matching_apply=*/true, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // www.foo-bar.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://www.foo-bar.com/");
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form2),
                             /*should_PSL_matching_apply=*/true, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // a.b.foo-bar.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://a.b.foo-bar.com/");
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form2),
                             /*should_PSL_matching_apply=*/true, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // a-b.foo-bar.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://a-b.foo-bar.com/");
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form2),
                             /*should_PSL_matching_apply=*/true, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // foo.com with port 1337 should not match.
  form2 = GetFormWithNewSignonRealm(form, "http://foo.com:1337/");
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form2),
                             /*should_PSL_matching_apply=*/true, &result));
  EXPECT_EQ(0U, result.size());

  // http://foo.com should not match since the scheme is wrong.
  form2 = GetFormWithNewSignonRealm(form, "https://foo.com/");
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form2),
                             /*should_PSL_matching_apply=*/true, &result));
  EXPECT_EQ(0U, result.size());

  // notfoo.com should not match.
  form2 = GetFormWithNewSignonRealm(form, "http://notfoo.com/");
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form2),
                             /*should_PSL_matching_apply=*/true, &result));
  EXPECT_EQ(0U, result.size());

  // baz.com should not match.
  form2 = GetFormWithNewSignonRealm(form, "http://baz.com/");
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form2),
                             /*should_PSL_matching_apply=*/true, &result));
  EXPECT_EQ(0U, result.size());

  // foo-baz.com should not match.
  form2 = GetFormWithNewSignonRealm(form, "http://foo-baz.com/");
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form2),
                             /*should_PSL_matching_apply=*/true, &result));
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
  form.submit_element = u"signIn";
  form.signon_realm = url;
  form.display_name = ASCIIToUTF16(unique_string);
  form.icon_url = GURL("https://accounts.google.com/Icon");
  form.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));
  form.skip_zero_click = true;

  if (date_is_creation)
    form.date_created = time;
  return db->AddLogin(form) == AddChangeForForm(form);
}

TEST_F(LoginDatabaseTest, ClearPrivateData_SavedPasswords) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  base::Time now = base::Time::Now();
  base::TimeDelta one_day = base::Days(1);
  base::Time back_30_days = now - base::Days(30);
  base::Time back_31_days = now - base::Days(31);

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
  std::vector<std::unique_ptr<PasswordForm>> forms;
  EXPECT_TRUE(db().GetLoginsCreatedBetween(now, base::Time(), &forms));
  EXPECT_EQ(2U, forms.size());
  forms.clear();

  // Get all logins created more than 30 days back.
  EXPECT_TRUE(db().GetLoginsCreatedBetween(base::Time(), back_30_days, &forms));
  EXPECT_EQ(2U, forms.size());
  forms.clear();

  // Delete everything from today's date and on.
  PasswordStoreChangeList changes;
  db().RemoveLoginsCreatedBetween(now, base::Time(), &changes);
  ASSERT_EQ(2U, changes.size());
  // The 3rd and the 4th should have been deleted.
  EXPECT_EQ(3, changes[0].form().primary_key.value().value());
  EXPECT_EQ(4, changes[1].form().primary_key.value().value());

  // Should have deleted two logins.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(3U, result.size());
  result.clear();

  // Delete all logins created more than 30 days back.
  db().RemoveLoginsCreatedBetween(base::Time(), back_30_days, &changes);
  ASSERT_EQ(2U, changes.size());
  // The 1st and the 5th should have been deleted.
  EXPECT_EQ(1, changes[0].form().primary_key.value().value());
  EXPECT_EQ(5, changes[1].form().primary_key.value().value());

  // Should have deleted two logins.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // Delete with 0 date (should delete all).
  db().RemoveLoginsCreatedBetween(base::Time(), base::Time(), &changes);
  ASSERT_EQ(1U, changes.size());
  // The 2nd should have been deleted.
  EXPECT_EQ(2, changes[0].form().primary_key.value().value());

  // Verify nothing is left.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());
}

TEST_F(LoginDatabaseTest, GetAutoSignInLogins) {
  std::vector<std::unique_ptr<PasswordForm>> forms;

  GURL origin("https://example.com");
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo1", origin));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo2", origin));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo3", origin));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo4", origin));

  EXPECT_TRUE(db().GetAutoSignInLogins(&forms));
  EXPECT_EQ(4U, forms.size());
  for (const auto& form : forms)
    EXPECT_FALSE(form->skip_zero_click);

  EXPECT_TRUE(db().DisableAutoSignInForOrigin(origin));
  EXPECT_TRUE(db().GetAutoSignInLogins(&forms));
  EXPECT_EQ(0U, forms.size());
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

TEST_F(LoginDatabaseTest, BlocklistedLogins) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetBlocklistLogins(&result));
  ASSERT_EQ(0U, result.size());

  // Save a form as blocklisted.
  PasswordForm form;
  form.url = GURL("http://accounts.google.com/LoginAuth");
  form.action = GURL("http://accounts.google.com/Login");
  form.username_element = u"Email";
  form.password_element = u"Passwd";
  form.submit_element = u"signIn";
  form.signon_realm = "http://www.google.com/";
  form.blocked_by_user = true;
  form.scheme = PasswordForm::Scheme::kHtml;
  form.display_name = u"Mr. Smith";
  form.icon_url = GURL("https://accounts.google.com/Icon");
  form.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));
  form.skip_zero_click = true;

  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  // Get all non-blocklisted logins (should be none).
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(0U, result.size());

  // When we retrieve the form from the store, it should have |in_store| set.
  form.in_store = PasswordForm::Store::kProfileStore;

  // GetLogins should give the blocklisted result.
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form),
                             /*should_PSL_matching_apply=*/true, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_THAT(result[0], Pointee(HasPrimaryKeyAndEquals(form)));
  result.clear();

  // So should GetBlocklistedLogins.
  EXPECT_TRUE(db().GetBlocklistLogins(&result));
  ASSERT_EQ(1U, result.size());
  EXPECT_THAT(result[0], Pointee(HasPrimaryKeyAndEquals(form)));
  result.clear();
}

TEST_F(LoginDatabaseTest, VectorSerialization) {
  // Empty vector.
  AlternativeElementVector vec;
  base::Pickle temp = SerializeAlternativeElementVector(vec);
  AlternativeElementVector output = DeserializeAlternativeElementVector(temp);
  EXPECT_THAT(output, Eq(vec));

  // Normal data.
  vec.emplace_back(AlternativeElement::Value(u"first"),
                   autofill::FieldRendererId(1),
                   AlternativeElement::Name(u"id1"));
  vec.emplace_back(AlternativeElement::Value(u"second"),
                   autofill::FieldRendererId(2),
                   AlternativeElement::Name(u"id2"));
  vec.emplace_back(AlternativeElement::Value(u"third"),
                   autofill::FieldRendererId(3),
                   AlternativeElement::Name(u"id3"));

  // Field renderer id is a transient field for login database and we
  // expect it will be erased during serialisation+deserialisation process.
  AlternativeElementVector expected;
  expected.emplace_back(AlternativeElement::Value(u"first"),
                        autofill::FieldRendererId(),
                        AlternativeElement::Name(u"id1"));
  expected.emplace_back(AlternativeElement::Value(u"second"),
                        autofill::FieldRendererId(),
                        AlternativeElement::Name(u"id2"));
  expected.emplace_back(AlternativeElement::Value(u"third"),
                        autofill::FieldRendererId(),
                        AlternativeElement::Name(u"id3"));

  temp = SerializeAlternativeElementVector(vec);
  output = DeserializeAlternativeElementVector(temp);
  EXPECT_THAT(output, Eq(expected));
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
  incomplete_form.username_value = u"my_username";
  incomplete_form.password_value = u"my_password";
  incomplete_form.blocked_by_user = false;
  incomplete_form.scheme = PasswordForm::Scheme::kHtml;
  EXPECT_EQ(AddChangeForForm(incomplete_form), db().AddLogin(incomplete_form));

  // A form on some website. It should trigger a match with the stored one.
  PasswordForm encountered_form;
  encountered_form.url = GURL("http://accounts.google.com/LoginAuth");
  encountered_form.signon_realm = "http://accounts.google.com/";
  encountered_form.action = GURL("http://accounts.google.com/Login");
  encountered_form.username_element = u"Email";
  encountered_form.password_element = u"Passwd";
  encountered_form.submit_element = u"signIn";

  // Get matches for encountered_form.
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(encountered_form),
                             /*should_PSL_matching_apply=*/true, &result));
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
  completed_form.date_last_used = base::Time::Now();
  EXPECT_EQ(AddChangeForForm(completed_form), db().AddLogin(completed_form));
  EXPECT_TRUE(db().RemoveLogin(incomplete_form, /*changes=*/nullptr));

  // Get matches for encountered_form again.
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(encountered_form),
                             /*should_PSL_matching_apply=*/true, &result));
  ASSERT_EQ(1U, result.size());

  // This time we should have all the info available.
  PasswordForm expected_form(completed_form);
  // When we retrieve the form from the store, it should have |in_store| set.
  expected_form.in_store = PasswordForm::Store::kProfileStore;
  // And |password_issues| should be empty.
  // TODO(crbug.com/1223022): Once all places that operate changes on forms
  // via UpdateLogin properly set |password_issues|, setting them to an empty
  // map should be part of the default constructor.
  expected_form.password_issues =
      base::flat_map<InsecureType, InsecurityMetadata>();
  EXPECT_THAT(result[0], Pointee(HasPrimaryKeyAndEquals(expected_form)));
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
  incomplete_form.username_value = u"my_username";
  incomplete_form.password_value = u"my_password";
  incomplete_form.blocked_by_user = false;
  incomplete_form.scheme = PasswordForm::Scheme::kHtml;
  EXPECT_EQ(AddChangeForForm(incomplete_form), db().AddLogin(incomplete_form));

  // Save a complete version of the previous form. Both forms could exist if
  // the user created the complete version before importing the incomplete
  // version from a different browser.
  PasswordForm complete_form = incomplete_form;
  complete_form.action = GURL("http://accounts.google.com/Login");
  complete_form.username_element = u"username_element";
  complete_form.password_element = u"password_element";
  complete_form.submit_element = u"submit";

  // An update fails because the primary key for |complete_form| is different.
  EXPECT_EQ(PasswordStoreChangeList(), db().UpdateLogin(complete_form));
  EXPECT_EQ(AddChangeForForm(complete_form), db().AddLogin(complete_form));

  // Make sure both passwords exist.
  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(2U, result.size());
  result.clear();

  // TODO(crbug.com/1223022): Once all places that operate changes on forms
  // via UpdateLogin properly set |password_issues|, setting them to an empty
  // map should be part of the default constructor.
  complete_form.password_issues =
      base::flat_map<InsecureType, InsecurityMetadata>();
  incomplete_form.password_issues =
      base::flat_map<InsecureType, InsecurityMetadata>();

  // Simulate the user changing their password.
  complete_form.password_value = u"new_password";
  complete_form.date_last_used = base::Time::Now();
  complete_form.date_password_modified = base::Time::Now();
  EXPECT_EQ(UpdateChangeForForm(complete_form, /*password_changed=*/true),
            db().UpdateLogin(complete_form));

  // When we retrieve the forms from the store, |in_store| should be set.
  complete_form.in_store = PasswordForm::Store::kProfileStore;
  incomplete_form.in_store = PasswordForm::Store::kProfileStore;

  // Both still exist now.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(2U, result.size());

  if (result[0]->username_element.empty())
    std::swap(result[0], result[1]);
  EXPECT_THAT(result[0], Pointee(HasPrimaryKeyAndEquals(complete_form)));
  EXPECT_THAT(result[1], Pointee(HasPrimaryKeyAndEquals(incomplete_form)));
}

TEST_F(LoginDatabaseTest, DoubleAdd) {
  PasswordForm form;
  form.url = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = u"my_username";
  form.password_value = u"my_password";
  form.blocked_by_user = false;
  form.scheme = PasswordForm::Scheme::kHtml;
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  // Add almost the same form again.
  form.times_used_in_html_form++;
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
  form.username_value = u"my_username";
  form.password_value = u"my_password";
  form.blocked_by_user = false;
  form.scheme = PasswordForm::Scheme::kHtml;
  EXPECT_EQ(PasswordStoreChangeList(), db().AddLogin(form));

  // |signon_realm| shouldn't be empty.
  form.url = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm.clear();
  EXPECT_EQ(PasswordStoreChangeList(), db().AddLogin(form));
}

// Test that when adding a login with no password_value but with
// encrypted_password, the encrypted_password is kept and the password_value
// is filled in with the decrypted password.
TEST_F(LoginDatabaseTest, AddLoginWithEncryptedPassword) {
  PasswordForm form;
  form.url = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = u"my_username";
  std::string encrypted;
  EXPECT_EQ(LoginDatabase::ENCRYPTION_RESULT_SUCCESS,
            db().EncryptedString(u"my_encrypted_password", &encrypted));
  form.encrypted_password = encrypted;
  form.blocked_by_user = false;
  form.scheme = PasswordForm::Scheme::kHtml;

  // |AddLogin| will decrypt the encrypted password, so compare with that.
  PasswordForm form_with_password = form;
  form_with_password.password_value = u"my_encrypted_password";
  EXPECT_EQ(AddChangeForForm(form_with_password), db().AddLogin(form));

  std::vector<std::unique_ptr<PasswordForm>> result;
  ASSERT_TRUE(db().GetLogins(PasswordFormDigest(form),
                             /*should_PSL_matching_apply=*/true, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form.encrypted_password, result[0].get()->encrypted_password);
  EXPECT_EQ(u"my_encrypted_password", result[0].get()->password_value);

  std::u16string decrypted;
  EXPECT_EQ(
      LoginDatabase::ENCRYPTION_RESULT_SUCCESS,
      db().DecryptedString(result[0].get()->encrypted_password, &decrypted));
  EXPECT_EQ(u"my_encrypted_password", decrypted);
}

// Test that when adding a login with password_value but with
// encrypted_password, the encrypted_password is discarded.
TEST_F(LoginDatabaseTest, AddLoginWithEncryptedPasswordAndValue) {
  PasswordForm form;
  form.url = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = u"my_username";
  form.password_value = u"my_password_value";
  std::string encrypted;
  EXPECT_EQ(LoginDatabase::ENCRYPTION_RESULT_SUCCESS,
            db().EncryptedString(u"my_encrypted_password", &encrypted));
  form.encrypted_password = encrypted;
  form.blocked_by_user = false;
  form.scheme = PasswordForm::Scheme::kHtml;
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  std::vector<std::unique_ptr<PasswordForm>> result;
  ASSERT_TRUE(db().GetLogins(PasswordFormDigest(form),
                             /*should_PSL_matching_apply=*/true, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_NE(form.encrypted_password, result[0].get()->encrypted_password);

  std::u16string decrypted;
  EXPECT_EQ(
      LoginDatabase::ENCRYPTION_RESULT_SUCCESS,
      db().DecryptedString(result[0].get()->encrypted_password, &decrypted));
  EXPECT_EQ(u"my_password_value", decrypted);
}

TEST_F(LoginDatabaseTest, UpdateLogin) {
  PasswordForm form;
  form.url = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = u"my_username";
  form.password_value = u"my_password";
  form.blocked_by_user = false;
  form.scheme = PasswordForm::Scheme::kHtml;
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  form.action = GURL("http://accounts.google.com/login");
  form.password_value = u"my_new_password";
  form.all_alternative_usernames.emplace_back(
      AlternativeElement::Value(u"my_new_username"),
      autofill::FieldRendererId(),
      AlternativeElement::Name(u"new_username_id"));
  form.times_used_in_html_form = 20;
  form.submit_element = u"submit_element";
  form.date_created = base::Time::Now() - base::Days(3);
  form.date_last_used = base::Time::Now();
  form.date_password_modified = base::Time::Now() - base::Days(1);
  form.blocked_by_user = true;
  form.scheme = PasswordForm::Scheme::kBasic;
  form.type = PasswordForm::Type::kGenerated;
  form.display_name = u"Mr. Smith";
  form.icon_url = GURL("https://accounts.google.com/Icon");
  form.federation_origin =
      url::Origin::Create(GURL("https://accounts.google.com/"));
  form.skip_zero_click = true;
  form.moving_blocked_for_list.push_back(GaiaIdHash::FromGaiaId("gaia_id"));

  PasswordStoreChangeList changes = db().UpdateLogin(form);
  EXPECT_EQ(UpdateChangeForForm(form, /*password_changed=*/true), changes);
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(1, changes[0].form().primary_key.value().value());

  // When we retrieve the form from the store, it should have |in_store| set.
  form.in_store = PasswordForm::Store::kProfileStore;

  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form),
                             /*should_PSL_matching_apply=*/true, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_THAT(result[0], Pointee(HasPrimaryKeyAndEquals(form)));
}

TEST_F(LoginDatabaseTest, UpdateLoginWithoutPassword) {
  PasswordForm form;
  form.url = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = u"my_username";
  form.password_value = u"my_password";
  form.blocked_by_user = false;
  form.scheme = PasswordForm::Scheme::kHtml;
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  form.action = GURL("http://accounts.google.com/login");
  form.all_alternative_usernames.emplace_back(
      AlternativeElement::Value(u"my_new_username"),
      autofill::FieldRendererId(),
      AlternativeElement::Name(u"new_username_id"));
  form.times_used_in_html_form = 20;
  form.submit_element = u"submit_element";
  form.date_created = base::Time::Now() - base::Days(3);
  form.date_last_used = base::Time::Now();
  form.display_name = u"Mr. Smith";
  form.icon_url = GURL("https://accounts.google.com/Icon");
  form.skip_zero_click = true;
  form.moving_blocked_for_list.push_back(GaiaIdHash::FromGaiaId("gaia_id"));

  PasswordStoreChangeList changes = db().UpdateLogin(form);
  EXPECT_EQ(UpdateChangeForForm(form, /*password_changed=*/false), changes);
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(1, changes[0].form().primary_key.value().value());

  // When we retrieve the form from the store, it should have |in_store| set.
  form.in_store = PasswordForm::Store::kProfileStore;

  std::vector<std::unique_ptr<PasswordForm>> result;
  ASSERT_TRUE(db().GetLogins(PasswordFormDigest(form),
                             /*should_PSL_matching_apply=*/true, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_THAT(result[0], Pointee(HasPrimaryKeyAndEquals(form)));
}

TEST_F(LoginDatabaseTest, RemoveWrongForm) {
  PasswordForm form;
  // |origin| shouldn't be empty.
  form.url = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = u"my_username";
  form.password_value = u"my_password";
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
  password_form.username_value = u"test1@gmail.com";
  password_form.password_value = u"test";
  password_form.signon_realm = "http://example.com/";
  password_form.times_used_in_html_form = 0;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.username_value = u"test2@gmail.com";
  password_form.times_used_in_html_form = 1;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.url = GURL("http://second.example.com");
  password_form.signon_realm = "http://second.example.com";
  password_form.times_used_in_html_form = 3;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.username_value = u"test3@gmail.com";
  password_form.type = PasswordForm::Type::kGenerated;
  password_form.times_used_in_html_form = 2;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.url = GURL("ftp://third.example.com/");
  password_form.signon_realm = "ftp://third.example.com/";
  password_form.times_used_in_html_form = 4;
  password_form.scheme = PasswordForm::Scheme::kOther;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.url = GURL("http://fourth.example.com/");
  password_form.signon_realm = "http://fourth.example.com/";
  password_form.type = PasswordForm::Type::kFormSubmission;
  password_form.username_value = u"";
  password_form.times_used_in_html_form = 10;
  password_form.scheme = PasswordForm::Scheme::kHtml;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.url = GURL("https://fifth.example.com/");
  password_form.signon_realm = "https://fifth.example.com/";
  password_form.password_value = u"";
  password_form.blocked_by_user = true;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.url = GURL("https://sixth.example.com/");
  password_form.signon_realm = "https://sixth.example.com/";
  password_form.username_value = u"my_username";
  password_form.password_value = u"my_password";
  password_form.blocked_by_user = false;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.url = GURL();
  password_form.signon_realm = "android://hash@com.example.android/";
  password_form.username_value = u"JohnDoe";
  password_form.password_value = u"my_password";
  password_form.blocked_by_user = false;
  EXPECT_EQ(AddChangeForForm(password_form), db->AddLogin(password_form));

  password_form.username_value = u"JaneDoe";
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
  stats.username_value = u"user1";
  stats.dismissal_count = 10;
  stats.update_time = base::Time::FromTimeT(1);
  EXPECT_TRUE(stats_table.AddRow(stats));
  stats.username_value = u"user2";
  stats.dismissal_count = 1;
  EXPECT_TRUE(stats_table.AddRow(stats));
  stats.username_value = u"user3";
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
  db().ReportMetrics();
  account_db.ReportMetrics();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProfileStore.InaccessiblePasswords3", 0, 1);
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.BubbleSuppression.AccountsInStatisticsTable2", 4, 1);
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
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
  db().ReportMetrics();
  account_db.ReportMetrics();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.InaccessiblePasswords3", 0, 1);
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
      db().UpdateEntityMetadata(syncer::PASSWORDS, kStorageKey1, metadata));

  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_initial_sync_state(
      sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  EXPECT_TRUE(db().UpdateModelTypeState(syncer::PASSWORDS, model_type_state));

  metadata.set_sequence_number(2);
  EXPECT_TRUE(
      db().UpdateEntityMetadata(syncer::PASSWORDS, kStorageKey2, metadata));

  std::unique_ptr<syncer::MetadataBatch> metadata_batch =
      db().GetAllSyncMetadata();
  ASSERT_THAT(metadata_batch, testing::NotNull());

  EXPECT_EQ(metadata_batch->GetModelTypeState().initial_sync_state(),
            sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  syncer::EntityMetadataMap metadata_records =
      metadata_batch->TakeAllMetadata();

  EXPECT_EQ(metadata_records.size(), 2u);
  EXPECT_EQ(metadata_records[kStorageKey1]->sequence_number(), 1);
  EXPECT_EQ(metadata_records[kStorageKey2]->sequence_number(), 2);

  // Now check that a model type state update replaces the old value
  model_type_state.set_initial_sync_state(
      sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED);
  EXPECT_TRUE(db().UpdateModelTypeState(syncer::PASSWORDS, model_type_state));

  metadata_batch = db().GetAllSyncMetadata();
  ASSERT_THAT(metadata_batch, testing::NotNull());
  EXPECT_EQ(
      metadata_batch->GetModelTypeState().initial_sync_state(),
      sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED);
}

TEST_F(LoginDatabaseTest, DeleteAllSyncMetadata) {
  sync_pb::EntityMetadata metadata;
  // Storage keys must be integers.
  const std::string kStorageKey1 = "1";
  const std::string kStorageKey2 = "2";
  metadata.set_sequence_number(1);

  EXPECT_TRUE(
      db().UpdateEntityMetadata(syncer::PASSWORDS, kStorageKey1, metadata));

  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_initial_sync_state(
      sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  EXPECT_TRUE(db().UpdateModelTypeState(syncer::PASSWORDS, model_type_state));

  metadata.set_sequence_number(2);
  EXPECT_TRUE(
      db().UpdateEntityMetadata(syncer::PASSWORDS, kStorageKey2, metadata));

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

  model_type_state.set_initial_sync_state(
      sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  metadata.set_client_tag_hash("client_hash");

  // Write the data into the store.
  EXPECT_TRUE(
      db().UpdateEntityMetadata(syncer::PASSWORDS, kStorageKey, metadata));
  EXPECT_TRUE(db().UpdateModelTypeState(syncer::PASSWORDS, model_type_state));
  // Delete the data we just wrote.
  EXPECT_TRUE(db().ClearEntityMetadata(syncer::PASSWORDS, kStorageKey));

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

#if BUILDFLAG(IS_POSIX)
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
#endif  // BUILDFLAG(IS_POSIX)

#if !BUILDFLAG(IS_IOS)
// Test that LoginDatabase encrypts the password values that it stores.
TEST_F(LoginDatabaseTest, EncryptionEnabled) {
  PasswordForm password_form = GenerateExamplePasswordForm();
  base::FilePath file = temp_dir_.GetPath().AppendASCII("TestUnencryptedDB");
  {
    LoginDatabase db(file, IsAccountStore(false));
    ASSERT_TRUE(db.Init());
    EXPECT_EQ(AddChangeForForm(password_form), db.AddLogin(password_form));
  }
  std::u16string decrypted_pw;
  ASSERT_TRUE(OSCrypt::DecryptString16(
      GetColumnValuesFromDatabase<std::string>(file, "password_value").at(0),
      &decrypted_pw));
  EXPECT_EQ(decrypted_pw, password_form.password_value);
}
#endif  // !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
// On Android and ChromeOS there is a mix of plain-text and obfuscated
// passwords. Verify that they can both be accessed. Obfuscated passwords start
// with "v10". Some password values also start with "v10". Test that both are
// accessible (this doesn't work for any plain-text value).
TEST_F(LoginDatabaseTest, HandleObfuscationMix) {
  const char k_obfuscated_pw[] = "v10pass1";
  const char16_t k_obfuscated_pw16[] = u"v10pass1";
  const char k_plain_text_pw1[] = "v10pass2";
  const char16_t k_plain_text_pw116[] = u"v10pass2";
  const char k_plain_text_pw2[] = "v11pass3";
  const char16_t k_plain_text_pw216[] = u"v11pass3";

  base::FilePath file = temp_dir_.GetPath().AppendASCII("TestUnencryptedDB");
  {
    LoginDatabase db(file, IsAccountStore(false));
    ASSERT_TRUE(db.Init());
    // Add obfuscated (new) entries.
    PasswordForm password_form = GenerateExamplePasswordForm();
    password_form.password_value = k_obfuscated_pw16;
    EXPECT_EQ(AddChangeForForm(password_form), db.AddLogin(password_form));
    // Add plain-text (old) entries and rewrite the password on the disk.
    password_form.username_value = u"other_username";
    EXPECT_EQ(AddChangeForForm(password_form), db.AddLogin(password_form));
    password_form.username_value = u"other_username2";
    EXPECT_EQ(AddChangeForForm(password_form), db.AddLogin(password_form));
  }
  UpdatePasswordValueForUsername(file, u"other_username", k_plain_text_pw116);
  UpdatePasswordValueForUsername(file, u"other_username2", k_plain_text_pw216);

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
  EXPECT_THAT(
      forms,
      UnorderedElementsAre(
          Pointee(Field(&PasswordForm::password_value, k_obfuscated_pw16)),
          Pointee(Field(&PasswordForm::password_value, k_plain_text_pw116)),
          Pointee(Field(&PasswordForm::password_value, k_plain_text_pw216))));
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)

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
    ASSERT_TRUE(meta_table.SetVersionNumber(kDBFutureVersion));
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
    PasswordForm form = GenerateExamplePasswordForm();
    // Add the same form twice to test the constraints in the database.
    EXPECT_EQ(AddChangeForForm(form), db.AddLogin(form));
    PasswordStoreChangeList list;
    list.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form));
    list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
    EXPECT_EQ(list, db.AddLogin(form));

    result.clear();
    EXPECT_TRUE(db.GetLogins(PasswordFormDigest(form),
                             /*should_PSL_matching_apply=*/true, &result));
    ASSERT_EQ(1U, result.size());
    EXPECT_THAT(result[0], Pointee(HasPrimaryKeyAndEquals(form)));
    EXPECT_TRUE(db.RemoveLogin(form, /*changes=*/nullptr));

    if (version() == 31) {
      // Check that unset values of 'insecure_credentials.create_time' are set
      // to current time.
      std::vector<InsecureCredential> insecure_credentials(
          db.insecure_credentials_table().GetRows(FormPrimaryKey(1)));
      ASSERT_EQ(2U, insecure_credentials.size());
      base::Time time_now = base::Time::Now();
      base::Time time_slightly_before = time_now - base::Seconds(2);
      EXPECT_LE(insecure_credentials[0].create_time, time_now);
      EXPECT_GE(insecure_credentials[0].create_time, time_slightly_before);
    }
  }
  // Added 07/21. Safe to remove in a year.
  if (version() <= 29) {
    // Check that 'date_password_modified' is copied from 'date_created'.
    std::vector<int64_t> password_modified(GetColumnValuesFromDatabase<int64_t>(
        database_path_, "date_password_modified"));
    EXPECT_EQ(13047429345000000, password_modified[0]);
    EXPECT_EQ(13047423600000000, password_modified[1]);
    EXPECT_EQ(13047423600000000, password_modified[2]);
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
                             bool should_be_corrupted,
                             bool blocklisted);

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
    bool should_be_corrupted,
    bool blocklisted) {
  // Create a dummy password form.
  const std::u16string unique_string16 = ASCIIToUTF16(unique_string);
  PasswordForm form;
  form.url = origin;
  form.username_element = unique_string16;
  form.username_value = unique_string16;
  form.password_element = unique_string16;
  form.password_value = unique_string16;
  form.signon_realm = origin.DeprecatedGetOriginAsURL().spec();
  form.blocked_by_user = blocklisted;

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
  auto form1 =
      AddDummyLogin("foo1", GURL("https://foo1.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  auto form2 =
      AddDummyLogin("foo2", GURL("https://foo2.com/"),
                    /*should_be_corrupted=*/true, /*blocklisted=*/false);
  auto form3 =
      AddDummyLogin("foo3", GURL("https://foo3.com/"),
                    /*should_be_corrupted=*/true, /*blocklisted=*/true);

  LoginDatabase db(database_path(), IsAccountStore(false));
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(db.Init());

#if BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS))
  // Make sure that we can't get any logins when database is corrupted.
  // Disabling the checks in chromecast because encryption is unavailable.
  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_FALSE(db.GetAutofillableLogins(&result));
  EXPECT_TRUE(result.empty());
  EXPECT_FALSE(db.GetBlocklistLogins(&result));
  EXPECT_TRUE(result.empty());

  // Delete undecryptable logins and make sure we can get valid logins.
  EXPECT_EQ(DatabaseCleanupResult::kSuccess, db.DeleteUndecryptableLogins());
  EXPECT_TRUE(db.GetAutofillableLogins(&result));
  EXPECT_THAT(result,
              UnorderedElementsAre(Pointee(HasPrimaryKeyAndEquals(form1))));

  EXPECT_TRUE(db.GetBlocklistLogins(&result));
  EXPECT_THAT(result, IsEmpty());

  RunUntilIdle();
#elif BUILDFLAG(IS_CASTOS)
  EXPECT_EQ(DatabaseCleanupResult::kEncryptionUnavailable,
            db.DeleteUndecryptableLogins());
#else
  EXPECT_EQ(DatabaseCleanupResult::kSuccess, db.DeleteUndecryptableLogins());
#endif

// Check histograms.
#if BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS))
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.DeleteUndecryptableLoginsReturnValue",
      metrics_util::DeleteCorruptedPasswordsResult::kSuccessPasswordsDeleted,
      1);
#endif
}

#if BUILDFLAG(IS_MAC)
TEST_F(LoginDatabaseUndecryptableLoginsTest,
       PasswordRecoveryDisabledGetLogins) {
  AddDummyLogin("foo1", GURL("https://foo1.com/"), false,
                /*blocklisted=*/false);
  AddDummyLogin("foo2", GURL("https://foo2.com/"), true, /*blocklisted=*/false);

  LoginDatabase db(database_path(), IsAccountStore(false));
  ASSERT_TRUE(db.Init());

  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_FALSE(db.GetAutofillableLogins(&result));
  EXPECT_TRUE(result.empty());

  RunUntilIdle();
}

TEST_F(LoginDatabaseUndecryptableLoginsTest, KeychainLockedTest) {
  AddDummyLogin("foo1", GURL("https://foo1.com/"), false,
                /*blocklisted=*/false);
  AddDummyLogin("foo2", GURL("https://foo2.com/"), true, /*blocklisted=*/false);

  OSCryptMocker::SetBackendLocked(true);
  LoginDatabase db(database_path(), IsAccountStore(false));
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(db.Init());
  EXPECT_EQ(DatabaseCleanupResult::kEncryptionUnavailable,
            db.DeleteUndecryptableLogins());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.DeleteUndecryptableLoginsReturnValue",
      metrics_util::DeleteCorruptedPasswordsResult::kEncryptionUnavailable, 1);
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Test getting auto sign in logins when there are undecryptable ones
TEST_F(LoginDatabaseUndecryptableLoginsTest, GetAutoSignInLogins) {
  std::vector<std::unique_ptr<PasswordForm>> forms;

  auto form1 =
      AddDummyLogin("foo1", GURL("https://foo1.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  auto form2 =
      AddDummyLogin("foo2", GURL("https://foo2.com/"),
                    /*should_be_corrupted=*/true, /*blocklisted=*/false);
  auto form3 =
      AddDummyLogin("foo3", GURL("https://foo3.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);

  LoginDatabase db(database_path(), IsAccountStore(false));
  ASSERT_TRUE(db.Init());

  EXPECT_FALSE(db.GetAutoSignInLogins(&forms));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kSkipUndecryptablePasswords);

  EXPECT_TRUE(db.GetAutoSignInLogins(&forms));
  EXPECT_THAT(forms,
              UnorderedElementsAre(Pointee(HasPrimaryKeyAndEquals(form1)),
                                   Pointee(HasPrimaryKeyAndEquals(form3))));
}

// Test getting logins when there are undecryptable ones
TEST_F(LoginDatabaseUndecryptableLoginsTest, GetLogins) {
  auto form1 =
      AddDummyLogin("user1", GURL("http://www.google.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  auto form2 =
      AddDummyLogin("user2", GURL("http://www.google.com/"),
                    /*should_be_corrupted=*/true, /*blocklisted=*/false);
  LoginDatabase db(database_path(), IsAccountStore(false));
  ASSERT_TRUE(db.Init());
  std::vector<std::unique_ptr<PasswordForm>> result;

  PasswordForm form = GenerateExamplePasswordForm();
  EXPECT_FALSE(db.GetLogins(PasswordFormDigest(form),
                            /*should_PSL_matching_apply=*/false, &result));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kSkipUndecryptablePasswords);
  result.clear();

  EXPECT_TRUE(db.GetLogins(PasswordFormDigest(form),
                           /*should_PSL_matching_apply=*/false, &result));
  EXPECT_THAT(result, ElementsAre(Pointee(HasPrimaryKeyAndEquals(form1))));
}

// Test getting auto fillable logins when there are undecryptable ones
TEST_F(LoginDatabaseUndecryptableLoginsTest, GetAutofillableLogins) {
  std::vector<std::unique_ptr<PasswordForm>> result;

  auto form1 =
      AddDummyLogin("foo1", GURL("https://foo1.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  auto form2 =
      AddDummyLogin("foo2", GURL("https://foo2.com/"),
                    /*should_be_corrupted=*/true, /*blocklisted=*/false);
  auto form3 =
      AddDummyLogin("foo3", GURL("https://foo3.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/true);

  LoginDatabase db(database_path(), IsAccountStore(false));
  ASSERT_TRUE(db.Init());

  EXPECT_FALSE(db.GetAutofillableLogins(&result));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kSkipUndecryptablePasswords);

  EXPECT_TRUE(db.GetAutofillableLogins(&result));
  EXPECT_THAT(result, ElementsAre(Pointee(HasPrimaryKeyAndEquals(form1))));
}
#endif

// Test encrypted passwords are present in add change lists.
TEST_F(LoginDatabaseTest, EncryptedPasswordAdd) {
  PasswordForm form;
  form.url = GURL("http://0.com");
  form.signon_realm = "http://www.example.com/";
  form.action = GURL("http://www.example.com/action");
  form.password_element = u"pwd";
  form.password_value = u"example";
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
  form.password_element = u"pwd";
  form.password_value = u"example";

  std::ignore = db().AddLogin(form);

  form.password_value = u"secret";

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
  form.password_element = u"pwd";
  form.password_value = u"example";

  std::ignore = db().AddLogin(form);

  form.password_value = u"secret";

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
  form.password_element = u"pwd";
  form.password_value = u"example";
  password_manager::PasswordStoreChangeList changes = db().AddLogin(form);
  ASSERT_EQ(1u, changes.size());
  ASSERT_FALSE(changes[0].form().encrypted_password.empty());

  std::vector<std::unique_ptr<PasswordForm>> forms;
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form),
                             /*should_PSL_matching_apply=*/false, &forms));

  ASSERT_EQ(1U, forms.size());
  ASSERT_FALSE(forms[0]->encrypted_password.empty());
}

TEST_F(LoginDatabaseTest, RetrievesInsecureDataWithLogins) {
  PasswordForm form = GenerateExamplePasswordForm();
  std::ignore = db().AddLogin(form);

  base::flat_map<InsecureType, InsecurityMetadata> issues;
  // Assume that the leaked credential has been found by the proactive
  // check and a notification still needs to be sent.
  issues[InsecureType::kLeaked] = InsecurityMetadata(
      base::Time(), IsMuted(false), TriggerBackendNotification(true));
  issues[InsecureType::kPhished] = InsecurityMetadata(
      base::Time(), IsMuted(false), TriggerBackendNotification(false));
  form.password_issues = std::move(issues);

  db().insecure_credentials_table().InsertOrReplace(
      FormPrimaryKey(1), InsecureType::kLeaked,
      form.password_issues[InsecureType::kLeaked]);
  db().insecure_credentials_table().InsertOrReplace(
      FormPrimaryKey(1), InsecureType::kPhished,
      form.password_issues[InsecureType::kPhished]);

  std::vector<std::unique_ptr<PasswordForm>> result;
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form),
                             /*should_PSL_matching_apply=*/true, &result));
  EXPECT_THAT(result,
              UnorderedElementsAre(Pointee(HasPrimaryKeyAndEquals(form))));
}

TEST_F(LoginDatabaseTest, RetrievesNoteWithLogin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kPasswordNotesWithBackup);

  PasswordForm form = GenerateExamplePasswordForm();
  std::ignore = db().AddLogin(form);
  PasswordNote note(u"example note", base::Time::Now());
  db().password_notes_table().InsertOrReplace(FormPrimaryKey(1), note);

  std::vector<std::unique_ptr<PasswordForm>> results;
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form),
                             /* should_PSL_matching_apply */ true, &results));

  PasswordForm expected_form = form;
  expected_form.notes = {note};
  EXPECT_THAT(results, UnorderedElementsAre(
                           Pointee(HasPrimaryKeyAndEquals(expected_form))));
}

TEST_F(LoginDatabaseTest, AddLoginWithNotePersistsThem) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kPasswordNotesWithBackup);

  PasswordForm form = GenerateExamplePasswordForm();
  PasswordNote note(u"example note", base::Time::Now());
  form.notes = {note};

  std::ignore = db().AddLogin(form);

  EXPECT_EQ(db().password_notes_table().GetPasswordNotes(FormPrimaryKey(1))[0],
            note);
}

TEST_F(LoginDatabaseTest, RemoveLoginRemovesNoteAttachedToTheLogin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kPasswordNotesWithBackup);

  PasswordForm form = GenerateExamplePasswordForm();
  PasswordNote note = PasswordNote(u"example note", base::Time::Now());
  form.notes = {note};
  std::ignore = db().AddLogin(form);

  EXPECT_EQ(db().password_notes_table().GetPasswordNotes(FormPrimaryKey(1))[0],
            note);

  PasswordStoreChangeList list;
  EXPECT_TRUE(db().RemoveLogin(form, &list));
  EXPECT_TRUE(
      db().password_notes_table().GetPasswordNotes(FormPrimaryKey(1)).empty());
}

TEST_F(LoginDatabaseTest, RemovingLoginRemovesInsecureCredentials) {
  PasswordForm form = GenerateExamplePasswordForm();

  std::ignore = db().AddLogin(form);
  InsecureCredential credential1{
      form.signon_realm, form.username_value,
      base::Time(),      InsecureType::kLeaked,
      IsMuted(false),    TriggerBackendNotification(false)};
  InsecureCredential credential2 = credential1;
  credential2.insecure_type = InsecureType::kPhished;

  db().insecure_credentials_table().InsertOrReplace(
      FormPrimaryKey(1), credential1.insecure_type,
      InsecurityMetadata(credential1.create_time, credential1.is_muted,
                         credential1.trigger_notification_from_backend));
  db().insecure_credentials_table().InsertOrReplace(
      FormPrimaryKey(1), credential2.insecure_type,
      InsecurityMetadata(credential2.create_time, credential2.is_muted,
                         credential2.trigger_notification_from_backend));

  ASSERT_THAT(db().insecure_credentials_table().GetRows(FormPrimaryKey(1)),
              ElementsAre(credential1, credential2));

  EXPECT_TRUE(db().RemoveLogin(form, nullptr));
  EXPECT_THAT(db().insecure_credentials_table().GetRows(FormPrimaryKey(1)),
              testing::IsEmpty());
}

// Test retrieving password forms by supplied signon_realm and username.
TEST_F(LoginDatabaseTest, GetLoginsBySignonRealmAndUsername) {
  std::string signon_realm = "https://test.com";
  std::u16string username1 = u"username1";
  std::u16string username2 = u"username2";

  // Insert first login.
  PasswordForm form1 = GenerateExamplePasswordForm();
  form1.signon_realm = signon_realm;
  form1.username_value = username1;
  ASSERT_EQ(AddChangeForForm(form1), db().AddLogin(form1));

  PasswordForm form2 = GenerateExamplePasswordForm();
  form2.signon_realm = signon_realm;
  form2.username_value = username2;
  ASSERT_EQ(AddChangeForForm(form2), db().AddLogin(form2));

  std::vector<std::unique_ptr<PasswordForm>> forms;
  // Check if there is exactly one form with this signon_realm & username1.
  EXPECT_EQ(
      FormRetrievalResult::kSuccess,
      db().GetLoginsBySignonRealmAndUsername(signon_realm, username1, &forms));
  EXPECT_THAT(forms, ElementsAre(Pointee(HasPrimaryKeyAndEquals(form1))));

  // Insert another form with the same username as form1.
  PasswordForm form3 = GenerateExamplePasswordForm();
  form3.signon_realm = signon_realm;
  form3.username_value = username1;
  form3.username_element = u"another_element";
  ASSERT_EQ(AddChangeForForm(form3), db().AddLogin(form3));

  // Check if there are exactly two forms with given username and signon_realm.
  EXPECT_EQ(
      FormRetrievalResult::kSuccess,
      db().GetLoginsBySignonRealmAndUsername(signon_realm, username1, &forms));
  EXPECT_THAT(forms, ElementsAre(Pointee(HasPrimaryKeyAndEquals(form1)),
                                 Pointee(HasPrimaryKeyAndEquals(form3))));
}

TEST_F(LoginDatabaseTest, UpdateLoginWithAddedInsecureCredential) {
  PasswordForm form = GenerateExamplePasswordForm();
  std::ignore = db().AddLogin(form);
  // Assume the leaked credential was found outside of Chrome and a notification
  // trigger was set on it.
  InsecureCredential insecure_credential{
      form.signon_realm, form.username_value,
      base::Time(),      InsecureType::kLeaked,
      IsMuted(false),    TriggerBackendNotification(true)};
  base::flat_map<InsecureType, InsecurityMetadata> issues;
  issues[InsecureType::kLeaked] = InsecurityMetadata(
      insecure_credential.create_time, insecure_credential.is_muted,
      insecure_credential.trigger_notification_from_backend);
  form.password_issues = std::move(issues);

  EXPECT_EQ(UpdateChangeForForm(form, /*password_changed=*/false,
                                /*insecure_changed=*/true),
            db().UpdateLogin(form, nullptr));
  EXPECT_THAT(db().insecure_credentials_table().GetRows(FormPrimaryKey(1)),
              ElementsAre(insecure_credential));
}

TEST_F(LoginDatabaseTest, UpdateLoginWithUpdatedInsecureCredential) {
  PasswordForm form = GenerateExamplePasswordForm();
  std::ignore = db().AddLogin(form);
  InsecureCredential insecure_credential{
      form.signon_realm, form.username_value,
      base::Time(),      InsecureType::kLeaked,
      IsMuted(false),    TriggerBackendNotification(false)};
  base::flat_map<InsecureType, InsecurityMetadata> issues;
  issues[InsecureType::kLeaked] = InsecurityMetadata(
      base::Time(), IsMuted(false), TriggerBackendNotification(false));
  form.password_issues = std::move(issues);

  ASSERT_EQ(UpdateChangeForForm(form, /*password_changed=*/false,
                                /*insecure_changed=*/true),
            db().UpdateLogin(form, nullptr));
  ASSERT_THAT(db().insecure_credentials_table().GetRows(FormPrimaryKey(1)),
              ElementsAre(insecure_credential));

  form.password_issues[InsecureType::kLeaked].is_muted = IsMuted(true);
  EXPECT_EQ(UpdateChangeForForm(form, /*password_changed=*/false,
                                /*insecure_changed=*/true),
            db().UpdateLogin(form, nullptr));
  insecure_credential.is_muted = IsMuted(true);
  EXPECT_THAT(db().insecure_credentials_table().GetRows(FormPrimaryKey(1)),
              ElementsAre(insecure_credential));
}

TEST_F(LoginDatabaseTest, UpdateLoginWithRemovedInsecureCredentialEntry) {
  PasswordForm form = GenerateExamplePasswordForm();
  std::ignore = db().AddLogin(form);
  InsecureCredential leaked{
      form.signon_realm, form.username_value,
      base::Time(),      InsecureType::kLeaked,
      IsMuted(false),    TriggerBackendNotification(false)};
  InsecureCredential phished{
      form.signon_realm, form.username_value,
      base::Time(),      InsecureType::kPhished,
      IsMuted(false),    TriggerBackendNotification(false)};
  leaked.parent_key = phished.parent_key = FormPrimaryKey(1);
  base::flat_map<InsecureType, InsecurityMetadata> issues;
  issues[InsecureType::kLeaked] = InsecurityMetadata(
      base::Time(), IsMuted(false), TriggerBackendNotification(false));
  issues[InsecureType::kPhished] = InsecurityMetadata(
      base::Time(), IsMuted(false), TriggerBackendNotification(false));
  form.password_issues = std::move(issues);

  ASSERT_EQ(UpdateChangeForForm(form, /*password_changed=*/false,
                                /*insecure_changed=*/true),
            db().UpdateLogin(form, nullptr));
  ASSERT_THAT(db().insecure_credentials_table().GetRows(FormPrimaryKey(1)),
              UnorderedElementsAre(leaked, phished));

  // Complete password_issues removal can usually only happen when the password
  // is changed.
  form.password_value = u"new_password";
  form.password_issues.clear();
  EXPECT_EQ(UpdateChangeForForm(form, /*password_changed=*/true,
                                /*insecure_changed=*/true),
            db().UpdateLogin(form, nullptr));
  EXPECT_THAT(db().insecure_credentials_table().GetRows(FormPrimaryKey(1)),
              IsEmpty());
}

TEST_F(LoginDatabaseTest,
       AddLoginWithDifferentPasswordRemovesInsecureCredentials) {
  PasswordForm form = GenerateExamplePasswordForm();

  std::ignore = db().AddLogin(form);
  InsecureCredential credential1{
      form.signon_realm, form.username_value,
      base::Time(),      InsecureType::kLeaked,
      IsMuted(false),    TriggerBackendNotification(false)};
  InsecureCredential credential2 = credential1;
  credential2.insecure_type = InsecureType::kPhished;

  db().insecure_credentials_table().InsertOrReplace(
      FormPrimaryKey(1), InsecureType::kLeaked,
      InsecurityMetadata(base::Time(), IsMuted(false),
                         TriggerBackendNotification(false)));
  db().insecure_credentials_table().InsertOrReplace(
      FormPrimaryKey(1), InsecureType::kPhished,
      InsecurityMetadata(base::Time(), IsMuted(false),
                         TriggerBackendNotification(false)));

  EXPECT_THAT(db().insecure_credentials_table().GetRows(FormPrimaryKey(1)),
              testing::UnorderedElementsAre(credential1, credential2));
  form.password_value = u"new_password";

  PasswordStoreChangeList list;
  list.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form));
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
  EXPECT_EQ(list, db().AddLogin(form));
  EXPECT_THAT(db().insecure_credentials_table().GetRows(FormPrimaryKey(1)),
              IsEmpty());
}

TEST_F(LoginDatabaseTest, AddLoginWithInsecureCredentialsPersistsThem) {
  PasswordForm form = GenerateExamplePasswordForm();
  InsecureCredential leaked{
      form.signon_realm, form.username_value,
      base::Time(),      InsecureType::kLeaked,
      IsMuted(false),    TriggerBackendNotification(false)};
  InsecureCredential phished = leaked;
  phished.insecure_type = InsecureType::kPhished;
  phished.trigger_notification_from_backend = TriggerBackendNotification(false);

  form.password_value = u"new_password";
  form.password_issues.insert_or_assign(
      InsecureType::kLeaked,
      InsecurityMetadata(leaked.create_time, leaked.is_muted,
                         leaked.trigger_notification_from_backend));
  form.password_issues.insert_or_assign(
      InsecureType::kPhished,
      InsecurityMetadata(phished.create_time, phished.is_muted,
                         phished.trigger_notification_from_backend));

  PasswordStoreChangeList list;
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
  EXPECT_EQ(list, db().AddLogin(form));
  EXPECT_THAT(db().insecure_credentials_table().GetRows(FormPrimaryKey(1)),
              testing::UnorderedElementsAre(leaked, phished));
}

TEST_F(LoginDatabaseTest, RemoveLoginRemovesInsecureCredentials) {
  PasswordForm form = GenerateExamplePasswordForm();
  form.password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))}};
  std::ignore = db().AddLogin(form);

  InsecureCredential leaked{
      form.signon_realm,        form.username_value,
      base::Time::FromTimeT(1), InsecureType::kLeaked,
      IsMuted(false),           TriggerBackendNotification(false)};
  ASSERT_THAT(db().insecure_credentials_table().GetRows(FormPrimaryKey(1)),
              ElementsAre(leaked));

  PasswordStoreChangeList list;
  EXPECT_TRUE(db().RemoveLogin(form, &list));
  EXPECT_THAT(db().insecure_credentials_table().GetRows(FormPrimaryKey(1)),
              IsEmpty());
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
  PasswordForm form = GenerateExamplePasswordForm();

  PasswordStoreChangeList changes = db().AddLogin(form);
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(PasswordForm::Store::kAccountStore, changes[0].form().in_store);
}

}  // namespace password_manager
