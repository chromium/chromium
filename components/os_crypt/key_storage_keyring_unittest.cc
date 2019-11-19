// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/key_storage_keyring.h"

#include <cstdarg>  // Needed to mock ellipsis
#include <string>

#include "base/macros.h"
#include "base/test/test_simple_task_runner.h"
#include "build/branding_buildflags.h"
#include "components/os_crypt/keyring_util_linux.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kApplicationName[] = "chrome";
#else
const char kApplicationName[] = "chromium";
#endif

// Replaces some of GnomeKeyringLoader's methods with mocked ones.
class MockGnomeKeyringLoader : public GnomeKeyringLoader {
 public:
  static void ResetForOSCrypt() {
    GnomeKeyringLoader::gnome_keyring_find_password_sync_ptr =
        &mock_gnome_keyring_find_password_sync;
    GnomeKeyringLoader::gnome_keyring_store_password_sync_ptr =
        &mock_gnome_keyring_store_password_sync;
    GnomeKeyringLoader::gnome_keyring_free_password_ptr =
        &mock_gnome_keyring_free_password;

    delete s_password_ptr_;
    s_password_ptr_ = nullptr;

    // GnomeKeyringLoader does not (re)load keyring is this is true.
    GnomeKeyringLoader::keyring_loaded = true;
  }

  static void SetOSCryptPassword(const char* password) {
    delete s_password_ptr_;
    s_password_ptr_ = new std::string(password);
  }

  static void TearDown() {
    delete s_password_ptr_;
    s_password_ptr_ = nullptr;
    // Function pointers will be reset the next time loading is requested.
    GnomeKeyringLoader::keyring_loaded = false;
  }

 private:
  // These methods are used to redirect calls through GnomeKeyringLoader.
  static GnomeKeyringResult mock_gnome_keyring_find_password_sync(
      const GnomeKeyringPasswordSchema* schema,
      gchar** password,
      ...);

  static GnomeKeyringResult mock_gnome_keyring_store_password_sync(
      const GnomeKeyringPasswordSchema* schema,
      const gchar* keyring,
      const gchar* display_name,
      const gchar* password,
      ...);

  static void mock_gnome_keyring_free_password(gchar* password);

  static std::string* s_password_ptr_;
};

std::string* MockGnomeKeyringLoader::s_password_ptr_ = nullptr;

// static
GnomeKeyringResult
MockGnomeKeyringLoader::mock_gnome_keyring_find_password_sync(
    const GnomeKeyringPasswordSchema* schema,
    gchar** password,
    ...) {
  va_list attrs;
  va_start(attrs, password);
  EXPECT_STREQ("application", va_arg(attrs, const char*));
  EXPECT_STREQ(kApplicationName, va_arg(attrs, const char*));
  EXPECT_EQ(nullptr, va_arg(attrs, const char*));
  va_end(attrs);

  if (!s_password_ptr_)
    return GNOME_KEYRING_RESULT_NO_MATCH;
  *password = strdup(s_password_ptr_->c_str());
  return GNOME_KEYRING_RESULT_OK;
}

// static
GnomeKeyringResult
MockGnomeKeyringLoader::mock_gnome_keyring_store_password_sync(
    const GnomeKeyringPasswordSchema* schema,
    const gchar* keyring,
    const gchar* display_name,
    const gchar* password,
    ...) {
  va_list attrs;
  va_start(attrs, password);
  EXPECT_STREQ("application", va_arg(attrs, const char*));
  EXPECT_STREQ(kApplicationName, va_arg(attrs, const char*));
  EXPECT_EQ(nullptr, va_arg(attrs, const char*));
  va_end(attrs);

  delete s_password_ptr_;
  s_password_ptr_ = new std::string(password);
  return GNOME_KEYRING_RESULT_OK;
}

// static
void MockGnomeKeyringLoader::mock_gnome_keyring_free_password(gchar* password) {
  free(password);  // We are mocking a C function.
}

class GnomeKeyringTest : public testing::Test {
 public:
  GnomeKeyringTest();
  ~GnomeKeyringTest() override;

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  KeyStorageKeyring keyring_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GnomeKeyringTest);
};

GnomeKeyringTest::GnomeKeyringTest()
    : task_runner_(new base::TestSimpleTaskRunner()), keyring_(task_runner_) {
  MockGnomeKeyringLoader::ResetForOSCrypt();
}

GnomeKeyringTest::~GnomeKeyringTest() {
  MockGnomeKeyringLoader::TearDown();
}

TEST_F(GnomeKeyringTest, KeyringRepeats) {
  std::string password = keyring_.GetKey();
  EXPECT_FALSE(password.empty());
  std::string password_repeat = keyring_.GetKey();
  EXPECT_EQ(password, password_repeat);
}

TEST_F(GnomeKeyringTest, KeyringCreatesRandomised) {
  std::string password = keyring_.GetKey();
  MockGnomeKeyringLoader::ResetForOSCrypt();
  std::string password_new = keyring_.GetKey();
  EXPECT_NE(password, password_new);
}

}  // namespace
