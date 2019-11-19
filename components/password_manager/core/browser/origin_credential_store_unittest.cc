// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/origin_credential_store.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {
namespace {

using base::ASCIIToUTF16;
using testing::ElementsAre;

using IsPublicSuffixMatch = CredentialPair::IsPublicSuffixMatch;

constexpr char kExampleSite[] = "https://example.com";
constexpr char kExampleSiteAndroidApp[] = "android://3x4mpl3@com.example.app/";

CredentialPair CreateCredentials(std::string username, std::string password) {
  return CredentialPair(base::ASCIIToUTF16(std::move(username)),
                        base::ASCIIToUTF16(std::move(password)),
                        GURL(kExampleSite), IsPublicSuffixMatch(false));
}

}  // namespace

class OriginCredentialStoreTest : public testing::Test {
 public:
  OriginCredentialStore* store() { return &store_; }

 private:
  OriginCredentialStore store_{url::Origin::Create(GURL(kExampleSite))};
};

TEST_F(OriginCredentialStoreTest, StoresCredentials) {
  store()->SaveCredentials({CreateCredentials("Berta", "30948"),
                            CreateCredentials("Adam", "Pas83B"),
                            CreateCredentials("Dora", "PakudC"),
                            CreateCredentials("Carl", "P1238C")});

  EXPECT_THAT(store()->GetCredentials(),
              ElementsAre(CreateCredentials("Berta", "30948"),
                          CreateCredentials("Adam", "Pas83B"),
                          CreateCredentials("Dora", "PakudC"),
                          CreateCredentials("Carl", "P1238C")));
}

TEST_F(OriginCredentialStoreTest, StoresOnlyNormalizedOrigins) {
  store()->SaveCredentials(
      {CredentialPair(base::ASCIIToUTF16("Berta"), base::ASCIIToUTF16("30948"),
                      GURL(kExampleSite), IsPublicSuffixMatch(false)),
       CredentialPair(base::ASCIIToUTF16("Adam"), base::ASCIIToUTF16("Pas83B"),
                      GURL(kExampleSite).Resolve("/agbs"),
                      IsPublicSuffixMatch(false)),
       CredentialPair(base::ASCIIToUTF16("Dora"), base::ASCIIToUTF16("PakudC"),
                      GURL(kExampleSiteAndroidApp),
                      IsPublicSuffixMatch(false))});

  EXPECT_THAT(
      store()->GetCredentials(),
      ElementsAre(

          // The URL that equals an origin stays untouched.
          CredentialPair(base::ASCIIToUTF16("Berta"),
                         base::ASCIIToUTF16("30948"), GURL(kExampleSite),
                         IsPublicSuffixMatch(false)),

          // The longer URL is reduced to an origin.
          CredentialPair(base::ASCIIToUTF16("Adam"),
                         base::ASCIIToUTF16("Pas83B"), GURL(kExampleSite),
                         IsPublicSuffixMatch(false)),

          // The android origin stays untouched.
          CredentialPair(
              base::ASCIIToUTF16("Dora"), base::ASCIIToUTF16("PakudC"),
              GURL(kExampleSiteAndroidApp), IsPublicSuffixMatch(false))));
}

TEST_F(OriginCredentialStoreTest, ReplacesCredentials) {
  store()->SaveCredentials({CreateCredentials("Ben", "S3cur3")});
  ASSERT_THAT(store()->GetCredentials(),
              ElementsAre(CreateCredentials("Ben", "S3cur3")));

  store()->SaveCredentials({CreateCredentials(std::string(), "M1@u")});
  EXPECT_THAT(store()->GetCredentials(),
              ElementsAre(CreateCredentials(std::string(), "M1@u")));
}

TEST_F(OriginCredentialStoreTest, ClearsCredentials) {
  store()->SaveCredentials({CreateCredentials("Ben", "S3cur3")});
  ASSERT_THAT(store()->GetCredentials(),
              ElementsAre(CreateCredentials("Ben", "S3cur3")));

  store()->ClearCredentials();
  EXPECT_EQ(store()->GetCredentials().size(), 0u);
}

}  // namespace password_manager
