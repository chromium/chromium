// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/login_database.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_switches.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/password_store_change.h"
#include "components/password_manager/core/browser/password_store/psl_matching_helper.h"
#include "components/password_manager/core/browser/sync/password_store_sync.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

#if BUILDFLAG(IS_IOS)
#import <Security/Security.h>
#endif  // BUILDFLAG(IS_IOS)

using base::ASCIIToUTF16;
using base::UTF16ToASCII;
using signin::GaiaIdHash;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Ne;
using ::testing::NiceMock;
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
  form.form_data.set_name(u"form_name");
  form.date_last_used = base::Time::Now();
  form.date_password_modified = base::Time::Now() - base::Days(1);
  form.display_name = u"Mr. Smith";
  form.icon_url = GURL("https://accounts.google.com/Icon");
  form.skip_zero_click = true;
  form.in_store = PasswordForm::Store::kProfileStore;
  form.moving_blocked_for_list.push_back(GaiaIdHash::FromGaiaId("user1"));
  form.moving_blocked_for_list.push_back(GaiaIdHash::FromGaiaId("user2"));
  form.sender_email = u"sender@gmail.com";
  form.sender_name = u"Cool Sender";
  form.sender_profile_image_url = GURL("http://www.sender.com/profile_image");
  form.date_received = base::Time::Now() - base::Hours(1);
  form.sharing_notification_displayed = true;
  return form;
}

PasswordForm GenerateBlocklistedForm() {
  PasswordForm form = GenerateExamplePasswordForm();
  form.url = GURL("http://accounts.blocklisted.com/LoginAuth");
  form.action = GURL("http://accounts.blocklisted.com/Login");
  form.signon_realm = "http://www.blocklisted.com/";
  form.blocked_by_user = true;
  return form;
}

PasswordForm GenerateFederatedCredentialForm() {
  PasswordForm form = GenerateExamplePasswordForm();
  form.url = GURL("http://accounts.federated.com/LoginAuth");
  form.action = GURL("http://accounts.federated.com/Login");
  form.federation_origin =
      url::SchemeHostPort(GURL("https://accounts.federated.com/"));
  return form;
}

PasswordForm GenerateUsernameOnlyForm() {
  PasswordForm form = GenerateExamplePasswordForm();
  form.url = GURL("http://accounts.usernameonly.com/LoginAuth");
  form.action = GURL("http://accounts.usernameonly.com/Login");
  form.signon_realm = "http://www.usernameonly.com/";
  form.scheme = PasswordForm::Scheme::kUsernameOnly;
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
  sql::Statement s(db.GetUniqueStatement(statement));
  EXPECT_TRUE(s.is_valid());

  while (s.Step()) {
    results.push_back(GetFirstColumn<T>(s));
  }

  return results;
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
// Set the new password value for all the rows with the specified username.
void UpdatePasswordValueForUsername(const base::FilePath& database_path,
                                    const std::u16string& username,
                                    const std::u16string& password) {
  sql::Database db;
  CHECK(db.Open(database_path));

  sql::Statement s(db.GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE logins SET password_value = ? WHERE username_value = ?"));
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
  form.federation_origin = url::SchemeHostPort(origin);
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

os_crypt_async::Encryptor GetInstanceSync(
    os_crypt_async::OSCryptAsync* factory) {
  base::test::TestFuture<os_crypt_async::Encryptor, bool> future;

  auto sub = factory->GetInstance(future.GetCallback(),
                                  os_crypt_async::Encryptor::Option::kNone);
  return std::move(std::get<0>(future.Take()));
}

}  // namespace

// Serialization routines for vectors implemented in login_database.cc.
base::Pickle SerializeAlternativeElementVector(
    const AlternativeElementVector& vector);
AlternativeElementVector DeserializeAlternativeElementVector(
    const base::Pickle& pickle);
base::Pickle SerializeGaiaIdHashVector(const std::vector<GaiaIdHash>& hashes);
std::vector<GaiaIdHash> DeserializeGaiaIdHashVector(const base::Pickle& p);

class LoginDatabaseTestBase : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestMetadataStoreMacDatabase");
    OSCryptMocker::SetUp();

    db_ = std::make_unique<LoginDatabase>(file_, IsAccountStore(false));
    db_->SetIsEmptyCb(is_empty_cb_.Get());
    ASSERT_TRUE(
        db_->Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
                  /*encryptor=*/nullptr));
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  LoginDatabase& db() { return *db_; }

  base::ScopedTempDir temp_dir_;
  base::FilePath file_;
  NiceMock<base::MockCallback<LoginDatabase::IsEmptyCallback>> is_empty_cb_;
  std::unique_ptr<LoginDatabase> db_;
  // A full TaskEnvironment is required instead of only
  // SingleThreadTaskEnvironment because on iOS,
  // password_manager::DeletePasswordsDirectory() which calls
  // base::ThreadPool::PostTask().
  base::test::TaskEnvironment task_environment_;
};

// `GetParam()` controls whether `os_crypt_async::Encryptor` is used.
class LoginDatabaseTest : public LoginDatabaseTestBase,
                          public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestMetadataStoreMacDatabase");
    OSCryptMocker::SetUp();

    db_ = std::make_unique<LoginDatabase>(file_, IsAccountStore(false));
    db_->SetIsEmptyCb(is_empty_cb_.Get());
    ASSERT_TRUE(
        db_->Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
                  /*encryptor=*/encryptor()));
  }

  std::unique_ptr<os_crypt_async::Encryptor> encryptor() {
    if (GetParam()) {
      return std::make_unique<os_crypt_async::Encryptor>(
          GetInstanceSync(test_oscrypt_async_.get()));
    }
    return nullptr;
  }

 private:
  std::unique_ptr<os_crypt_async::OSCryptAsync> test_oscrypt_async_ =
      os_crypt_async::GetTestOSCryptAsyncForTesting(
          /*is_sync_for_unittests = */ true);
};

INSTANTIATE_TEST_SUITE_P(, LoginDatabaseTest, testing::Bool(), [](auto& info) {
  return info.param ? "OSCryptAsync" : "OsCryptSync";
});

TEST_P(LoginDatabaseTest, GetAllLogins) {
  // Example password form.
  PasswordForm form = GenerateExamplePasswordForm();
  ASSERT_EQ(AddChangeForForm(form), db().AddLogin(form));
  PasswordForm blocklisted;
  blocklisted.signon_realm = "http://example3.com/";
  blocklisted.url = GURL("http://example3.com/path");
  blocklisted.blocked_by_user = true;
  blocklisted.in_store = PasswordForm::Store::kProfileStore;
  ASSERT_EQ(AddChangeForForm(blocklisted), db().AddLogin(blocklisted));

  std::vector<PasswordForm> forms;
  EXPECT_EQ(db().GetAllLogins(&forms), FormRetrievalResult::kSuccess);
  EXPECT_THAT(forms, UnorderedElementsAre(HasPrimaryKeyAndEquals(form),
                                          HasPrimaryKeyAndEquals(blocklisted)));
}

TEST_P(LoginDatabaseTest, GetLogins_Self) {
  PasswordForm form = GenerateExamplePasswordForm();
  ASSERT_EQ(AddChangeForForm(form), db().AddLogin(form));

  // Match against an exact copy.
  std::vector<PasswordForm> result;
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form),
                             /*should_PSL_matching_apply=*/false, &result));
  EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form)));
}

TEST_P(LoginDatabaseTest, GetLogins_InexactCopy) {
  PasswordForm form = GenerateExamplePasswordForm();
  ASSERT_EQ(AddChangeForForm(form), db().AddLogin(form));

  PasswordFormDigest digest(
      PasswordForm::Scheme::kHtml, "http://www.google.com/",
      GURL("http://www.google.com/new/accounts/LoginAuth"));

  // Match against an inexact copy
  std::vector<PasswordForm> result;
  EXPECT_TRUE(
      db().GetLogins(digest, /*should_PSL_matching_apply=*/true, &result));
  EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form)));
}

TEST_P(LoginDatabaseTest, GetLogins_ProtocolMismatch_HTTP) {
  PasswordForm form = GenerateExamplePasswordForm();
  ASSERT_TRUE(base::StartsWith(form.signon_realm, "http://"));
  ASSERT_EQ(AddChangeForForm(form), db().AddLogin(form));

  PasswordFormDigest digest(
      PasswordForm::Scheme::kHtml, "https://www.google.com/",
      GURL("https://www.google.com/new/accounts/LoginAuth"));

  // We have only an http record, so no match for this.
  std::vector<PasswordForm> result;
  EXPECT_TRUE(
      db().GetLogins(digest, /*should_PSL_matching_apply=*/true, &result));
  EXPECT_THAT(result, IsEmpty());
}

TEST_P(LoginDatabaseTest, GetLogins_ProtocolMismatch_HTTPS) {
  PasswordForm form = GenerateExamplePasswordForm();
  form.url = GURL("https://accounts.google.com/LoginAuth");
  form.signon_realm = "https://accounts.google.com/";
  ASSERT_EQ(AddChangeForForm(form), db().AddLogin(form));

  PasswordFormDigest digest(
      PasswordForm::Scheme::kHtml, "http://accounts.google.com/",
      GURL("http://accounts.google.com/new/accounts/LoginAuth"));

  // We have only an https record, so no match for this.
  std::vector<PasswordForm> result;
  EXPECT_TRUE(
      db().GetLogins(digest, /*should_PSL_matching_apply=*/true, &result));
  EXPECT_THAT(result, IsEmpty());
}

TEST_P(LoginDatabaseTest, AddLoginReturnsPrimaryKey) {
  std::vector<PasswordForm> result;

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

TEST_P(LoginDatabaseTest, RemoveLoginsByPrimaryKey) {
  std::vector<PasswordForm> result;

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
  EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form)));

  // RemoveLoginByPrimaryKey() doesn't decrypt or fill the password value.
  form.password_value = u"";

  EXPECT_TRUE(db().RemoveLoginByPrimaryKey(primary_key, &change_list));
  EXPECT_EQ(RemoveChangeForForm(form), change_list);
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_THAT(result, IsEmpty());
}

TEST_P(LoginDatabaseTest, ShouldNotRecyclePrimaryKeys) {
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

TEST_P(LoginDatabaseTest, TestPublicSuffixDomainMatching) {
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
  std::vector<PasswordForm> result;
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form2),
                             /*should_PSL_matching_apply=*/true, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ("https://foo.com/", result[0].signon_realm);

  // Do an exact match by excluding psl matches.
  result.clear();
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form2),
                             /*should_PSL_matching_apply=*/false, &result));
  EXPECT_THAT(result, IsEmpty());
}

