// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_cache.h"

#include <memory>
#include <string>

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using url::Origin;
using IsOriginBlocklisted = CredentialCache::IsOriginBlocklisted;

constexpr char kExampleSite[] = "https://example.com/";
constexpr char kExampleSite2[] = "https://example.two.com/";
constexpr char kExampleSiteMobile[] = "https://m.example.com/";
constexpr char kExampleSiteSubdomain[] = "https://accounts.example.com/";

UiCredential MakeUiCredential(
    base::StringPiece username,
    base::StringPiece password,
    base::StringPiece origin = kExampleSite,
    password_manager_util::GetLoginMatchType match_type =
        password_manager_util::GetLoginMatchType::kExact) {
  return UiCredential(base::UTF8ToUTF16(username), base::UTF8ToUTF16(password),
                      Origin::Create(GURL(origin)), match_type, base::Time());
}

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
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Berta", "30948", GURL(kExampleSite), false, false).get(),
       CreateEntry("Adam", "Pas83B", GURL(kExampleSite), false, false).get(),
       CreateEntry("Dora", "PakudC", GURL(kExampleSite), false, false).get(),
       CreateEntry("Carl", "P1238C", GURL(kExampleSite), false, false).get(),
       // These entries need to be ordered but come after the examples above.
       CreateEntry("Cesar", "V3V1V", GURL(kExampleSite), false, true).get(),
       CreateEntry("Rolf", "A4nd0m", GURL(kExampleSiteMobile), true, false)
           .get(),
       CreateEntry("Greg", "5fnd1m", GURL(kExampleSiteSubdomain), true, false)
           .get(),
       CreateEntry("Elfi", "a65ddm", GURL(kExampleSiteSubdomain), true, false)
           .get(),
       CreateEntry("Alf", "R4nd50m", GURL(kExampleSiteMobile), true, false)
           .get()},
      IsOriginBlocklisted(false), origin);

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
              "Cesar", "V3V1V", kExampleSite,
              password_manager_util::GetLoginMatchType::kAffiliated),
          MakeUiCredential("Dora", "PakudC"),

          // Alphabetical entries of PSL-match https://accounts.example.com:
          MakeUiCredential("Elfi", "a65ddm", kExampleSiteSubdomain,
                           password_manager_util::GetLoginMatchType::kPSL),
          MakeUiCredential("Greg", "5fnd1m", kExampleSiteSubdomain,
                           password_manager_util::GetLoginMatchType::kPSL),

          // Alphabetical entries of PSL-match https://m.example.com:
          MakeUiCredential("Alf", "R4nd50m", kExampleSiteMobile,
                           password_manager_util::GetLoginMatchType::kPSL),
          MakeUiCredential("Rolf", "A4nd0m", kExampleSiteMobile,
                           password_manager_util::GetLoginMatchType::kPSL)));
}

TEST_F(CredentialCacheTest, StoredCredentialsForIndependentOrigins) {
  Origin origin = Origin::Create(GURL(kExampleSite));
  Origin origin2 = Origin::Create(GURL(kExampleSite2));

  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false, false).get()},
      IsOriginBlocklisted(false), origin);
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Abe", "B4dPW", GURL(kExampleSite2), false, false).get()},
      IsOriginBlocklisted(false), origin2);

  EXPECT_THAT(cache()->GetCredentialStore(origin).GetCredentials(),
              testing::ElementsAre(MakeUiCredential("Ben", "S3cur3")));
  EXPECT_THAT(
      cache()->GetCredentialStore(origin2).GetCredentials(),
      testing::ElementsAre(MakeUiCredential("Abe", "B4dPW", kExampleSite2)));
}

TEST_F(CredentialCacheTest, ClearsCredentials) {
  Origin origin = Origin::Create(GURL(kExampleSite));
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false, false).get()},
      IsOriginBlocklisted(false), Origin::Create(GURL(kExampleSite)));
  ASSERT_THAT(cache()->GetCredentialStore(origin).GetCredentials(),
              testing::ElementsAre(MakeUiCredential("Ben", "S3cur3")));

  cache()->ClearCredentials();
  EXPECT_EQ(cache()->GetCredentialStore(origin).GetCredentials().size(), 0u);
}

TEST_F(CredentialCacheTest, StoresBlocklistedWithCredentials) {
  Origin origin = Origin::Create(GURL(kExampleSite));
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false, false).get()},
      IsOriginBlocklisted(true), Origin::Create(GURL(kExampleSite)));
  EXPECT_EQ(OriginCredentialStore::BlocklistedStatus::kIsBlocklisted,
            cache()->GetCredentialStore(origin).GetBlocklistedStatus());
}

}  // namespace password_manager
