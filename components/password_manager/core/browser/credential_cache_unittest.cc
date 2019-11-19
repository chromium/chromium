// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_cache.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using url::Origin;

using IsPublicSuffixMatch = CredentialPair::IsPublicSuffixMatch;

constexpr char kExampleSiteAndroidApp[] = "android://3x4mpl3@com.example.app/";
constexpr char kExampleSite[] = "https://example.com";
constexpr char kExampleSite2[] = "https://example.two.com";
constexpr char kExampleSiteMobile[] = "https://m.example.com";
constexpr char kExampleSiteSubdomain[] = "https://accounts.example.com";

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
  cache()->SaveCredentialsForOrigin(
      {CreateEntry("Berta", "30948", GURL(kExampleSite), false).first,
       CreateEntry("Adam", "Pas83B", GURL(kExampleSite), false).first,
       CreateEntry("Dora", "PakudC", GURL(kExampleSite), false).first,
       CreateEntry("Carl", "P1238C", GURL(kExampleSite), false).first,
       // These entries need to be ordered but come after the examples above.
       CreateEntry("Cesar", "V3V1V", GURL(kExampleSiteAndroidApp), false).first,
       CreateEntry("Rolf", "A4nd0m", GURL(kExampleSiteMobile), true).first,
       CreateEntry("Greg", "5fnd1m", GURL(kExampleSiteSubdomain), true).first,
       CreateEntry("Elfi", "a65ddm", GURL(kExampleSiteSubdomain), true).first,
       CreateEntry("Alf", "R4nd50m", GURL(kExampleSiteMobile), true).first},
      origin);

  EXPECT_THAT(
      cache()->GetCredentialStore(origin).GetCredentials(),
      testing::ElementsAre(

          // Alphabetical entries of the exactly matching https://example.com:
          CredentialPair(ASCIIToUTF16("Adam"), ASCIIToUTF16("Pas83B"),
                         GURL(kExampleSite), IsPublicSuffixMatch(false)),
          CredentialPair(ASCIIToUTF16("Berta"), ASCIIToUTF16("30948"),
                         GURL(kExampleSite), IsPublicSuffixMatch(false)),
          CredentialPair(ASCIIToUTF16("Carl"), ASCIIToUTF16("P1238C"),
                         GURL(kExampleSite), IsPublicSuffixMatch(false)),
          CredentialPair(ASCIIToUTF16("Dora"), ASCIIToUTF16("PakudC"),
                         GURL(kExampleSite), IsPublicSuffixMatch(false)),

          // Entry for PSL-match android://3x4mpl3@com.example.app:
          CredentialPair(ASCIIToUTF16("Cesar"), ASCIIToUTF16("V3V1V"),
                         GURL(kExampleSiteAndroidApp),
                         IsPublicSuffixMatch(false)),

          // Alphabetical entries of PSL-match https://accounts.example.com:
          CredentialPair(ASCIIToUTF16("Elfi"), ASCIIToUTF16("a65ddm"),
                         GURL(kExampleSiteSubdomain),
                         IsPublicSuffixMatch(true)),
          CredentialPair(ASCIIToUTF16("Greg"), ASCIIToUTF16("5fnd1m"),
                         GURL(kExampleSiteSubdomain),
                         IsPublicSuffixMatch(true)),

          // Alphabetical entries of PSL-match https://m.example.com:
          CredentialPair(ASCIIToUTF16("Alf"), ASCIIToUTF16("R4nd50m"),
                         GURL(kExampleSiteMobile), IsPublicSuffixMatch(true)),
          CredentialPair(ASCIIToUTF16("Rolf"), ASCIIToUTF16("A4nd0m"),
                         GURL(kExampleSiteMobile), IsPublicSuffixMatch(true))));
}

TEST_F(CredentialCacheTest, StoredCredentialsForIndependentOrigins) {
  Origin origin = Origin::Create(GURL(kExampleSite));
  Origin origin2 = Origin::Create(GURL(kExampleSite2));

  cache()->SaveCredentialsForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false).first}, origin);
  cache()->SaveCredentialsForOrigin(
      {CreateEntry("Abe", "B4dPW", GURL(kExampleSite2), false).first}, origin2);

  EXPECT_THAT(cache()->GetCredentialStore(origin).GetCredentials(),
              testing::ElementsAre(CredentialPair(
                  ASCIIToUTF16("Ben"), ASCIIToUTF16("S3cur3"),
                  GURL(kExampleSite), IsPublicSuffixMatch(false))));
  EXPECT_THAT(cache()->GetCredentialStore(origin2).GetCredentials(),
              testing::ElementsAre(CredentialPair(
                  ASCIIToUTF16("Abe"), ASCIIToUTF16("B4dPW"),
                  GURL(kExampleSite2), IsPublicSuffixMatch(false))));
}

TEST_F(CredentialCacheTest, ClearsCredentials) {
  Origin origin = Origin::Create(GURL(kExampleSite));
  cache()->SaveCredentialsForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false).first},
      Origin::Create(GURL(kExampleSite)));
  ASSERT_THAT(cache()->GetCredentialStore(origin).GetCredentials(),
              testing::ElementsAre(CredentialPair(
                  ASCIIToUTF16("Ben"), ASCIIToUTF16("S3cur3"),
                  GURL(kExampleSite), IsPublicSuffixMatch(false))));

  cache()->ClearCredentials();
  EXPECT_EQ(cache()->GetCredentialStore(origin).GetCredentials().size(), 0u);
}

}  // namespace password_manager