TEST_P(LoginDatabaseTest, TestFederatedMatching) {
  std::vector<PasswordForm> result;

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
      url::SchemeHostPort(GURL("https://accounts.google.com/"));

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
  EXPECT_THAT(result, UnorderedElementsAre(HasPrimaryKeyAndEquals(form),
                                           HasPrimaryKeyAndEquals(form2)));

  // Match against the mobile site.
  form_request.url = GURL("https://mobile.foo.com/");
  form_request.signon_realm = "https://mobile.foo.com/";
  EXPECT_TRUE(db().GetLogins(form_request, /*should_PSL_matching_apply=*/true,
                             &result));
  EXPECT_THAT(result, UnorderedElementsAre(HasPrimaryKeyAndEquals(form),
                                           HasPrimaryKeyAndEquals(form2)));
}

TEST_P(LoginDatabaseTest, TestFederatedMatchingLocalhost) {
  PasswordForm form;
  form.url = GURL("http://localhost/");
  form.signon_realm = "federation://localhost/accounts.google.com";
  form.federation_origin =
      url::SchemeHostPort(GURL("https://accounts.google.com/"));
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
  std::vector<PasswordForm> result;
  EXPECT_TRUE(db().GetLogins(form_request, /*should_PSL_matching_apply=*/false,
                             &result));
  EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form)));

  form_request.url = GURL("http://localhost:8080/");
  form_request.signon_realm = "http://localhost:8080/";
  EXPECT_TRUE(db().GetLogins(form_request, /*should_PSL_matching_apply=*/false,
                             &result));
  EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form_with_port)));
}

class LoginDatabaseSchemesTest
    : public LoginDatabaseTestBase,
      public testing::WithParamInterface<PasswordForm::Scheme> {};

TEST_P(LoginDatabaseSchemesTest, TestPublicSuffixDisabled) {
  // The test is based on the different treatment for kHtml vs. non kHtml
  // schemes.
  if (GetParam() == PasswordForm::Scheme::kHtml) {
    return;
  }
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
  std::vector<PasswordForm> result;
  EXPECT_TRUE(db().GetLogins(second_non_html_auth,
                             /*should_PSL_matching_apply=*/false, &result));
  EXPECT_EQ(0U, result.size());

  // non-html auth still matches against itself.
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(non_html_auth),
                             /*should_PSL_matching_apply=*/false, &result));
  EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(non_html_auth)));
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
  std::vector<PasswordForm> result;
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(ip_form),
                             /*should_PSL_matching_apply=*/false, &result));
  EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(ip_form)));
}

INSTANTIATE_TEST_SUITE_P(Schemes,
                         LoginDatabaseSchemesTest,
                         testing::Values(PasswordForm::Scheme::kHtml,
                                         PasswordForm::Scheme::kBasic,
                                         PasswordForm::Scheme::kDigest,
                                         PasswordForm::Scheme::kOther));

TEST_P(LoginDatabaseTest, TestPublicSuffixDomainGoogle) {
  std::vector<PasswordForm> result;

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
  EXPECT_EQ(form.signon_realm, result[0].signon_realm);

  // There should be no PSL match on other subdomains.
  PasswordFormDigest form3 = {PasswordForm::Scheme::kHtml,
                              "https://some.other.google.com/",
                              GURL("https://some.other.google.com/")};

  EXPECT_TRUE(
      db().GetLogins(form3, /*should_PSL_matching_apply=*/true, &result));
  EXPECT_THAT(result, IsEmpty());
}

TEST_P(LoginDatabaseTest, TestFederatedMatchingWithoutPSLMatching) {
  std::vector<PasswordForm> result;

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
      url::SchemeHostPort(GURL("https://accounts.google.com/"));

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
  EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form)));

  // Match against the second one.
  form_request.url = form2.url;
  form_request.signon_realm = form2.signon_realm;
  EXPECT_TRUE(db().GetLogins(form_request, /*should_PSL_matching_apply=*/false,
                             &result));
  EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form2)));
}

TEST_P(LoginDatabaseTest, TestFederatedPSLMatching) {
  // Save a federated credential for the PSL matched site.
  PasswordForm form;
  form.url = GURL("https://psl.example.com/");
  form.action = GURL("https://psl.example.com/login");
  form.signon_realm = "federation://psl.example.com/accounts.google.com";
  form.username_value = u"test1@gmail.com";
  form.type = PasswordForm::Type::kApi;
  form.federation_origin =
      url::SchemeHostPort(GURL("https://accounts.google.com/"));
  form.scheme = PasswordForm::Scheme::kHtml;
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  // When we retrieve the form from the store, it should have |in_store| set.
  form.in_store = PasswordForm::Store::kProfileStore;

  // Match against.
  PasswordFormDigest form_request = {PasswordForm::Scheme::kHtml,
                                     "https://example.com/",
                                     GURL("https://example.com/login")};
  std::vector<PasswordForm> result;
  EXPECT_TRUE(db().GetLogins(form_request, /*should_PSL_matching_apply=*/true,
                             &result));
  EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form)));
}

// This test fails if the implementation of GetLogins uses GetCachedStatement
// instead of GetUniqueStatement, since REGEXP is in use. See
// http://crbug.com/248608.
TEST_P(LoginDatabaseTest, TestPublicSuffixDomainMatchingDifferentSites) {
  std::vector<PasswordForm> result;

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
  EXPECT_EQ("https://foo.com/", result[0].signon_realm);
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
  EXPECT_EQ("https://baz.com/", result[0].signon_realm);
}

PasswordForm GetFormWithNewSignonRealm(PasswordForm form,
                                       const std::string& signon_realm) {
  PasswordForm form2(form);
  form2.url = GURL(signon_realm);
  form2.action = GURL(signon_realm);
  form2.signon_realm = signon_realm;
  return form2;
}

TEST_P(LoginDatabaseTest, TestPublicSuffixDomainMatchingRegexp) {
  std::vector<PasswordForm> result;

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
      url::SchemeHostPort(GURL("https://accounts.google.com/"));
  form.skip_zero_click = true;

  if (date_is_creation) {
    form.date_created = time;
  }
  return db->AddLogin(form) == AddChangeForForm(form);
}

TEST_P(LoginDatabaseTest, ClearPrivateData_SavedPasswords) {
  std::vector<PasswordForm> result;

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
  std::vector<PasswordForm> forms;
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

TEST_P(LoginDatabaseTest, ClearPrivateData_SavedMaxCreatedTimePasswords) {
  // Create one with Max time.
  EXPECT_TRUE(AddTimestampedLogin(&db(), "http://1.com", "foo1",
                                  base::Time::Max(), true));

  std::vector<PasswordForm> forms;

  // Get all time logins.
  EXPECT_TRUE(
      db().GetLoginsCreatedBetween(base::Time(), base::Time::Max(), &forms));
  EXPECT_EQ(1U, forms.size());

  // Delete with Max date (should delete all).
  PasswordStoreChangeList changes;
  db().RemoveLoginsCreatedBetween(base::Time(), base::Time::Max(), &changes);
  ASSERT_EQ(1U, changes.size());

  EXPECT_EQ(forms[0], changes[0].form());
  forms.clear();

  // Verify nothing is left.
  EXPECT_TRUE(db().GetAutofillableLogins(&forms));
  EXPECT_EQ(0U, forms.size());
}

TEST_P(LoginDatabaseTest, GetAutoSignInLogins) {
  std::vector<PasswordForm> forms;

  GURL origin("https://example.com");
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo1", origin));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo2", origin));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo3", origin));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo4", origin));

  EXPECT_TRUE(db().GetAutoSignInLogins(&forms));
  EXPECT_EQ(4U, forms.size());
  for (const auto& form : forms) {
    EXPECT_FALSE(form.skip_zero_click);
  }

  EXPECT_TRUE(db().DisableAutoSignInForOrigin(origin));
  EXPECT_TRUE(db().GetAutoSignInLogins(&forms));
  EXPECT_EQ(0U, forms.size());
}

TEST_P(LoginDatabaseTest, DisableAutoSignInForOrigin) {
  std::vector<PasswordForm> result;

  GURL origin1("https://google.com");
  GURL origin2("https://chrome.com");
  GURL origin3("http://example.com");
  GURL origin4("http://localhost");
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo1", origin1));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo2", origin2));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo3", origin3));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo4", origin4));

  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  for (const auto& form : result) {
    EXPECT_FALSE(form.skip_zero_click);
  }

  EXPECT_TRUE(db().DisableAutoSignInForOrigin(origin1));
  EXPECT_TRUE(db().DisableAutoSignInForOrigin(origin3));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  for (const auto& form : result) {
    if (form.url == origin1 || form.url == origin3) {
      EXPECT_TRUE(form.skip_zero_click);
    } else {
      EXPECT_FALSE(form.skip_zero_click);
    }
  }
}

TEST_P(LoginDatabaseTest, BlocklistedLogins) {
  std::vector<PasswordForm> result;

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
      url::SchemeHostPort(GURL("https://accounts.google.com/"));
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
  EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form)));

  // So should GetBlocklistedLogins.
  EXPECT_TRUE(db().GetBlocklistLogins(&result));
  EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form)));
}

TEST_P(LoginDatabaseTest, VectorSerialization) {
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

TEST_P(LoginDatabaseTest, GaiaIdHashVectorSerialization) {
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

TEST_P(LoginDatabaseTest, UpdateIncompleteCredentials) {
  std::vector<PasswordForm> result;
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
  EXPECT_EQ(incomplete_form.url, result[0].url);
  EXPECT_EQ(incomplete_form.signon_realm, result[0].signon_realm);
  EXPECT_EQ(incomplete_form.username_value, result[0].username_value);
  EXPECT_EQ(incomplete_form.password_value, result[0].password_value);
  EXPECT_EQ(incomplete_form.date_last_used, result[0].date_last_used);

  // We should return empty 'action', 'username_element', 'password_element'
  // and 'submit_element' as we can't be sure if the credentials were entered
  // in this particular form on the page.
  EXPECT_EQ(GURL(), result[0].action);
  EXPECT_TRUE(result[0].username_element.empty());
  EXPECT_TRUE(result[0].password_element.empty());
  EXPECT_TRUE(result[0].submit_element.empty());
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
  // TODO(crbug.com/40774419): Once all places that operate changes on forms
  // via UpdateLogin properly set |password_issues|, setting them to an empty
  // map should be part of the default constructor.
  expected_form.password_issues =
      base::flat_map<InsecureType, InsecurityMetadata>();
  EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(expected_form)));
}

