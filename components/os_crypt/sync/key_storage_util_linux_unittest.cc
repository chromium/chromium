// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/sync/key_storage_util_linux.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace os_crypt {
namespace {

// Set the setting that disables using OS-level encryption. `use` determines
// whether a backend will be used or not.
bool WriteBackendUse(const base::FilePath& user_data_dir, bool use) {
  if (user_data_dir.empty()) {
    return false;
  }
  base::FilePath pref_path = user_data_dir.Append(kBackendPreferenceFileName);
  if (use) {
    return base::DeleteFile(pref_path);
  }
  FILE* f = base::OpenFile(pref_path, "w");
  return f && base::CloseFile(f);
}

class KeyStorageUtilLinuxPreferenceTest : public testing::Test {
 public:
  KeyStorageUtilLinuxPreferenceTest() = default;

  KeyStorageUtilLinuxPreferenceTest(const KeyStorageUtilLinuxPreferenceTest&) =
      delete;
  KeyStorageUtilLinuxPreferenceTest& operator=(
      const KeyStorageUtilLinuxPreferenceTest&) = delete;

  ~KeyStorageUtilLinuxPreferenceTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(base::CreateNewTempDirectory("", &fake_user_data_dir_));
  }

  void TearDown() override {
    ASSERT_TRUE(base::DeletePathRecursively(fake_user_data_dir_));
  }

 protected:
  base::FilePath fake_user_data_dir_;
};

TEST_F(KeyStorageUtilLinuxPreferenceTest, FirstTimeDefaultsToTrue) {
  EXPECT_TRUE(GetBackendUse(fake_user_data_dir_));
}

TEST_F(KeyStorageUtilLinuxPreferenceTest, SetToTrue) {
  EXPECT_TRUE(WriteBackendUse(fake_user_data_dir_, true));
  EXPECT_TRUE(GetBackendUse(fake_user_data_dir_));
}

TEST_F(KeyStorageUtilLinuxPreferenceTest, SetToFalse) {
  EXPECT_TRUE(WriteBackendUse(fake_user_data_dir_, false));
  EXPECT_FALSE(GetBackendUse(fake_user_data_dir_));
}

TEST_F(KeyStorageUtilLinuxPreferenceTest, MultipleWrites) {
  EXPECT_TRUE(WriteBackendUse(fake_user_data_dir_, false));
  EXPECT_FALSE(GetBackendUse(fake_user_data_dir_));

  EXPECT_TRUE(WriteBackendUse(fake_user_data_dir_, true));
  EXPECT_TRUE(GetBackendUse(fake_user_data_dir_));

  EXPECT_TRUE(WriteBackendUse(fake_user_data_dir_, false));
  EXPECT_FALSE(GetBackendUse(fake_user_data_dir_));
}

using KeyStorageUtilLinuxTest = testing::Test;

TEST_F(KeyStorageUtilLinuxTest, PasswordStoreFlagOverrides) {
  SelectedLinuxBackend selected = SelectBackend(
      "basic", true, base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_GNOME);
  EXPECT_EQ(selected, SelectedLinuxBackend::BASIC_TEXT);

  selected =
      SelectBackend("gnome-libsecret", false,
                    base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_KDE4);
  EXPECT_EQ(selected, SelectedLinuxBackend::GNOME_LIBSECRET);

  selected =
      SelectBackend("gnome-libsecret", true,
                    base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_KDE4);
  EXPECT_EQ(selected, SelectedLinuxBackend::GNOME_LIBSECRET);
}

TEST_F(KeyStorageUtilLinuxTest, IgnoreBackends) {
  SelectedLinuxBackend selected = SelectBackend(
      "", false, base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_GNOME);
  EXPECT_EQ(selected, SelectedLinuxBackend::BASIC_TEXT);

  selected = SelectBackend(
      "", true, base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_KDE5);
  EXPECT_EQ(selected, SelectedLinuxBackend::KWALLET5);

  selected = SelectBackend(
      "", false, base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_KDE5);
  EXPECT_EQ(selected, SelectedLinuxBackend::BASIC_TEXT);
}

}  // namespace
}  // namespace os_crypt
