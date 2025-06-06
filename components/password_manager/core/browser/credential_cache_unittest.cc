// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_cache.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace password_manager {

namespace {

using testing::IsEmpty;
using testing::Property;
using url::Origin;
using IsOriginBlocklisted = CredentialCache::IsOriginBlocklisted;
using IsBackupCredential = UiCredential::IsBackupCredential;

constexpr char kExampleSite[] = "https://example.com/";
constexpr char kExampleSite2[] = "https://example.two.com/";
constexpr char kExampleSiteMobile[] = "https://m.example.com/";
constexpr char kExampleSiteSubdomain[] = "https://accounts.example.com/";

UiCredential MakeUiCredential(
    std::string_view username,
    std::string_view password,
    std::string_view origin = kExampleSite,
    std::string_view display_name = kExampleSite,
    password_manager_util::GetLoginMatchType match_type =
        password_manager_util::GetLoginMatchType::kExact,
    IsBackupCredential is_backup_credential = IsBackupCredential(false)) {
  return UiCredential(base::UTF8ToUTF16(username), base::UTF8ToUTF16(password),
                      Origin::Create(GURL(origin)), std::string(display_name),
                      match_type, base::Time(), is_backup_credential);
}

#if BUILDFLAG(IS_ANDROID)
PasswordForm CreateEntryWithBackupPassword(
    const std::string& username,
    const std::string& password,
    const std::u16string& backup_password,
    const GURL& origin_url,
    PasswordForm::MatchType match_type) {
  PasswordForm form = CreateEntry(username, password, origin_url, match_type);
  form.SetPasswordBackupNote(backup_password);
  return form;
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

class CredentialCacheTest : public testing::Test {
 public:
  CredentialCache* cache() { return &cache_; }

 private:
  CredentialCache cache_;
};

TEST_F(CredentialCacheTest, ReturnsSameStoreForSameOriginOnly) {
  EXPECT_EQ(&cache()->GetCredentialStore(Origin::Create(GURL(kExampleSite))),
            &cache()->GetCredentialStore(Origin::Create(GURL(kExampleSite))));

  EXPECT_NE(&cache()->GetCredentialStore(Origin::Create(GURL(kExampleSite))),
            &cache()->GetCredentialStore(Origin::Create(GURL(kExampleSite2))));
}

TEST_F(CredentialCacheTest, StoresCredentialsSortedByAplhabetAndOrigins) {
  Origin origin = Origin::Create(GURL(kExampleSite));
  std::vector<PasswordForm> matches = {
      CreateEntry("Berta", "30948", GURL(kExampleSite),
                  PasswordForm::MatchType::kExact),
      CreateEntry("Adam", "Pas83B", GURL(kExampleSite),
                  PasswordForm::MatchType::kExact),
      CreateEntry("Dora", "PakudC", GURL(kExampleSite),
                  PasswordForm::MatchType::kExact),
      CreateEntry("Carl", "P1238C", GURL(kExampleSite),
                  PasswordForm::MatchType::kExact),
      // These entries need to be ordered but come after the examples above.
      CreateEntry("Cesar", "V3V1V", GURL(kExampleSite),
                  PasswordForm::MatchType::kAffiliated),
      CreateEntry("Rolf", "A4nd0m", GURL(kExampleSiteMobile),
                  PasswordForm::MatchType::kPSL),
      CreateEntry("Greg", "5fnd1m", GURL(kExampleSiteSubdomain),
                  PasswordForm::MatchType::kPSL),
      CreateEntry("Elfi", "a65ddm", GURL(kExampleSiteSubdomain),
                  PasswordForm::MatchType::kPSL),
      CreateEntry("Alf", "R4nd50m", GURL(kExampleSiteMobile),
                  PasswordForm::MatchType::kPSL)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, IsOriginBlocklisted(false), std::nullopt, origin);

  EXPECT_THAT(
      cache()->GetCredentialStore(origin).GetCredentials(),
      testing::ElementsAre(

          // Alphabetical entries of the exactly matching https://example.com:
          MakeUiCredential("Adam", "Pas83B"),
          MakeUiCredential("Berta", "30948"),
          MakeUiCredential("Carl", "P1238C"),
          // Affiliation based matches are first class citizens and should be
          // treated as a first-party credential.
          MakeUiCredential(
              "Cesar", "V3V1V", kExampleSite, kExampleSite,
              password_manager_util::GetLoginMatchType::kAffiliated),
          MakeUiCredential("Dora", "PakudC"),

          // Alphabetical entries of PSL-match https://accounts.example.com:
          MakeUiCredential("Elfi", "a65ddm", kExampleSiteSubdomain,
                           kExampleSiteSubdomain,
                           password_manager_util::GetLoginMatchType::kPSL),
          MakeUiCredential("Greg", "5fnd1m", kExampleSiteSubdomain,
                           kExampleSiteSubdomain,
                           password_manager_util::GetLoginMatchType::kPSL),

          // Alphabetical entries of PSL-match https://m.example.com:
          MakeUiCredential("Alf", "R4nd50m", kExampleSiteMobile,
                           kExampleSiteMobile,
                           password_manager_util::GetLoginMatchType::kPSL),
          MakeUiCredential("Rolf", "A4nd0m", kExampleSiteMobile,
                           kExampleSiteMobile,
                           password_manager_util::GetLoginMatchType::kPSL)));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(CredentialCacheTest,
       StoresCredentialsSortedByOriginsAndUsernamesWithBackup) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(features::kFillRecoveryPassword);
  Origin origin = Origin::Create(GURL(kExampleSite));
  std::vector<PasswordForm> matches = {
      CreateEntryWithBackupPassword("Berta", "30948", u"backuppassword",
                                    GURL(kExampleSite),
                                    PasswordForm::MatchType::kExact),
      CreateEntry("Adam", "Pas83B", GURL(kExampleSite),
                  PasswordForm::MatchType::kExact),
      CreateEntry("Dora", "PakudC", GURL(kExampleSite),
                  PasswordForm::MatchType::kExact),
      CreateEntry("Carl", "P1238C", GURL(kExampleSite),
                  PasswordForm::MatchType::kExact),
      // These entries need to be ordered but come after the examples above.
      CreateEntry("Cesar", "V3V1V", GURL(kExampleSite),
                  PasswordForm::MatchType::kAffiliated),
      CreateEntry("Rolf", "A4nd0m", GURL(kExampleSiteMobile),
                  PasswordForm::MatchType::kPSL),
      CreateEntryWithBackupPassword("Greg", "5fnd1m", u"backup",
                                    GURL(kExampleSiteSubdomain),
                                    PasswordForm::MatchType::kPSL),
      CreateEntry("Elfi", "a65ddm", GURL(kExampleSiteSubdomain),
                  PasswordForm::MatchType::kPSL),
      CreateEntry("Alf", "R4nd50m", GURL(kExampleSiteMobile),
                  PasswordForm::MatchType::kPSL)};

  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, IsOriginBlocklisted(false), std::nullopt, origin);

  EXPECT_THAT(
      cache()->GetCredentialStore(origin).GetCredentials(),
      testing::ElementsAre(

          // Alphabetical entries of the exactly matching https://example.com:
          MakeUiCredential("Adam", "Pas83B"),
          MakeUiCredential("Berta", "30948", kExampleSite, kExampleSite,
                           password_manager_util::GetLoginMatchType::kExact),
          MakeUiCredential("Berta", "backuppassword", kExampleSite,
                           kExampleSite,
                           password_manager_util::GetLoginMatchType::kExact,
                           IsBackupCredential(true)),
          MakeUiCredential("Carl", "P1238C"),
          // Affiliation based matches are first class citizens and should be
          // treated as a first-party credential.
          MakeUiCredential(
              "Cesar", "V3V1V", kExampleSite, kExampleSite,
              password_manager_util::GetLoginMatchType::kAffiliated),
          MakeUiCredential("Dora", "PakudC"),

          // Alphabetical entries of PSL-match https://accounts.example.com:
          MakeUiCredential("Elfi", "a65ddm", kExampleSiteSubdomain,
                           kExampleSiteSubdomain,
                           password_manager_util::GetLoginMatchType::kPSL),
          MakeUiCredential("Greg", "5fnd1m", kExampleSiteSubdomain,
                           kExampleSiteSubdomain,
                           password_manager_util::GetLoginMatchType::kPSL),
          MakeUiCredential("Greg", "backup", kExampleSiteSubdomain,
                           kExampleSiteSubdomain,
                           password_manager_util::GetLoginMatchType::kPSL,
                           IsBackupCredential(true)),

          // Alphabetical entries of PSL-match https://m.example.com:
          MakeUiCredential("Alf", "R4nd50m", kExampleSiteMobile,
                           kExampleSiteMobile,
                           password_manager_util::GetLoginMatchType::kPSL),
          MakeUiCredential("Rolf", "A4nd0m", kExampleSiteMobile,
                           kExampleSiteMobile,
                           password_manager_util::GetLoginMatchType::kPSL)));
}

#endif

TEST_F(CredentialCacheTest, StoresUnnotifiedSharedCredentialsCredentials) {
  Origin origin = Origin::Create(GURL(kExampleSite));
  const std::string kNonShared = "non_shared";
  const std::string kSharedNotified = "shared_notified";
  const std::string kSharedUnnotified = "shared_unnotified";

  PasswordForm non_shared_credentials = CreateEntry(
      kNonShared, "pass", GURL(kExampleSite), PasswordForm::MatchType::kExact);

  PasswordForm shared_notified_credentials =
      CreateEntry(kSharedNotified, "pass", GURL(kExampleSite),
                  PasswordForm::MatchType::kExact);
  shared_notified_credentials.type = PasswordForm::Type::kReceivedViaSharing;
  shared_notified_credentials.sharing_notification_displayed = true;

  PasswordForm shared_unnotified_credentials =
      CreateEntry(kSharedUnnotified, "pass", GURL(kExampleSite),
                  PasswordForm::MatchType::kExact);
  shared_unnotified_credentials.type = PasswordForm::Type::kReceivedViaSharing;
  shared_unnotified_credentials.sharing_notification_displayed = false;

  std::vector<PasswordForm> matches = {non_shared_credentials,
                                       shared_notified_credentials,
                                       shared_unnotified_credentials};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, IsOriginBlocklisted(false), std::nullopt, origin);