TEST_P(LoginDatabaseTest, UpdateOverlappingCredentials) {
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
  std::vector<PasswordForm> result;
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(2U, result.size());
  result.clear();

  // TODO(crbug.com/40774419): Once all places that operate changes on forms
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

  EXPECT_THAT(result,
              UnorderedElementsAre(HasPrimaryKeyAndEquals(complete_form),
                                   HasPrimaryKeyAndEquals(incomplete_form)));
}

TEST_P(LoginDatabaseTest, DoubleAdd) {
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

TEST_P(LoginDatabaseTest, AddWrongForm) {
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

#if BUILDFLAG(IS_IOS)
// Test that when adding a login with no password_value but with
// keychain_identifier, the keychain_identifier is kept and the password_value
// is filled in with the decrypted password.
TEST_P(LoginDatabaseTest, AddLoginWithEncryptedPassword) {
  PasswordForm form;
  form.url = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = u"my_username";
  std::string keychain_identifier;
  EXPECT_TRUE(
      CreateKeychainIdentifier(u"my_encrypted_password", &keychain_identifier));
  form.keychain_identifier = keychain_identifier;
  form.blocked_by_user = false;
  form.scheme = PasswordForm::Scheme::kHtml;

  // |AddLogin| will decrypt the encrypted password, so compare with that.
  PasswordForm form_with_password = form;
  form_with_password.password_value = u"my_encrypted_password";
  EXPECT_EQ(AddChangeForForm(form_with_password), db().AddLogin(form));

  std::vector<PasswordForm> result;
  ASSERT_TRUE(db().GetLogins(PasswordFormDigest(form),
                             /*should_PSL_matching_apply=*/true, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form.keychain_identifier, result[0].keychain_identifier);
  EXPECT_EQ(u"my_encrypted_password", result[0].password_value);

  std::u16string decrypted;
  EXPECT_EQ(errSecSuccess, GetTextFromKeychainIdentifier(
                               result[0].keychain_identifier, &decrypted));
  EXPECT_EQ(u"my_encrypted_password", decrypted);
}

// Test that when adding a login with password_value but with
// keychain_identifier, the keychain_identifier is discarded.
TEST_P(LoginDatabaseTest, AddLoginWithEncryptedPasswordAndValue) {
  PasswordForm form;
  form.url = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = u"my_username";
  form.password_value = u"my_password_value";
  std::string keychain_identifier;
  EXPECT_TRUE(
      CreateKeychainIdentifier(u"my_encrypted_password", &keychain_identifier));
  form.keychain_identifier = keychain_identifier;
  form.blocked_by_user = false;
  form.scheme = PasswordForm::Scheme::kHtml;
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  std::vector<PasswordForm> result;
  ASSERT_TRUE(db().GetLogins(PasswordFormDigest(form),
                             /*should_PSL_matching_apply=*/true, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_NE(form.keychain_identifier, result[0].keychain_identifier);

  std::u16string decrypted;
  EXPECT_EQ(errSecSuccess, GetTextFromKeychainIdentifier(
                               result[0].keychain_identifier, &decrypted));
  EXPECT_EQ(u"my_password_value", decrypted);
}
#endif

TEST_P(LoginDatabaseTest, UpdateLogin) {
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
      url::SchemeHostPort(GURL("https://accounts.google.com/"));
  form.skip_zero_click = true;
  form.moving_blocked_for_list.push_back(GaiaIdHash::FromGaiaId("gaia_id"));

  PasswordStoreChangeList changes = db().UpdateLogin(form);
  EXPECT_EQ(UpdateChangeForForm(form, /*password_changed=*/true), changes);
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(1, changes[0].form().primary_key.value().value());

  // When we retrieve the form from the store, it should have |in_store| set.
  form.in_store = PasswordForm::Store::kProfileStore;

  std::vector<PasswordForm> result;
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form),
                             /*should_PSL_matching_apply=*/true, &result));
  EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form)));
}

TEST_P(LoginDatabaseTest, UpdateLoginWithoutPassword) {
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

  std::vector<PasswordForm> result;
  ASSERT_TRUE(db().GetLogins(PasswordFormDigest(form),
                             /*should_PSL_matching_apply=*/true, &result));
  EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form)));
}

