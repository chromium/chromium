// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/origin_credential_store.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {
namespace {

using base::ASCIIToUTF16;
using testing::ElementsAre;
using testing::Property;

using BlocklistedStatus = OriginCredentialStore::BlocklistedStatus;

constexpr char kExampleSite[] = "https://example.com/";

UiCredential MakeUiCredential(
    std::string_view username,
    std::string_view password,
    std::string_view origin = kExampleSite,
    password_manager_util::GetLoginMatchType match_type =
        password_manager_util::GetLoginMatchType::kExact) {
  return UiCredential(base::UTF8ToUTF16(username), base::UTF8ToUTF16(password),
                      url::Origin::Create(GURL(origin)), match_type,
                      base::Time());
}

password_manager::PasswordForm CreateTestPasswordForm(int index = 0) {
  password_manager::PasswordForm form;
  form.url = GURL("https://test" + base::NumberToString(index) + ".com");
  form.signon_realm = form.url.spec();
  form.username_value = u"username" + base::NumberToString16(index);
  form.password_value = u"password" + base::NumberToString16(index);
  return form;
}

}  // namespace

class OriginCredentialStoreTest : public testing::Test {
 public:
  OriginCredentialStore* store() { return &store_; }

 private:
  OriginCredentialStore store_{url::Origin::Create(GURL(kExampleSite))};
};

TEST_F(OriginCredentialStoreTest, StoresCredentials) {
  store()->SaveCredentials(
      {MakeUiCredential("Berta", "30948"), MakeUiCredential("Adam", "Pas83B"),
       MakeUiCredential("Dora", "PakudC"), MakeUiCredential("Carl", "P1238C")});

  EXPECT_THAT(store()->GetCredentials(),
              ElementsAre(MakeUiCredential("Berta", "30948"),
                          MakeUiCredential("Adam", "Pas83B"),
                          MakeUiCredential("Dora", "PakudC"),
                          MakeUiCredential("Carl", "P1238C")));
}

TEST_F(OriginCredentialStoreTest, StoresUnnotifiedSharedCredentials) {
  store()->SaveUnnotifiedSharedCredentials(
      {CreateTestPasswordForm(1), CreateTestPasswordForm(2)});

  EXPECT_THAT(
      store()->GetUnnotifiedSharedCredentials(),
      ElementsAre(CreateTestPasswordForm(1), CreateTestPasswordForm(2)));
}

TEST_F(OriginCredentialStoreTest, StoresOnlyNormalizedOrigins) {
  store()->SaveCredentials(
      {MakeUiCredential("Berta", "30948", kExampleSite),
       MakeUiCredential("Adam", "Pas83B", std::string(kExampleSite) + "path"),
       MakeUiCredential(
           "Dora", "PakudC", kExampleSite,
           password_manager_util::GetLoginMatchType::kAffiliated)});

  EXPECT_THAT(store()->GetCredentials(),
              ElementsAre(

                  // The URL that equals an origin stays untouched.
                  MakeUiCredential("Berta", "30948", kExampleSite),

                  // The longer URL is reduced to an origin.
                  MakeUiCredential("Adam", "Pas83B", kExampleSite),

                  // The android credential stays untouched.
                  MakeUiCredential(
                      "Dora", "PakudC", kExampleSite,
                      password_manager_util::GetLoginMatchType::kAffiliated)));
}

TEST_F(OriginCredentialStoreTest, ReplacesCredentials) {
  store()->SaveCredentials({MakeUiCredential("Ben", "S3cur3")});
  ASSERT_THAT(store()->GetCredentials(),
              ElementsAre(MakeUiCredential("Ben", "S3cur3")));

  store()->SaveCredentials({MakeUiCredential(std::string(), "M1@u")});
  EXPECT_THAT(store()->GetCredentials(),
              ElementsAre(MakeUiCredential(std::string(), "M1@u")));
}

TEST_F(OriginCredentialStoreTest, ClearsCredentials) {
  store()->SaveCredentials({MakeUiCredential("Ben", "S3cur3")});
  ASSERT_THAT(store()->GetCredentials(),
              ElementsAre(MakeUiCredential("Ben", "S3cur3")));

  store()->ClearCredentials();
  EXPECT_EQ(store()->GetCredentials().size(), 0u);
}

TEST_F(OriginCredentialStoreTest, SetBlocklistedAfterNeverBlocklisted) {
  store()->SetBlocklistedStatus(true);
  EXPECT_EQ(BlocklistedStatus::kIsBlocklisted, store()->GetBlocklistedStatus());
}

TEST_F(OriginCredentialStoreTest, CorrectlyUpdatesBlocklistedStatus) {
  store()->SetBlocklistedStatus(true);
  ASSERT_EQ(BlocklistedStatus::kIsBlocklisted, store()->GetBlocklistedStatus());

  store()->SetBlocklistedStatus(false);
  EXPECT_EQ(BlocklistedStatus::kWasBlocklisted,
            store()->GetBlocklistedStatus());

  store()->SetBlocklistedStatus(true);
  EXPECT_EQ(BlocklistedStatus::kIsBlocklisted, store()->GetBlocklistedStatus());
}

TEST_F(OriginCredentialStoreTest, WasBlocklistedStaysTheSame) {
  store()->SetBlocklistedStatus(true);
  ASSERT_EQ(BlocklistedStatus::kIsBlocklisted, store()->GetBlocklistedStatus());

  store()->SetBlocklistedStatus(false);
  ASSERT_EQ(BlocklistedStatus::kWasBlocklisted,
            store()->GetBlocklistedStatus());

  // If unblocklisting is communicated twice in a row, the status shouldn't
  // change.
  store()->SetBlocklistedStatus(false);
  EXPECT_EQ(BlocklistedStatus::kWasBlocklisted,
            store()->GetBlocklistedStatus());
}

TEST_F(OriginCredentialStoreTest, NeverBlocklistedStaysTheSame) {
  ASSERT_EQ(BlocklistedStatus::kNeverBlocklisted,
            store()->GetBlocklistedStatus());

  store()->SetBlocklistedStatus(false);
  EXPECT_EQ(BlocklistedStatus::kNeverBlocklisted,
            store()->GetBlocklistedStatus());
}

TEST_F(OriginCredentialStoreTest, SaveSharedPasswords) {
  password_manager::PasswordForm shared_password;
  shared_password.username_value = u"username";
  shared_password.password_value = u"password";
  shared_password.signon_realm = kExampleSite;
  shared_password.match_type =
      password_manager::PasswordForm::MatchType::kExact;
  shared_password.type =
      password_manager::PasswordForm::Type::kReceivedViaSharing;

  store()->SaveCredentials(
      {UiCredential(shared_password, url::Origin::Create(GURL(kExampleSite)))});

  EXPECT_THAT(store()->GetCredentials(),
              ElementsAre(Property(&UiCredential::is_shared, true)));
}

}  // namespace password_manager
