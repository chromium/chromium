// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/public/key.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const char kPassword[] = "password";
const char kLabel[] = "label";
const char kSalt[] =
    "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef";

}  // namespace

TEST(KeyTest, ClearSecret) {
  Key key(kPassword);
  key.SetLabel(kLabel);
  EXPECT_EQ(Key::KEY_TYPE_PASSWORD_PLAIN, key.GetKeyType());
  EXPECT_EQ(kPassword, key.GetSecret());
  EXPECT_EQ(kLabel, key.GetLabel());

  key.ClearSecret();
  EXPECT_EQ(Key::KEY_TYPE_PASSWORD_PLAIN, key.GetKeyType());
  EXPECT_TRUE(key.GetSecret().empty());
  EXPECT_EQ(kLabel, key.GetLabel());
}

TEST(KeyTest, TransformToSaltedSHA256TopHalf) {
  Key key(kPassword);
  key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, kSalt);
  EXPECT_EQ(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, key.GetKeyType());
  EXPECT_EQ("5b01941771e47fa408380aa675703f4f", key.GetSecret());
}

TEST(KeyTest, TransformToSaltedAES2561234) {
  Key key(kPassword);
  key.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, kSalt);
  EXPECT_EQ(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, key.GetKeyType());
  EXPECT_EQ("GUkNnvqoULf/cXbZscVUnANmLBB0ovjGZsj1sKzP5BE=", key.GetSecret());
}

TEST(KeyTest, TransformToSaltedSHA256) {
  Key key(kPassword);
  key.Transform(Key::KEY_TYPE_SALTED_SHA256, kSalt);
  EXPECT_EQ(Key::KEY_TYPE_SALTED_SHA256, key.GetKeyType());
  EXPECT_EQ("WwGUF3Hkf6QIOAqmdXA/TyScTFDo4d+ow5xfof0zGdo=", key.GetSecret());
}

// The values in the KeyType enum must never change because they are stored as
// ints in the user's cryptohome key metadata.
TEST(KeyTest, KeyTypeStable) {
  EXPECT_EQ(0, Key::KEY_TYPE_PASSWORD_PLAIN);
  EXPECT_EQ(1, Key::KEY_TYPE_SALTED_SHA256_TOP_HALF);
  EXPECT_EQ(2, Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234);
  EXPECT_EQ(3, Key::KEY_TYPE_SALTED_SHA256);
  // The sentinel does not have to remain stable. It should be adjusted whenever
  // a new key type is added.
  EXPECT_EQ(4, Key::KEY_TYPE_COUNT);
}

}  // namespace ash