TEST_P(LoginDatabaseTest, RemoveWrongForm) {
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

TEST_P(LoginDatabaseTest, ReportMetricsTest) {
  AddMetricsTestData(&db());

  // Note: We also create and populate an account DB here and instruct it to
  // report metrics, even though all the checks below only test the profile DB.
  // This is to make sure that the account DB doesn't write to any of the same
  // histograms.
  base::FilePath account_db_file =
      temp_dir_.GetPath().AppendASCII("TestAccountStoreDatabase");
  LoginDatabase account_db(account_db_file, IsAccountStore(true));
  ASSERT_TRUE(account_db.Init(
      /*on_undecryptable_passwords_removed=*/base::NullCallback(),
      /*encryptor=*/encryptor()));
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
TEST_P(LoginDatabaseTest, ReportAccountStoreMetricsTest) {
  // Note: We also populate the profile DB here and instruct it to report
  // metrics, even though all the checks below only test the account DB. This is
  // to make sure that the profile DB doesn't write to any of the same
  // histograms.
  AddMetricsTestData(&db());

  base::FilePath account_db_file =
      temp_dir_.GetPath().AppendASCII("TestAccountStoreDatabase");
  LoginDatabase account_db(account_db_file, IsAccountStore(true));
  ASSERT_TRUE(account_db.Init(
      /*on_undecryptable_passwords_removed=*/base::NullCallback(),
      /*encryptor=*/encryptor()));
  AddMetricsTestData(&account_db);

  base::HistogramTester histogram_tester;
  db().ReportMetrics();
  account_db.ReportMetrics();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.InaccessiblePasswords3", 0, 1);
}

class LoginDatabaseSyncMetadataTest : public LoginDatabaseTestBase {
 public:
  syncer::DataType SyncDataType() { return syncer::PASSWORDS; }
};

TEST_F(LoginDatabaseSyncMetadataTest, NoMetadata) {
  std::unique_ptr<syncer::MetadataBatch> metadata_batch =
      db().password_sync_metadata_store().GetAllSyncMetadata(SyncDataType());
  ASSERT_THAT(metadata_batch, testing::NotNull());
  EXPECT_EQ(0u, metadata_batch->TakeAllMetadata().size());
  EXPECT_EQ(sync_pb::DataTypeState().SerializeAsString(),
            metadata_batch->GetDataTypeState().SerializeAsString());
}

TEST_F(LoginDatabaseSyncMetadataTest, GetAllSyncMetadata) {
  sync_pb::EntityMetadata metadata;
  PasswordStoreSync::MetadataStore& password_sync_metadata_store =
      db().password_sync_metadata_store();
  // Storage keys must be integers.
  const std::string kStorageKey1 = "1";
  const std::string kStorageKey2 = "2";
  metadata.set_sequence_number(1);

  EXPECT_TRUE(password_sync_metadata_store.UpdateEntityMetadata(
      SyncDataType(), kStorageKey1, metadata));

  sync_pb::DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  EXPECT_TRUE(password_sync_metadata_store.UpdateDataTypeState(
      SyncDataType(), data_type_state));

  metadata.set_sequence_number(2);
  EXPECT_TRUE(password_sync_metadata_store.UpdateEntityMetadata(
      SyncDataType(), kStorageKey2, metadata));

  std::unique_ptr<syncer::MetadataBatch> metadata_batch =
      password_sync_metadata_store.GetAllSyncMetadata(SyncDataType());
  ASSERT_THAT(metadata_batch, testing::NotNull());

  EXPECT_EQ(metadata_batch->GetDataTypeState().initial_sync_state(),
            sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  syncer::EntityMetadataMap metadata_records =
      metadata_batch->TakeAllMetadata();

  EXPECT_EQ(metadata_records.size(), 2u);
  EXPECT_EQ(metadata_records[kStorageKey1]->sequence_number(), 1);
  EXPECT_EQ(metadata_records[kStorageKey2]->sequence_number(), 2);

  // Now check that a data type state update replaces the old value
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED);
  EXPECT_TRUE(password_sync_metadata_store.UpdateDataTypeState(
      SyncDataType(), data_type_state));

  metadata_batch =
      password_sync_metadata_store.GetAllSyncMetadata(SyncDataType());
  ASSERT_THAT(metadata_batch, testing::NotNull());
  EXPECT_EQ(
      metadata_batch->GetDataTypeState().initial_sync_state(),
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED);
}

TEST_F(LoginDatabaseSyncMetadataTest, GetSyncEntityMetadataForStorageKey) {
  // Construct metadata with at least one field set to test deserialization.
  sync_pb::EntityMetadata metadata;
  metadata.set_is_deleted(true);

  PasswordStoreSync::MetadataStore& password_sync_metadata_store =
      db().password_sync_metadata_store();

  // Storage keys must be integers.
  const std::string kStorageKey1 = "1";
  metadata.set_sequence_number(1);

  ASSERT_TRUE(password_sync_metadata_store.UpdateEntityMetadata(
      SyncDataType(), kStorageKey1, metadata));

  LoginDatabase::SyncMetadataStore& store_impl =
      static_cast<LoginDatabase::SyncMetadataStore&>(
          password_sync_metadata_store);

  const std::unique_ptr<sync_pb::EntityMetadata> entity_metadata =
      store_impl.GetSyncEntityMetadataForStorageKeyForTest(syncer::PASSWORDS,
                                                           kStorageKey1);
  ASSERT_THAT(entity_metadata, testing::NotNull());
  EXPECT_TRUE(entity_metadata->is_deleted());

  // Other arbitrary storage keys should return no metadata.
  EXPECT_THAT(store_impl.GetSyncEntityMetadataForStorageKeyForTest(
                  syncer::PASSWORDS, "5"),
              testing::IsNull());
}

TEST_F(LoginDatabaseSyncMetadataTest, DeleteAllSyncMetadata) {
  sync_pb::EntityMetadata metadata;
  PasswordStoreSync::MetadataStore& password_sync_metadata_store =
      db().password_sync_metadata_store();
  // Storage keys must be integers.
  const std::string kStorageKey1 = "1";
  const std::string kStorageKey2 = "2";
  metadata.set_sequence_number(1);

  EXPECT_TRUE(password_sync_metadata_store.UpdateEntityMetadata(
      SyncDataType(), kStorageKey1, metadata));

  sync_pb::DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  EXPECT_TRUE(password_sync_metadata_store.UpdateDataTypeState(
      SyncDataType(), data_type_state));

  metadata.set_sequence_number(2);
  EXPECT_TRUE(password_sync_metadata_store.UpdateEntityMetadata(
      SyncDataType(), kStorageKey2, metadata));

  std::unique_ptr<syncer::MetadataBatch> metadata_batch =
      password_sync_metadata_store.GetAllSyncMetadata(SyncDataType());
  ASSERT_THAT(metadata_batch, testing::NotNull());
  ASSERT_EQ(metadata_batch->TakeAllMetadata().size(), 2u);

  password_sync_metadata_store.DeleteAllSyncMetadata(SyncDataType());

  std::unique_ptr<syncer::MetadataBatch> empty_metadata_batch =
      password_sync_metadata_store.GetAllSyncMetadata(SyncDataType());
  ASSERT_THAT(empty_metadata_batch, testing::NotNull());
  EXPECT_EQ(empty_metadata_batch->TakeAllMetadata().size(), 0u);
}

TEST_F(LoginDatabaseSyncMetadataTest, WriteThenDeleteSyncMetadata) {
  sync_pb::EntityMetadata metadata;
  PasswordStoreSync::MetadataStore& password_sync_metadata_store =
      db().password_sync_metadata_store();
  const std::string kStorageKey = "1";
  sync_pb::DataTypeState data_type_state;

  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  metadata.set_client_tag_hash("client_hash");

  // Write the data into the store.
  EXPECT_TRUE(password_sync_metadata_store.UpdateEntityMetadata(
      SyncDataType(), kStorageKey, metadata));
  EXPECT_TRUE(password_sync_metadata_store.UpdateDataTypeState(
      SyncDataType(), data_type_state));
  // Delete the data we just wrote.
  EXPECT_TRUE(password_sync_metadata_store.ClearEntityMetadata(SyncDataType(),
                                                               kStorageKey));

  std::unique_ptr<syncer::MetadataBatch> metadata_batch =
      password_sync_metadata_store.GetAllSyncMetadata(SyncDataType());
  ASSERT_THAT(metadata_batch, testing::NotNull());

  // It shouldn't be there any more.
  syncer::EntityMetadataMap metadata_records =
      metadata_batch->TakeAllMetadata();
  EXPECT_EQ(metadata_records.size(), 0u);

  // Now delete the data type state.
  EXPECT_TRUE(password_sync_metadata_store.ClearDataTypeState(SyncDataType()));
  metadata_batch =
      password_sync_metadata_store.GetAllSyncMetadata(SyncDataType());
  ASSERT_THAT(metadata_batch, testing::NotNull());

  EXPECT_EQ(sync_pb::DataTypeState().SerializeAsString(),
            metadata_batch->GetDataTypeState().SerializeAsString());
}

TEST_F(LoginDatabaseSyncMetadataTest, HasUnsyncedPasswordDeletions) {
  sync_pb::EntityMetadata tombstone_metadata;
  tombstone_metadata.set_is_deleted(true);
  tombstone_metadata.set_sequence_number(1);

  sync_pb::EntityMetadata non_tombstone_metadata;
  non_tombstone_metadata.set_is_deleted(false);
  non_tombstone_metadata.set_sequence_number(1);

  PasswordStoreSync::MetadataStore& password_sync_metadata_store =
      db().password_sync_metadata_store();

  EXPECT_FALSE(password_sync_metadata_store.HasUnsyncedPasswordDeletions());

  // Storage keys must be integers.
  const std::string kTombstoneStorageKey = "1";
  const std::string kNonTombstoneStorageKey = "2";

  ASSERT_TRUE(password_sync_metadata_store.UpdateEntityMetadata(
      SyncDataType(), kTombstoneStorageKey, tombstone_metadata));
  ASSERT_TRUE(password_sync_metadata_store.UpdateEntityMetadata(
      SyncDataType(), kNonTombstoneStorageKey, non_tombstone_metadata));

  EXPECT_TRUE(password_sync_metadata_store.HasUnsyncedPasswordDeletions());

  // Delete the only metadata entry representing a deletion.
  ASSERT_TRUE(password_sync_metadata_store.ClearEntityMetadata(
      SyncDataType(), kTombstoneStorageKey));

  EXPECT_FALSE(password_sync_metadata_store.HasUnsyncedPasswordDeletions());
}

#if BUILDFLAG(IS_POSIX)
// Only the current user has permission to read the database.
//
// Only POSIX because GetPosixFilePermissions() only exists on POSIX.
// This tests that sql::Database::set_restrict_to_user() was called,
// and that function is a noop on non-POSIX platforms in any case.
TEST_P(LoginDatabaseTest, FilePermissions) {
  int mode = base::FILE_PERMISSION_MASK;
  EXPECT_TRUE(base::GetPosixFilePermissions(file_, &mode));
  EXPECT_EQ((mode & base::FILE_PERMISSION_USER_MASK), mode);
}
#endif  // BUILDFLAG(IS_POSIX)

#if !BUILDFLAG(IS_IOS)
// Test that LoginDatabase encrypts the password values that it stores.
TEST_P(LoginDatabaseTest, EncryptionEnabled) {
  PasswordForm password_form = GenerateExamplePasswordForm();
  base::FilePath file = temp_dir_.GetPath().AppendASCII("TestUnencryptedDB");

  {
    LoginDatabase db(file, IsAccountStore(false));
    ASSERT_TRUE(
        db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
                /*encryptor=*/encryptor()));
    EXPECT_EQ(AddChangeForForm(password_form), db.AddLogin(password_form));
  }
  std::u16string decrypted_pw;
  if (GetParam()) {
    ASSERT_TRUE(encryptor()->DecryptString16(
        GetColumnValuesFromDatabase<std::string>(file, "password_value").at(0),
        &decrypted_pw));
  } else {
    ASSERT_TRUE(OSCrypt::DecryptString16(
        GetColumnValuesFromDatabase<std::string>(file, "password_value").at(0),
        &decrypted_pw));
  }

  EXPECT_EQ(decrypted_pw, password_form.password_value);
}
#endif  // !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
// On Android and ChromeOS there is a mix of plain-text and obfuscated
// passwords. Verify that they can both be accessed. Obfuscated passwords start
// with "v10". Some password values also start with "v10". Test that both are
// accessible (this doesn't work for any plain-text value).
TEST_P(LoginDatabaseTest, HandleObfuscationMix) {
  const char k_obfuscated_pw[] = "v10pass1";
  const char16_t k_obfuscated_pw16[] = u"v10pass1";
  const char k_plain_text_pw1[] = "v10pass2";
  const char16_t k_plain_text_pw116[] = u"v10pass2";
  const char k_plain_text_pw2[] = "v11pass3";
  const char16_t k_plain_text_pw216[] = u"v11pass3";

  base::FilePath file = temp_dir_.GetPath().AppendASCII("TestUnencryptedDB");
  {
    LoginDatabase db(file, IsAccountStore(false));
    ASSERT_TRUE(
        db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
                /*encryptor=*/encryptor()));
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

  std::vector<PasswordForm> forms;
  {
    LoginDatabase db(file, IsAccountStore(false));
    ASSERT_TRUE(
        db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
                /*encryptor=*/encryptor()));
    ASSERT_TRUE(db.GetAutofillableLogins(&forms));
  }

  // On disk, unobfuscated passwords are as-is, while obfuscated passwords have
  // been changed (obfuscated).
  EXPECT_THAT(GetColumnValuesFromDatabase<std::string>(file, "password_value"),
              UnorderedElementsAre(Ne(k_obfuscated_pw), k_plain_text_pw1,
                                   k_plain_text_pw2));
  // LoginDatabase serves the original values.
  EXPECT_THAT(forms,
              UnorderedElementsAre(
                  Field(&PasswordForm::password_value, k_obfuscated_pw16),
                  Field(&PasswordForm::password_value, k_plain_text_pw116),
                  Field(&PasswordForm::password_value, k_plain_text_pw216)));
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)

// If the database initialisation fails, the initialisation transaction should
// roll back without crashing.
TEST_P(LoginDatabaseTest, Init_NoCrashOnFailedRollback) {
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
  EXPECT_FALSE(
      db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
              /*encryptor=*/encryptor()));
}

// If the database version is from the future, it shouldn't be downgraded.
TEST_P(LoginDatabaseTest, ShouldNotDowngradeDatabaseVersion) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath database_path = temp_dir.GetPath().AppendASCII("test.db");

  const int kDBFutureVersion = kCurrentVersionNumber + 1000;

  {
    // Open a database with the current version.
    LoginDatabase db(database_path, IsAccountStore(false));
    EXPECT_TRUE(
        db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
                /*encryptor=*/encryptor()));
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
    EXPECT_TRUE(
        db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
                /*encryptor=*/encryptor()));
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