  EXPECT_THAT(
      cache()->GetCredentialStore(origin).GetUnnotifiedSharedCredentials(),
      testing::ElementsAre(shared_unnotified_credentials));

  // Credentials should be sorted such that shared unnotified credentials come
  // first.
  EXPECT_THAT(
      cache()->GetCredentialStore(origin).GetCredentials(),
      testing::ElementsAre(
          Property(&UiCredential::username,
                   base::UTF8ToUTF16(kSharedUnnotified)),
          Property(&UiCredential::username, base::UTF8ToUTF16(kNonShared)),
          Property(&UiCredential::username,
                   base::UTF8ToUTF16(kSharedNotified))));
}

TEST_F(CredentialCacheTest, StoresCredentialsForIndependentOrigins) {
  Origin origin = Origin::Create(GURL(kExampleSite));
  Origin origin2 = Origin::Create(GURL(kExampleSite2));
  std::vector<PasswordForm> matches1 = {CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches1, IsOriginBlocklisted(false), std::nullopt, origin);
  std::vector<PasswordForm> matches2 = {CreateEntry(
      "Abe", "B4dPW", GURL(kExampleSite2), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches2, IsOriginBlocklisted(false), std::nullopt, origin2);

  EXPECT_THAT(cache()->GetCredentialStore(origin).GetCredentials(),
              testing::ElementsAre(MakeUiCredential("Ben", "S3cur3")));
  EXPECT_THAT(
      cache()->GetCredentialStore(origin2).GetCredentials(),
      testing::ElementsAre(MakeUiCredential("Abe", "B4dPW", kExampleSite2)));
}

TEST_F(CredentialCacheTest, ClearsCredentials) {
  Origin origin = Origin::Create(GURL(kExampleSite));
  std::vector<PasswordForm> matches = {CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, IsOriginBlocklisted(false), std::nullopt,
      Origin::Create(GURL(kExampleSite)));
  ASSERT_THAT(cache()->GetCredentialStore(origin).GetCredentials(),
              testing::ElementsAre(MakeUiCredential("Ben", "S3cur3")));

  cache()->ClearCredentials();
  EXPECT_EQ(cache()->GetCredentialStore(origin).GetCredentials().size(), 0u);
}

TEST_F(CredentialCacheTest, StoresBlocklistedWithCredentials) {
  Origin origin = Origin::Create(GURL(kExampleSite));
  std::vector<PasswordForm> matches = {CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, IsOriginBlocklisted(true), std::nullopt,
      Origin::Create(GURL(kExampleSite)));
  EXPECT_EQ(OriginCredentialStore::BlocklistedStatus::kIsBlocklisted,
            cache()->GetCredentialStore(origin).GetBlocklistedStatus());
}

TEST_F(CredentialCacheTest, StoresBackendErrorAndCredentials) {
  Origin origin = Origin::Create(GURL(kExampleSite));

  // Only backend error.
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, IsOriginBlocklisted(false),
      PasswordStoreBackendErrorType::kAuthErrorResolvable,
      Origin::Create(GURL(kExampleSite)));
  EXPECT_EQ(PasswordStoreBackendErrorType::kAuthErrorResolvable,
            cache()->backend_error());
  EXPECT_THAT(cache()->GetCredentialStore(origin).GetCredentials(), IsEmpty());

  // Backend error and matched passwords.
  std::vector<PasswordForm> matches = {CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, IsOriginBlocklisted(false),
      PasswordStoreBackendErrorType::kAuthErrorResolvable,
      Origin::Create(GURL(kExampleSite)));
  EXPECT_EQ(PasswordStoreBackendErrorType::kAuthErrorResolvable,
            cache()->backend_error());
  EXPECT_THAT(cache()->GetCredentialStore(origin).GetCredentials(),
              testing::ElementsAre(MakeUiCredential("Ben", "S3cur3")));

  // Backend error is cleared.
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, IsOriginBlocklisted(false), std::nullopt,
      Origin::Create(GURL(kExampleSite)));
  EXPECT_EQ(std::nullopt, cache()->backend_error());
  EXPECT_THAT(cache()->GetCredentialStore(origin).GetCredentials(),
              testing::ElementsAre(MakeUiCredential("Ben", "S3cur3")));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(CredentialCacheTest, SplitsCredentialsInMainAndBackupFlagEnabled) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(features::kFillRecoveryPassword);

  Origin origin = Origin::Create(GURL(kExampleSite));
  PasswordForm match_with_backup = CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact);
  match_with_backup.SetPasswordBackupNote(u"backuppassword");
  std::vector<PasswordForm> matches = {std::move(match_with_backup)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, IsOriginBlocklisted(false), std::nullopt, origin);

  EXPECT_THAT(
      cache()->GetCredentialStore(origin).GetCredentials(),
      testing::ElementsAre(
          MakeUiCredential("Ben", "S3cur3", kExampleSite),
          MakeUiCredential("Ben", "backuppassword", kExampleSite, kExampleSite,
                           password_manager_util::GetLoginMatchType::kExact,
                           IsBackupCredential(true))));
}

TEST_F(CredentialCacheTest, IgnoresBackupCredentialIfFlagDisabled) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndDisableFeature(features::kFillRecoveryPassword);

  Origin origin = Origin::Create(GURL(kExampleSite));
  PasswordForm match_with_backup = CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact);
  match_with_backup.SetPasswordBackupNote(u"backuppassword");
  std::vector<PasswordForm> matches = {std::move(match_with_backup)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, IsOriginBlocklisted(false), std::nullopt, origin);

  EXPECT_THAT(
      cache()->GetCredentialStore(origin).GetCredentials(),
      testing::ElementsAre(MakeUiCredential("Ben", "S3cur3", kExampleSite)));
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace password_manager
