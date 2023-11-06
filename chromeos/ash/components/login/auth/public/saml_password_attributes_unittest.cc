// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/public/saml_password_attributes.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr int64_t kModified = 155083625842L;
constexpr char kModifiedStr[] = "155083625842";
constexpr int64_t kExpiration = 155083625842L;
constexpr char kExpirationStr[] = "155083625842";
constexpr char kPasswordChangeUrl[] =
    "https://example.com/adfs/portal/updatepassword/";

void ExpectEmpty(const SamlPasswordAttributes& attrs) {
  EXPECT_FALSE(attrs.has_modified_time());
  EXPECT_FALSE(attrs.has_expiration_time());
  EXPECT_FALSE(attrs.has_password_change_url());
}

}  // namespace

TEST(SamlPasswordAttributesTest, FromJs) {
  base::Value::Dict dict;
  SamlPasswordAttributes attrs = SamlPasswordAttributes::FromJs(dict);
  ExpectEmpty(attrs);

  dict.Set("modifiedTime", "");
  dict.Set("expirationTime", "");
  dict.Set("passwordChangeUrl", "");
  attrs = SamlPasswordAttributes::FromJs(dict);
  ExpectEmpty(attrs);

  dict.Set("modifiedTime", kModifiedStr);
  dict.Set("expirationTime", kExpirationStr);
  dict.Set("passwordChangeUrl", kPasswordChangeUrl);
  attrs = SamlPasswordAttributes::FromJs(dict);

  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(kModified),
            attrs.modified_time());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(kExpiration),
            attrs.expiration_time());
  EXPECT_EQ(kPasswordChangeUrl, attrs.password_change_url());

  dict.Set("passwordChangeUrl", "");
  attrs = SamlPasswordAttributes::FromJs(dict);

  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(kModified),
            attrs.modified_time());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(kExpiration),
            attrs.expiration_time());
  EXPECT_FALSE(attrs.has_password_change_url());
}

TEST(SamlPasswordAttributesTest, PrefsSerialization) {
  TestingPrefServiceSimple prefs;
  SamlPasswordAttributes::RegisterProfilePrefs(prefs.registry());

  base::Time modified_time =
      base::Time::FromMillisecondsSinceUnixEpoch(kModified);
  base::Time expiration_time =
      base::Time::FromMillisecondsSinceUnixEpoch(kExpiration);
  SamlPasswordAttributes original(modified_time, expiration_time,
                                  kPasswordChangeUrl);
  original.SaveToPrefs(&prefs);

  SamlPasswordAttributes loaded = SamlPasswordAttributes::LoadFromPrefs(&prefs);
  EXPECT_EQ(modified_time, loaded.modified_time());
  EXPECT_EQ(expiration_time, loaded.expiration_time());
  EXPECT_EQ(kPasswordChangeUrl, loaded.password_change_url());

  SamlPasswordAttributes::DeleteFromPrefs(&prefs);
  loaded = SamlPasswordAttributes::LoadFromPrefs(&prefs);
  ExpectEmpty(loaded);
}

}  // namespace ash