// Test the migration from `std::get<0>(GetParam())` version to
// `kCurrentVersionNumber`.
// `std::get<1>(GetParam())` controls whether `os_crypt_async::Encryptor` is
// used.
class LoginDatabaseMigrationTest
    : public testing::TestWithParam<std::tuple<int, bool>> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    database_dump_location_ = database_dump_location_.AppendASCII("components")
                                  .AppendASCII("test")
                                  .AppendASCII("data")
                                  .AppendASCII("password_manager");
    database_path_ = temp_dir_.GetPath().AppendASCII("test.db");
    OSCryptMocker::SetUp();
    if (std::get<1>(GetParam())) {
      test_oscrypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting(
          /*is_sync_for_unittests = */ true);
    }
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  // Creates the database from |sql_file|.
  void CreateDatabase(std::string_view sql_file) {
    base::FilePath database_dump;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &database_dump));
    database_dump =
        database_dump.Append(database_dump_location_).AppendASCII(sql_file);
    ASSERT_TRUE(
        sql::test::CreateDatabaseFromSQL(database_path_, database_dump));
  }

  void DestroyDatabase() {
    if (!database_path_.empty()) {
      sql::Database::Delete(database_path_);
    }
  }

  // Returns the database version for the test.
  int version() const { return std::get<0>(GetParam()); }

  // Actual test body.
  void MigrationToVCurrent(std::string_view sql_file);

  base::FilePath database_path_;

  void AdvanceTime(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  std::unique_ptr<os_crypt_async::Encryptor> encryptor() {
    if (std::get<1>(GetParam())) {
      return std::make_unique<os_crypt_async::Encryptor>(
          GetInstanceSync(test_oscrypt_async_.get()));
    }
    return nullptr;
  }

 private:
  base::FilePath database_dump_location_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> test_oscrypt_async_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

void LoginDatabaseMigrationTest::MigrationToVCurrent(
    std::string_view sql_file) {
  AdvanceTime(base::Days(10));
  SCOPED_TRACE(testing::Message("Version file = ") << sql_file);
  CreateDatabase(sql_file);

  {
    // Assert that the database was successfully opened and updated
    // to current version.
    LoginDatabase db(database_path_, IsAccountStore(false));
    ASSERT_TRUE(
        db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
                /*encryptor=*/encryptor()));

    // Check that the contents was preserved.
    std::vector<PasswordForm> result;
    EXPECT_TRUE(db.GetAutofillableLogins(&result));
    EXPECT_THAT(result,
                UnorderedElementsAre(IsGoogle1Account(), IsGoogle2Account(),
                                     IsBasicAuthAccount()));

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
    EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form)));
    EXPECT_TRUE(db.RemoveLogin(form, /*changes=*/nullptr));

    if (version() == 31) {
      // Check that unset values of 'insecure_credentials.create_time' are set
      // to current time.
      std::vector<InsecureCredential> insecure_credentials(
          db.insecure_credentials_table().GetRows(FormPrimaryKey(1)));
      ASSERT_EQ(2U, insecure_credentials.size());
      EXPECT_EQ(insecure_credentials[0].create_time, base::Time::Now());
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

INSTANTIATE_TEST_SUITE_P(
    MigrationToVCurrent,
    LoginDatabaseMigrationTest,
    testing::Combine(testing::Range(1, kCurrentVersionNumber + 1),
                     testing::Bool()));
INSTANTIATE_TEST_SUITE_P(MigrationToVCurrent,
                         LoginDatabaseMigrationTestV9,
                         testing::Combine(testing::Values(9), testing::Bool()));
INSTANTIATE_TEST_SUITE_P(MigrationToVCurrent,
                         LoginDatabaseMigrationTestBroken,
                         testing::Combine(testing::Values(1, 2, 3, 24),
                                          testing::Bool()));

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_IOS) || \
    BUILDFLAG(IS_WIN)
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
    env_ = base::Environment::Create();
  }

  void TearDown() override {
    OSCryptMocker::TearDown();
    if (env_->HasVar("CHROME_USER_DATA_DIR")) {
      env_->UnSetVar("CHROME_USER_DATA_DIR");
    }
  }

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

  base::Environment* env() { return env_.get(); }

 private:
  std::unique_ptr<base::Environment> env_;
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
    EXPECT_TRUE(
        db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
                /*encryptor=*/nullptr));
    EXPECT_EQ(db.AddLogin(form), AddChangeForForm(form));
  }

  if (should_be_corrupted) {
    sql::Database db;
    EXPECT_TRUE(db.Open(database_path()));

    // Change encrypted password in the database if the login should be
    // corrupted.
    static constexpr char kStatement[] =
        "UPDATE logins SET password_value = password_value || 'trash' "
        "WHERE signon_realm = ? AND username_value = ?";
    sql::Statement s(db.GetCachedStatement(SQL_FROM_HERE, kStatement));
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
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kSkipUndecryptablePasswords);
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
  NiceMock<base::MockCallback<LoginDatabase::IsEmptyCallback>> is_empty_cb;
  db.SetIsEmptyCb(is_empty_cb.Get());
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(
      db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
              /*encryptor=*/nullptr));

#if BUILDFLAG(IS_CASTOS)
  // Disabling the checks in chromecast because encryption is unavailable.
  EXPECT_EQ(DatabaseCleanupResult::kEncryptionUnavailable,
            db.DeleteUndecryptableLogins());
#else
  std::vector<PasswordForm> result;
  EXPECT_FALSE(db.GetAutofillableLogins(&result));
  EXPECT_TRUE(result.empty());
  EXPECT_FALSE(db.GetBlocklistLogins(&result));
  EXPECT_TRUE(result.empty());

  // Delete undecryptable logins and make sure we can get valid logins.
  // `is_empty_cb_` is called more than once because DeleteUndecryptableLogins()
  // internally calls RemoveLogin() for each form.
  EXPECT_CALL(is_empty_cb, Run(LoginDatabase::LoginDatabaseEmptinessState{
                               .no_login_found = false,
                               .autofillable_credentials_exist = true}))
      .Times(AnyNumber());
  EXPECT_EQ(DatabaseCleanupResult::kSuccess, db.DeleteUndecryptableLogins());
  EXPECT_TRUE(db.GetAutofillableLogins(&result));
  EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form1)));

  EXPECT_TRUE(db.GetBlocklistLogins(&result));
  EXPECT_THAT(result, IsEmpty());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.DeleteUndecryptableLoginsReturnValue",
      metrics_util::DeleteCorruptedPasswordsResult::kSuccessPasswordsDeleted,
      1);
#endif
}

#if BUILDFLAG(IS_LINUX)
TEST_F(LoginDatabaseUndecryptableLoginsTest,
       DontDeleteUndecryptableLoginsIfStoreSwitchTest) {
  // Init with feature states allowing for password deletion.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureStates(
      {{features::kSkipUndecryptablePasswords, false},
       {features::kClearUndecryptablePasswords, true}});

  // Set the password store switch
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      password_manager::kPasswordStore);

  base::HistogramTester histogram_tester;
  std::vector<PasswordForm> forms;
  auto form1 =
      AddDummyLogin("foo1", GURL("https://foo1.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  auto form2 =
      AddDummyLogin("foo2", GURL("https://foo2.com/"),
                    /*should_be_corrupted=*/true, /*blocklisted=*/false);
  auto form3 =
      AddDummyLogin("foo3", GURL("https://foo3.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  NiceMock<base::MockCallback<LoginDatabase::OnUndecryptablePasswordsRemoved>>
      on_undecryptable_passwords_removed;

  LoginDatabase db(database_path(), IsAccountStore(false));
  ASSERT_TRUE(db.Init(on_undecryptable_passwords_removed.Get(), nullptr));

  EXPECT_CALL(on_undecryptable_passwords_removed, Run).Times(0);
  EXPECT_FALSE(db.GetAutoSignInLogins(&forms));
  histogram_tester.ExpectTotalCount(
      "PasswordManager.DeleteUndecryptableLoginsReturnValue", 0);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LoginDatabase.ShouldDeleteUndecryptablePasswords", 3, 1);
}

TEST_F(LoginDatabaseUndecryptableLoginsTest,
       DontDeleteUndecryptableLoginsIfEncryptionSelectionSwitchTest) {
  // Init with feature states allowing for password deletion.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureStates(
      {{features::kSkipUndecryptablePasswords, false},
       {features::kClearUndecryptablePasswords, true}});

  // Set the ecryption selection switch
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      password_manager::kEnableEncryptionSelection);

  base::HistogramTester histogram_tester;
  std::vector<PasswordForm> forms;
  auto form1 =
      AddDummyLogin("foo1", GURL("https://foo1.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  auto form2 =
      AddDummyLogin("foo2", GURL("https://foo2.com/"),
                    /*should_be_corrupted=*/true, /*blocklisted=*/false);
  auto form3 =
      AddDummyLogin("foo3", GURL("https://foo3.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);

  NiceMock<base::MockCallback<LoginDatabase::OnUndecryptablePasswordsRemoved>>
      on_undecryptable_passwords_removed;

  LoginDatabase db(database_path(), IsAccountStore(false));
  ASSERT_TRUE(db.Init(on_undecryptable_passwords_removed.Get(), nullptr));

  EXPECT_CALL(on_undecryptable_passwords_removed, Run).Times(0);
  EXPECT_FALSE(db.GetAutoSignInLogins(&forms));
  histogram_tester.ExpectTotalCount(
      "PasswordManager.DeleteUndecryptableLoginsReturnValue", 0);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LoginDatabase.ShouldDeleteUndecryptablePasswords", 4, 1);
}

TEST_F(LoginDatabaseUndecryptableLoginsTest,
       DontDeleteUndecryptableLoginsIfEnvVariableSetTest) {
  // Init with feature states allowing for password deletion.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureStates(
      {{features::kSkipUndecryptablePasswords, false},
       {features::kClearUndecryptablePasswords, true}});

  // Set the home dir env variable.
  env()->SetVar("CHROME_USER_DATA_DIR", "test/path");

  base::HistogramTester histogram_tester;
  std::vector<PasswordForm> forms;
  auto form1 =
      AddDummyLogin("foo1", GURL("https://foo1.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  auto form2 =
      AddDummyLogin("foo2", GURL("https://foo2.com/"),
                    /*should_be_corrupted=*/true, /*blocklisted=*/false);
  auto form3 =
      AddDummyLogin("foo3", GURL("https://foo3.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  NiceMock<base::MockCallback<LoginDatabase::OnUndecryptablePasswordsRemoved>>
      on_undecryptable_passwords_removed;

  LoginDatabase db(database_path(), IsAccountStore(false));
  ASSERT_TRUE(db.Init(on_undecryptable_passwords_removed.Get(), nullptr));

  EXPECT_CALL(on_undecryptable_passwords_removed, Run).Times(0);
  EXPECT_FALSE(db.GetAutoSignInLogins(&forms));
  histogram_tester.ExpectTotalCount(
      "PasswordManager.DeleteUndecryptableLoginsReturnValue", 0);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LoginDatabase.ShouldDeleteUndecryptablePasswords", 1, 1);
}

#endif  // BUILDFLAG(IS_LINUX)

TEST_F(LoginDatabaseUndecryptableLoginsTest,
       DontDeleteUndecryptableLoginsIfUserDataDirSwitchTest) {
  // Init with feature states allowing for password deletion.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureStates(
      {{features::kSkipUndecryptablePasswords, false},
       {features::kClearUndecryptablePasswords, true}});

  // Set the user data directory switch
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      password_manager::kUserDataDir);

  base::HistogramTester histogram_tester;
  std::vector<PasswordForm> forms;
  auto form1 =
      AddDummyLogin("foo1", GURL("https://foo1.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  auto form2 =
      AddDummyLogin("foo2", GURL("https://foo2.com/"),
                    /*should_be_corrupted=*/true, /*blocklisted=*/false);
  auto form3 =
      AddDummyLogin("foo3", GURL("https://foo3.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  NiceMock<base::MockCallback<LoginDatabase::OnUndecryptablePasswordsRemoved>>
      on_undecryptable_passwords_removed;

  LoginDatabase db(database_path(), IsAccountStore(false));
  ASSERT_TRUE(db.Init(on_undecryptable_passwords_removed.Get(), nullptr));

  EXPECT_CALL(on_undecryptable_passwords_removed, Run).Times(0);
  EXPECT_FALSE(db.GetAutoSignInLogins(&forms));
  histogram_tester.ExpectTotalCount(
      "PasswordManager.DeleteUndecryptableLoginsReturnValue", 0);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LoginDatabase.ShouldDeleteUndecryptablePasswords", 2, 1);
}

#if BUILDFLAG(IS_MAC)
TEST_F(LoginDatabaseUndecryptableLoginsTest,
       DontDeleteUndecryptableLoginsIfEncryptionNotAvailiableTest) {
  // Init with feature states allowing for password deletion.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureStates(
      {{features::kSkipUndecryptablePasswords, false},
       {features::kClearUndecryptablePasswords, true}});

  base::HistogramTester histogram_tester;
  std::vector<PasswordForm> forms;
  auto form1 =
      AddDummyLogin("foo1", GURL("https://foo1.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  auto form2 =
      AddDummyLogin("foo2", GURL("https://foo2.com/"),
                    /*should_be_corrupted=*/true, /*blocklisted=*/false);
  auto form3 =
      AddDummyLogin("foo3", GURL("https://foo3.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  NiceMock<base::MockCallback<LoginDatabase::OnUndecryptablePasswordsRemoved>>
      on_undecryptable_passwords_removed;

  LoginDatabase db(database_path(), IsAccountStore(false));
  ASSERT_TRUE(db.Init(on_undecryptable_passwords_removed.Get(), nullptr));

  // Make authentication not available.
  OSCryptMocker::SetBackendLocked(true);

  EXPECT_CALL(on_undecryptable_passwords_removed, Run).Times(0);
  EXPECT_FALSE(db.GetAutoSignInLogins(&forms));
  histogram_tester.ExpectTotalCount(
      "PasswordManager.DeleteUndecryptableLoginsReturnValue", 0);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LoginDatabase.ShouldDeleteUndecryptablePasswords", 5, 1);
}
#endif  // BUILDFLAG(IS_MAC)

TEST_F(LoginDatabaseUndecryptableLoginsTest,
       DontDeleteUndecryptableLoginsIfDisabledByPolicy) {
  // Init with feature states allowing for password deletion.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureStates(
      {{features::kSkipUndecryptablePasswords, false},
       {features::kClearUndecryptablePasswords, true}});

  base::HistogramTester histogram_tester;
  std::vector<PasswordForm> forms;
  auto form1 =
      AddDummyLogin("foo1", GURL("https://foo1.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  auto form2 =
      AddDummyLogin("foo2", GURL("https://foo2.com/"),
                    /*should_be_corrupted=*/true, /*blocklisted=*/false);
  auto form3 =
      AddDummyLogin("foo3", GURL("https://foo3.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  LoginDatabase db(database_path(), IsAccountStore(false),
                   LoginDatabase::DeletingUndecryptablePasswordsEnabled(false));
  ASSERT_TRUE(
      db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
              /*encryptor=*/nullptr));

  EXPECT_FALSE(db.GetAutoSignInLogins(&forms));
  histogram_tester.ExpectTotalCount(
      "PasswordManager.DeleteUndecryptableLoginsReturnValue", 0);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LoginDatabase.ShouldDeleteUndecryptablePasswords", 7, 1);
}

TEST_F(LoginDatabaseUndecryptableLoginsTest,
       PasswordRecoveryDisabledGetLogins) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kSkipUndecryptablePasswords);
  AddDummyLogin("foo1", GURL("https://foo1.com/"), false,
                /*blocklisted=*/false);
  AddDummyLogin("foo2", GURL("https://foo2.com/"), true, /*blocklisted=*/false);

  LoginDatabase db(database_path(), IsAccountStore(false));
  ASSERT_TRUE(
      db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
              /*encryptor=*/nullptr));

  std::vector<PasswordForm> result;
  EXPECT_FALSE(db.GetAutofillableLogins(&result));
  EXPECT_TRUE(result.empty());

  RunUntilIdle();
}

#if BUILDFLAG(IS_MAC)
TEST_F(LoginDatabaseUndecryptableLoginsTest, KeychainLockedTest) {
  AddDummyLogin("foo1", GURL("https://foo1.com/"), false,
                /*blocklisted=*/false);
  AddDummyLogin("foo2", GURL("https://foo2.com/"), true, /*blocklisted=*/false);

  OSCryptMocker::SetBackendLocked(true);
  LoginDatabase db(database_path(), IsAccountStore(false));
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(
      db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
              /*encryptor=*/nullptr));
  EXPECT_EQ(DatabaseCleanupResult::kEncryptionUnavailable,
            db.DeleteUndecryptableLogins());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.DeleteUndecryptableLoginsReturnValue",
      metrics_util::DeleteCorruptedPasswordsResult::kEncryptionUnavailable, 1);
}
#endif  // BUILDFLAG(IS_MAC)

// Tests getting various types of undecryptable credentials.
// First test parameter is responsible for toggling kSkipUndecryptablePasswords
// feature.
// Second test parameter is responsible for toggling
// kClearUndecryptablePasswords feature.
class LoginDatabaseGetUndecryptableLoginsTest
    : public LoginDatabaseUndecryptableLoginsTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  LoginDatabaseGetUndecryptableLoginsTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kSkipUndecryptablePasswords, std::get<0>(GetParam())},
         {features::kClearUndecryptablePasswords, std::get<1>(GetParam())}});
  }
  ~LoginDatabaseGetUndecryptableLoginsTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test getting auto sign in logins when there are undecryptable ones
TEST_P(LoginDatabaseGetUndecryptableLoginsTest, GetAutoSignInLogins) {
  base::HistogramTester histogram_tester;
  std::vector<PasswordForm> forms;
  auto form1 =
      AddDummyLogin("foo1", GURL("https://foo1.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  auto form2 =
      AddDummyLogin("foo2", GURL("https://foo2.com/"),
                    /*should_be_corrupted=*/true, /*blocklisted=*/false);
  auto form3 =
      AddDummyLogin("foo3", GURL("https://foo3.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  NiceMock<base::MockCallback<LoginDatabase::OnUndecryptablePasswordsRemoved>>
      on_undecryptable_passwords_removed;

  LoginDatabase db(database_path(), IsAccountStore(false));
  ASSERT_TRUE(db.Init(on_undecryptable_passwords_removed.Get(), nullptr));

  if (base::FeatureList::IsEnabled(features::kClearUndecryptablePasswords)) {
    EXPECT_CALL(on_undecryptable_passwords_removed, Run(IsAccountStore(false)));
    EXPECT_TRUE(db.GetAutoSignInLogins(&forms));
    EXPECT_THAT(forms, UnorderedElementsAre(HasPrimaryKeyAndEquals(form1),
                                            HasPrimaryKeyAndEquals(form3)));
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.DeleteUndecryptableLoginsReturnValue",
        metrics_util::DeleteCorruptedPasswordsResult::kSuccessPasswordsDeleted,
        1);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.LoginDatabase.ShouldDeleteUndecryptablePasswords", 0,
        1);
  } else {
    if (base::FeatureList::IsEnabled(features::kSkipUndecryptablePasswords)) {
      EXPECT_CALL(on_undecryptable_passwords_removed,
                  Run(IsAccountStore(false)));
      EXPECT_TRUE(db.GetAutoSignInLogins(&forms));
      EXPECT_THAT(forms, UnorderedElementsAre(HasPrimaryKeyAndEquals(form1),
                                              HasPrimaryKeyAndEquals(form3)));
      histogram_tester.ExpectTotalCount(
          "PasswordManager.DeleteUndecryptableLoginsReturnValue", 0);
    } else {
      EXPECT_CALL(on_undecryptable_passwords_removed,
                  Run(IsAccountStore(false)));
      EXPECT_FALSE(db.GetAutoSignInLogins(&forms));
      histogram_tester.ExpectTotalCount(
          "PasswordManager.DeleteUndecryptableLoginsReturnValue", 0);
    }
  }
}

// Test getting logins when there are undecryptable ones
TEST_P(LoginDatabaseGetUndecryptableLoginsTest, GetLogins) {
  base::HistogramTester histogram_tester;
  auto form1 =
      AddDummyLogin("user1", GURL("http://www.google.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  auto form2 =
      AddDummyLogin("user2", GURL("http://www.google.com/"),
                    /*should_be_corrupted=*/true, /*blocklisted=*/false);
  NiceMock<base::MockCallback<LoginDatabase::OnUndecryptablePasswordsRemoved>>
      on_undecryptable_passwords_removed;

  LoginDatabase db(database_path(), IsAccountStore(false));
  ASSERT_TRUE(db.Init(on_undecryptable_passwords_removed.Get(), nullptr));
  std::vector<PasswordForm> result;
  PasswordForm form = GenerateExamplePasswordForm();

  if (base::FeatureList::IsEnabled(features::kClearUndecryptablePasswords)) {
    EXPECT_CALL(on_undecryptable_passwords_removed, Run);
    EXPECT_TRUE(db.GetLogins(PasswordFormDigest(form),
                             /*should_PSL_matching_apply=*/false, &result));
    EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form1)));
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.DeleteUndecryptableLoginsReturnValue",
        metrics_util::DeleteCorruptedPasswordsResult::kSuccessPasswordsDeleted,
        1);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.LoginDatabase.ShouldDeleteUndecryptablePasswords", 0,
        1);
  } else {
    if (base::FeatureList::IsEnabled(features::kSkipUndecryptablePasswords)) {
      EXPECT_CALL(on_undecryptable_passwords_removed, Run);
      EXPECT_TRUE(db.GetLogins(PasswordFormDigest(form),
                               /*should_PSL_matching_apply=*/false, &result));
      EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form1)));
      histogram_tester.ExpectTotalCount(
          "PasswordManager.DeleteUndecryptableLoginsReturnValue", 0);
    } else {
      EXPECT_CALL(on_undecryptable_passwords_removed, Run);
      EXPECT_FALSE(db.GetLogins(PasswordFormDigest(form),
                                /*should_PSL_matching_apply=*/false, &result));
      histogram_tester.ExpectTotalCount(
          "PasswordManager.DeleteUndecryptableLoginsReturnValue", 0);
    }
  }
}

// Test getting auto fillable logins when there are undecryptable ones
TEST_P(LoginDatabaseGetUndecryptableLoginsTest, GetAutofillableLogins) {
  base::HistogramTester histogram_tester;
  std::vector<PasswordForm> result;

  auto form1 =
      AddDummyLogin("foo1", GURL("https://foo1.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  auto form2 =
      AddDummyLogin("foo2", GURL("https://foo2.com/"),
                    /*should_be_corrupted=*/true, /*blocklisted=*/false);
  auto form3 =
      AddDummyLogin("foo3", GURL("https://foo3.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/true);
  NiceMock<base::MockCallback<LoginDatabase::OnUndecryptablePasswordsRemoved>>
      on_undecryptable_passwords_removed;

  LoginDatabase db(database_path(), IsAccountStore(false));
  ASSERT_TRUE(db.Init(on_undecryptable_passwords_removed.Get(), nullptr));

  if (base::FeatureList::IsEnabled(features::kClearUndecryptablePasswords)) {
    EXPECT_CALL(on_undecryptable_passwords_removed, Run);
    EXPECT_TRUE(db.GetAutofillableLogins(&result));
    EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form1)));
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.DeleteUndecryptableLoginsReturnValue",
        metrics_util::DeleteCorruptedPasswordsResult::kSuccessPasswordsDeleted,
        1);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.LoginDatabase.ShouldDeleteUndecryptablePasswords", 0,
        1);
  } else {
    if (base::FeatureList::IsEnabled(features::kSkipUndecryptablePasswords)) {
      EXPECT_CALL(on_undecryptable_passwords_removed, Run);
      EXPECT_TRUE(db.GetAutofillableLogins(&result));
      EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form1)));
      histogram_tester.ExpectTotalCount(
          "PasswordManager.DeleteUndecryptableLoginsReturnValue", 0);
    } else {
      EXPECT_CALL(on_undecryptable_passwords_removed, Run);
      EXPECT_FALSE(db.GetAutofillableLogins(&result));
      histogram_tester.ExpectTotalCount(
          "PasswordManager.DeleteUndecryptableLoginsReturnValue", 0);
    }
  }
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)
// Regression test for b/354847250.
// Checks that if kSkipUndecryptablePasswords is enabled, getting login succeeds
// even if there are undecryptable passwords present.
TEST_P(LoginDatabaseGetUndecryptableLoginsTest,
       GettingLoginForFormIfUndecryptablePasswordsArePresent) {
  base::HistogramTester histogram_tester;
  std::vector<PasswordForm> result;
  auto form1 =
      AddDummyLogin("user1", GURL("http://www.google.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  auto form2 =
      AddDummyLogin("user2", GURL("http://www.google.com/"),
                    /*should_be_corrupted=*/true, /*blocklisted=*/false);
  LoginDatabase db(database_path(), IsAccountStore(false));
  ASSERT_TRUE(
      db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
              /*encryptor=*/nullptr));
  PasswordForm form = GenerateExamplePasswordForm();

  // Set the user data directory switch, it will prevent passwords from being
  // deleted.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      password_manager::kUserDataDir);

  if (!base::FeatureList::IsEnabled(features::kSkipUndecryptablePasswords)) {
    EXPECT_FALSE(db.GetLogins(PasswordFormDigest(form),
                              /*should_PSL_matching_apply=*/false, &result));

    histogram_tester.ExpectTotalCount(
        "PasswordManager.DeleteUndecryptableLoginsReturnValue", 0);
    return;
  }

  EXPECT_TRUE(db.GetLogins(PasswordFormDigest(form),
                           /*should_PSL_matching_apply=*/false, &result));
  EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form1)));

  histogram_tester.ExpectTotalCount(
      "PasswordManager.DeleteUndecryptableLoginsReturnValue", 0);
}

// Regression test for b/354847250.
// Checks that if kSkipUndecryptablePasswords is enabled, getting all logins
// succeeds even if there are undecryptable passwords present.
TEST_P(LoginDatabaseGetUndecryptableLoginsTest,
       GettingAllLoginsIfUndecryptablePasswordsArePresent) {
  base::HistogramTester histogram_tester;
  std::vector<PasswordForm> result;

  auto form1 =
      AddDummyLogin("foo1", GURL("https://foo1.com/"),
                    /*should_be_corrupted=*/false, /*blocklisted=*/false);
  auto form2 =
      AddDummyLogin("foo2", GURL("https://foo2.com/"),
                    /*should_be_corrupted=*/true, /*blocklisted=*/false);

  LoginDatabase db(database_path(), IsAccountStore(false));
  ASSERT_TRUE(
      db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
              /*encryptor=*/nullptr));

  // Set the user data directory switch, it will prevent passwords from being
  // deleted.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      password_manager::kUserDataDir);

  if (!base::FeatureList::IsEnabled(features::kSkipUndecryptablePasswords)) {
    EXPECT_FALSE(db.GetAutofillableLogins(&result));

    histogram_tester.ExpectTotalCount(
        "PasswordManager.DeleteUndecryptableLoginsReturnValue", 0);
    return;
  }

  EXPECT_TRUE(db.GetAutofillableLogins(&result));
  EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form1)));

  histogram_tester.ExpectTotalCount(
      "PasswordManager.DeleteUndecryptableLoginsReturnValue", 0);
}
#endif

INSTANTIATE_TEST_SUITE_P(All,
                         LoginDatabaseGetUndecryptableLoginsTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

#endif  // #if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_IOS) ||
        // BUILDFLAG(IS_WIN)

// Test encrypted passwords are present in add change lists.
TEST_P(LoginDatabaseTest, EncryptedPasswordAdd) {
  PasswordForm form;
  form.url = GURL("http://0.com");
  form.signon_realm = "http://www.example.com/";
  form.action = GURL("http://www.example.com/action");
  form.password_element = u"pwd";
  form.password_value = u"example";
  password_manager::PasswordStoreChangeList changes = db().AddLogin(form);
  ASSERT_EQ(1u, changes.size());
#if BUILDFLAG(IS_IOS)
  ASSERT_FALSE(changes[0].form().keychain_identifier.empty());
#else
  ASSERT_TRUE(changes[0].form().keychain_identifier.empty());
#endif
}

// Test encrypted passwords are present in add change lists, when the password
// is already in the DB.
TEST_P(LoginDatabaseTest, EncryptedPasswordAddWithReplaceSemantics) {
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
#if BUILDFLAG(IS_IOS)
  ASSERT_FALSE(changes[1].form().keychain_identifier.empty());
#else
  ASSERT_TRUE(changes[1].form().keychain_identifier.empty());
#endif
}

// Test encrypted passwords are present in update change lists.
TEST_P(LoginDatabaseTest, EncryptedPasswordUpdate) {
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
#if BUILDFLAG(IS_IOS)
  ASSERT_FALSE(changes[0].form().keychain_identifier.empty());
#else
  ASSERT_TRUE(changes[0].form().keychain_identifier.empty());
#endif
}

// Test encrypted passwords are present when retrieving from DB.
TEST_P(LoginDatabaseTest, GetLoginsEncryptedPassword) {
  PasswordForm form;
  form.url = GURL("http://0.com");
  form.signon_realm = "http://www.example.com/";
  form.action = GURL("http://www.example.com/action");
  form.password_element = u"pwd";
  form.password_value = u"example";
  password_manager::PasswordStoreChangeList changes = db().AddLogin(form);
  ASSERT_EQ(1u, changes.size());
#if BUILDFLAG(IS_IOS)
  ASSERT_FALSE(changes[0].form().keychain_identifier.empty());
#else
  ASSERT_TRUE(changes[0].form().keychain_identifier.empty());
#endif

  std::vector<PasswordForm> forms;
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form),
                             /*should_PSL_matching_apply=*/false, &forms));

  ASSERT_EQ(1U, forms.size());
#if BUILDFLAG(IS_IOS)
  ASSERT_FALSE(forms[0].keychain_identifier.empty());
#else
  ASSERT_TRUE(forms[0].keychain_identifier.empty());
#endif
}

TEST_P(LoginDatabaseTest, RetrievesInsecureDataWithLogins) {
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

  std::vector<PasswordForm> result;
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form),
                             /*should_PSL_matching_apply=*/true, &result));
  EXPECT_THAT(result, ElementsAre(HasPrimaryKeyAndEquals(form)));
}

TEST_P(LoginDatabaseTest, RetrievesNoteWithLogin) {
  PasswordForm form = GenerateExamplePasswordForm();
  std::ignore = db().AddLogin(form);
  PasswordNote note(u"example note", base::Time::Now());
  db().password_notes_table().InsertOrReplace(FormPrimaryKey(1), note);

  std::vector<PasswordForm> results;
  EXPECT_TRUE(db().GetLogins(PasswordFormDigest(form),
                             /* should_PSL_matching_apply */ true, &results));

  PasswordForm expected_form = form;
  expected_form.notes = {note};
  EXPECT_THAT(results, ElementsAre(HasPrimaryKeyAndEquals(expected_form)));
}

TEST_P(LoginDatabaseTest, AddLoginWithNotePersistsThem) {
  PasswordForm form = GenerateExamplePasswordForm();
  PasswordNote note(u"example note", base::Time::Now());
  form.notes = {note};

  std::ignore = db().AddLogin(form);

  EXPECT_EQ(db().password_notes_table().GetPasswordNotes(FormPrimaryKey(1))[0],
            note);
}

TEST_P(LoginDatabaseTest, RemoveLoginRemovesNoteAttachedToTheLogin) {
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

TEST_P(LoginDatabaseTest, RemovingLoginRemovesInsecureCredentials) {
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
TEST_P(LoginDatabaseTest, GetLoginsBySignonRealmAndUsername) {
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

  std::vector<PasswordForm> forms;
  // Check if there is exactly one form with this signon_realm & username1.
  EXPECT_EQ(
      FormRetrievalResult::kSuccess,
      db().GetLoginsBySignonRealmAndUsername(signon_realm, username1, &forms));
  EXPECT_THAT(forms, ElementsAre(HasPrimaryKeyAndEquals(form1)));

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
  EXPECT_THAT(forms, ElementsAre(HasPrimaryKeyAndEquals(form1),
                                 HasPrimaryKeyAndEquals(form3)));
}

TEST_P(LoginDatabaseTest, UpdateLoginWithAddedInsecureCredential) {
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

TEST_P(LoginDatabaseTest, UpdateLoginWithUpdatedInsecureCredential) {
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

TEST_P(LoginDatabaseTest, UpdateLoginWithRemovedInsecureCredentialEntry) {
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

TEST_P(LoginDatabaseTest,
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

TEST_P(LoginDatabaseTest, AddLoginWithInsecureCredentialsPersistsThem) {
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

TEST_P(LoginDatabaseTest, RemoveLoginRemovesInsecureCredentials) {
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

TEST_P(LoginDatabaseTest, AddLoginWithNonEmptyInvalidURL) {
  PasswordForm form;
  form.signon_realm = "invalid";
  form.url = GURL(form.signon_realm);
  form.username_value = u"username";
  form.password_value = u"password";
  auto error = AddCredentialError::kNone;
  EXPECT_THAT(db().AddLogin(form, &error), IsEmpty());
  EXPECT_EQ(error, AddCredentialError::kConstraintViolation);
}

TEST_P(LoginDatabaseTest, IsEmptyCb_InitEmpty) {
  LoginDatabase db(temp_dir_.GetPath().AppendASCII("DbDirectory"),
                   IsAccountStore(false));
  NiceMock<base::MockCallback<LoginDatabase::IsEmptyCallback>> is_empty_cb;
  db.SetIsEmptyCb(is_empty_cb.Get());
  EXPECT_CALL(is_empty_cb, Run(LoginDatabase::LoginDatabaseEmptinessState{
                               .no_login_found = true,
                               .autofillable_credentials_exist = false}));
  db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
          /*encryptor=*/encryptor());
}

TEST_P(LoginDatabaseTest, IsEmptyCb_InitNonEmpty) {
  base::FilePath directory = temp_dir_.GetPath().AppendASCII("DbDirectory");
  {
    // Simulate the DB being populated in a previous startup.
    auto db = std::make_unique<LoginDatabase>(directory, IsAccountStore(false));
    db->Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
             /*encryptor=*/encryptor());
    std::ignore =
        db->AddLogin(GenerateExamplePasswordForm(), /*error=*/nullptr);
    db.reset();
  }

  LoginDatabase db(directory, IsAccountStore(false));
  NiceMock<base::MockCallback<LoginDatabase::IsEmptyCallback>> is_empty_cb;
  db.SetIsEmptyCb(is_empty_cb.Get());
  EXPECT_CALL(is_empty_cb, Run(LoginDatabase::LoginDatabaseEmptinessState{
                               .no_login_found = false,
                               .autofillable_credentials_exist = true}));
  db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
          /*encryptor=*/encryptor());
}

TEST_P(LoginDatabaseTest, IsEmptyCb_AddLogin) {
  ASSERT_TRUE(db().IsEmpty().no_login_found &&
              !db().IsEmpty().autofillable_credentials_exist);
  EXPECT_CALL(is_empty_cb_, Run(LoginDatabase::LoginDatabaseEmptinessState{
                                .no_login_found = false,
                                .autofillable_credentials_exist = true}));
  std::ignore = db().AddLogin(GenerateExamplePasswordForm(), /*error=*/nullptr);
}

TEST_P(LoginDatabaseTest,
       IsEmptyCb_AddBlocklist_NoAutofillableCredentialsExist) {
  ASSERT_TRUE(db().IsEmpty().no_login_found &&
              !db().IsEmpty().autofillable_credentials_exist);
  PasswordForm blocklist = GenerateBlocklistedForm();
  EXPECT_CALL(is_empty_cb_, Run(LoginDatabase::LoginDatabaseEmptinessState{
                                .no_login_found = false,
                                .autofillable_credentials_exist = false}));
  std::ignore = db().AddLogin(blocklist, /*error=*/nullptr);
}

TEST_P(LoginDatabaseTest,
       IsEmptyCb_AddFederatedCredential_NoAutofillableCredentialsExist) {
  ASSERT_TRUE(db().IsEmpty().no_login_found &&
              !db().IsEmpty().autofillable_credentials_exist);
  PasswordForm federated_credential = GenerateFederatedCredentialForm();
  EXPECT_CALL(is_empty_cb_, Run(LoginDatabase::LoginDatabaseEmptinessState{
                                .no_login_found = false,
                                .autofillable_credentials_exist = false}));
  std::ignore = db().AddLogin(federated_credential, /*error=*/nullptr);
}

TEST_P(LoginDatabaseTest,
       IsEmptyCb_AddUsernameOnlyCredential_NoAutofillableCredentialsExist) {
  ASSERT_TRUE(db().IsEmpty().no_login_found &&
              !db().IsEmpty().autofillable_credentials_exist);
  PasswordForm username_only = GenerateUsernameOnlyForm();
  EXPECT_CALL(is_empty_cb_, Run(LoginDatabase::LoginDatabaseEmptinessState{
                                .no_login_found = false,
                                .autofillable_credentials_exist = false}));
  std::ignore = db().AddLogin(username_only, /*error=*/nullptr);
}

TEST_P(LoginDatabaseTest, IsEmptyCb_RemoveLogin) {
  PasswordForm normal_form = GenerateExamplePasswordForm();
  PasswordForm blocklist_form = GenerateBlocklistedForm();
  PasswordForm federated_form = GenerateFederatedCredentialForm();
  PasswordForm username_only_form = GenerateUsernameOnlyForm();

  ASSERT_EQ(db().AddLogin(normal_form, /*error=*/nullptr).size(), 1u);
  ASSERT_EQ(db().AddLogin(blocklist_form, /*error=*/nullptr).size(), 1u);
  ASSERT_EQ(db().AddLogin(federated_form, /*error=*/nullptr).size(), 1u);
  ASSERT_EQ(db().AddLogin(username_only_form, /*error=*/nullptr).size(), 1u);
  ASSERT_TRUE(!db().IsEmpty().no_login_found &&
              db().IsEmpty().autofillable_credentials_exist);

  testing::MockFunction<void(int)> check;
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(is_empty_cb_, Run(LoginDatabase::LoginDatabaseEmptinessState{
                                  .no_login_found = false,
                                  .autofillable_credentials_exist = false}))
        .Times(3);
    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(is_empty_cb_, Run(LoginDatabase::LoginDatabaseEmptinessState{
                                  .no_login_found = true,
                                  .autofillable_credentials_exist = false}));
  }
  std::ignore = db().RemoveLogin(normal_form, /*changes=*/nullptr);
  std::ignore = db().RemoveLogin(blocklist_form, /*changes=*/nullptr);
  std::ignore = db().RemoveLogin(federated_form, /*changes=*/nullptr);
  check.Call(1);
  std::ignore = db().RemoveLogin(username_only_form, /*changes=*/nullptr);
}

TEST_P(LoginDatabaseTest, IsEmptyCb_RemoveLoginByPrimaryKey) {
  PasswordForm normal_form = GenerateExamplePasswordForm();
  PasswordForm blocklist_form = GenerateBlocklistedForm();
  PasswordForm federated_form = GenerateFederatedCredentialForm();
  PasswordForm username_only_form = GenerateUsernameOnlyForm();

  PasswordStoreChangeList normal_form_changes = db().AddLogin(normal_form);
  PasswordStoreChangeList blocklist_form_changes =
      db().AddLogin(blocklist_form);
  PasswordStoreChangeList federated_form_changes =
      db().AddLogin(federated_form);
  PasswordStoreChangeList username_only_form_changes =
      db().AddLogin(username_only_form);

  ASSERT_EQ(normal_form_changes.size(), 1u);
  ASSERT_EQ(blocklist_form_changes.size(), 1u);
  ASSERT_EQ(federated_form_changes.size(), 1u);
  ASSERT_EQ(username_only_form_changes.size(), 1u);
  ASSERT_TRUE(!db().IsEmpty().no_login_found &&
              db().IsEmpty().autofillable_credentials_exist);

  testing::MockFunction<void(int)> check;
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(is_empty_cb_, Run(LoginDatabase::LoginDatabaseEmptinessState{
                                  .no_login_found = false,
                                  .autofillable_credentials_exist = false}))
        .Times(3);
    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(is_empty_cb_, Run(LoginDatabase::LoginDatabaseEmptinessState{
                                  .no_login_found = true,
                                  .autofillable_credentials_exist = false}));
  }

  std::ignore = db().RemoveLoginByPrimaryKey(
      *normal_form_changes[0].form().primary_key, &normal_form_changes);
  std::ignore = db().RemoveLoginByPrimaryKey(
      *blocklist_form_changes[0].form().primary_key, &blocklist_form_changes);
  std::ignore = db().RemoveLoginByPrimaryKey(
      *federated_form_changes[0].form().primary_key, &federated_form_changes);
  check.Call(1);
  std::ignore = db().RemoveLoginByPrimaryKey(
      *username_only_form_changes[0].form().primary_key,
      &username_only_form_changes);
}

TEST_P(LoginDatabaseTest, IsEmptyCb_RemoveLoginsCreatedBetween) {
  std::ignore = db().AddLogin(GenerateExamplePasswordForm(), /*error=*/nullptr);
  ASSERT_TRUE(!db().IsEmpty().no_login_found &&
              db().IsEmpty().autofillable_credentials_exist);
  EXPECT_CALL(is_empty_cb_, Run(LoginDatabase::LoginDatabaseEmptinessState{
                                .no_login_found = true,
                                .autofillable_credentials_exist = false}));
  std::ignore = db().RemoveLoginsCreatedBetween(base::Time(), base::Time::Now(),
                                                /*changes=*/nullptr);
}

TEST_P(LoginDatabaseTest, IsEmptyCb_DeleteAndRecreateDatabaseFile) {
  std::ignore = db().AddLogin(GenerateExamplePasswordForm(), /*error=*/nullptr);
  ASSERT_TRUE(!db().IsEmpty().no_login_found &&
              db().IsEmpty().autofillable_credentials_exist);
  EXPECT_CALL(is_empty_cb_, Run(LoginDatabase::LoginDatabaseEmptinessState{
                                .no_login_found = true,
                                .autofillable_credentials_exist = false}));
  db().DeleteAndRecreateDatabaseFile();
}

class LoginDatabaseForAccountStoreTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestMetadataStoreMacDatabase");
    OSCryptMocker::SetUp();

    db_ = std::make_unique<LoginDatabase>(file_, IsAccountStore(true));
    ASSERT_TRUE(
        db_->Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
                  /*encryptor=*/nullptr));
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
