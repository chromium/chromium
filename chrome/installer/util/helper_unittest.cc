// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/helper.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/initial_preferences.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace installer {

struct Params {
  Params(bool system_level, absl::optional<int> target_dir_key)
      : system_level(system_level), target_dir_key(target_dir_key) {}
  bool system_level;
  absl::optional<int> target_dir_key;
};

// Tests GetChromeInstallPath with a params object that contains a boolean
// |system_level| which is |true| if the test must use system-level values or
// |false| it the test must use user-level values, and an optional
// |target_dir_key| in which the installation should be made. When no value is
// set for |target_dir_key|, assume an empty path.
class GetChromeInstallPathTest : public testing::TestWithParam<Params> {
 public:
  GetChromeInstallPathTest() = default;

  void SetUp() override {
    ASSERT_TRUE(program_files_.CreateUniqueTempDir());
    ASSERT_TRUE(program_files_x86_.CreateUniqueTempDir());
    ASSERT_TRUE(random_.CreateUniqueTempDir());
    ASSERT_TRUE(local_app_data_.CreateUniqueTempDir());
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    program_files_override_.emplace(base::DIR_PROGRAM_FILES,
                                    program_files_path());
    program_files_x86_override_.emplace(base::DIR_PROGRAM_FILESX86,
                                        program_files_x86_path());
    local_data_app_override_.emplace(base::DIR_LOCAL_APP_DATA,
                                     local_app_data_path());
  }

  base::FilePath random_path() const { return random_.GetPath(); }
  base::FilePath program_files_path() const { return program_files_.GetPath(); }
  base::FilePath program_files_x86_path() const {
    return program_files_x86_.GetPath();
  }
  base::FilePath local_app_data_path() const {
    return local_app_data_.GetPath();
  }
  static bool is_system_level() { return GetParam().system_level; }

  base::FilePath GetExpectedPath(bool system_level) {
    auto path = system_level ? program_files_path() : local_app_data_path();
    return path.Append(install_static::GetChromeInstallSubDirectory())
        .Append(kInstallBinaryDir);
  }

  static base::win::RegKey GetClientsRegKey() {
    return base::win::RegKey(
        is_system_level() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
        install_static::GetClientsKeyPath().c_str(),
        KEY_SET_VALUE | KEY_WOW64_32KEY);
  }

  static base::win::RegKey GetClientStateRegKey() {
    return base::win::RegKey(
        is_system_level() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
        install_static::GetClientStateKeyPath().c_str(),
        KEY_SET_VALUE | KEY_WOW64_32KEY);
  }

 private:
  base::ScopedTempDir program_files_;
  base::ScopedTempDir program_files_x86_;
  base::ScopedTempDir random_;
  base::ScopedTempDir local_app_data_;
  registry_util::RegistryOverrideManager registry_override_manager_;
  absl::optional<base::ScopedPathOverride> program_files_override_;
  absl::optional<base::ScopedPathOverride> program_files_x86_override_;
  absl::optional<base::ScopedPathOverride> local_data_app_override_;
};

TEST_P(GetChromeInstallPathTest, NoRegistryValue) {
  EXPECT_EQ(GetChromeInstallPath(is_system_level()),
            GetExpectedPath(is_system_level()));
}

TEST_P(GetChromeInstallPathTest, RegistryValueSet) {
  base::win::RegKey client_state_key(GetClientStateRegKey());
  ASSERT_EQ(client_state_key.WriteValue(
                kUninstallStringField,
                random_path()
                    .Append(install_static::GetChromeInstallSubDirectory())
                    .Append(kInstallBinaryDir)
                    .AppendASCII("1.0.0.0\\Installer\\setup.exe")
                    .value()
                    .c_str()),
            ERROR_SUCCESS);

  base::win::RegKey client_key(GetClientsRegKey());
  ASSERT_EQ(client_key.WriteValue(google_update::kRegVersionField, L"1.0.0.0"),
            ERROR_SUCCESS);
  EXPECT_EQ(GetChromeInstallPath(is_system_level()),
            random_path()
                .Append(install_static::GetChromeInstallSubDirectory())
                .Append(kInstallBinaryDir));
}

TEST_P(GetChromeInstallPathTest, RegistryValueSetWrongScope) {
  base::win::RegKey client_state_key(GetClientStateRegKey());
  ASSERT_EQ(client_state_key.WriteValue(
                kUninstallStringField,
                random_path()
                    .Append(install_static::GetChromeInstallSubDirectory())
                    .Append(kInstallBinaryDir)
                    .AppendASCII("1.0.0.0\\Installer\\setup.exe")
                    .value()
                    .c_str()),
            ERROR_SUCCESS);

  base::win::RegKey client_key(GetClientsRegKey());
  ASSERT_EQ(client_key.WriteValue(google_update::kRegVersionField, L"1.0.0.0"),
            ERROR_SUCCESS);
  EXPECT_EQ(GetChromeInstallPath(!is_system_level()),
            GetExpectedPath(!is_system_level()));
}

TEST_P(GetChromeInstallPathTest, RegistryValueSetNoProductVersion) {
  base::win::RegKey client_state_key(GetClientStateRegKey());
  ASSERT_EQ(client_state_key.WriteValue(
                kUninstallStringField,
                random_path()
                    .Append(install_static::GetChromeInstallSubDirectory())
                    .Append(kInstallBinaryDir)
                    .AppendASCII("1.0.0.0\\Installer\\setup.exe")
                    .value()
                    .c_str()),
            ERROR_SUCCESS);
  EXPECT_EQ(GetChromeInstallPath(is_system_level()),
            GetExpectedPath(is_system_level()));
}

INSTANTIATE_TEST_SUITE_P(UserLevelTest,
                         GetChromeInstallPathTest,
                         testing::Values<Params>({false, absl::nullopt}));
INSTANTIATE_TEST_SUITE_P(SystemLevelTest,
                         GetChromeInstallPathTest,
                         testing::Values<Params>({true, absl::nullopt}));

// Tests GetChromeInstallPath with a params object that contains a boolean
// |system_level| which is |true| if the test must use system-level values or
// |false| it the test must use user-level values, and a |target_dir| path in
// which the installation should be made.
class GetChromeInstallPathWithPrefsTest : public GetChromeInstallPathTest {
 public:
  GetChromeInstallPathWithPrefsTest() = default;

  base::FilePath GetExpectedPathForSetup(bool system_level,
                                         base::FilePath target_dir) {
    if (system_level && !target_dir.empty() &&
        (target_dir == program_files_path() ||
         target_dir == program_files_x86_path())) {
      return target_dir.Append(install_static::GetChromeInstallSubDirectory())
          .Append(kInstallBinaryDir);
    }
    return GetExpectedPath(system_level);
  }

  static base::FilePath target_dir() {
    base::FilePath result;
    if (GetParam().target_dir_key.has_value())
      base::PathService::Get(GetParam().target_dir_key.value(), &result);
    return result;
  }

  static base::Value::Dict prefs_json() {
    base::FilePath result;
    if (GetParam().target_dir_key.has_value())
      base::PathService::Get(GetParam().target_dir_key.value(), &result);
    base::Value::Dict distribution;
    distribution.SetByDottedPath("distribution.program_files_dir",
                                 result.AsUTF8Unsafe());
    return distribution;
  }
};

TEST_P(GetChromeInstallPathWithPrefsTest, NoRegistryValue) {
  EXPECT_EQ(GetChromeInstallPathWithPrefs(is_system_level(),
                                          InitialPreferences(prefs_json())),
            GetExpectedPathForSetup(is_system_level(), target_dir()));
}

TEST_P(GetChromeInstallPathWithPrefsTest, RegistryValueSet) {
  base::win::RegKey client_state_key(GetClientStateRegKey());
  ASSERT_EQ(client_state_key.WriteValue(
                kUninstallStringField,
                random_path()
                    .Append(install_static::GetChromeInstallSubDirectory())
                    .Append(kInstallBinaryDir)
                    .AppendASCII("1.0.0.0\\Installer\\setup.exe")
                    .value()
                    .c_str()),
            ERROR_SUCCESS);

  base::win::RegKey client_key(GetClientsRegKey());
  ASSERT_EQ(client_key.WriteValue(google_update::kRegVersionField, L"1.0.0.0"),
            ERROR_SUCCESS);
  EXPECT_EQ(GetChromeInstallPathWithPrefs(is_system_level(),
                                          InitialPreferences(prefs_json())),
            random_path()
                .Append(install_static::GetChromeInstallSubDirectory())
                .Append(kInstallBinaryDir));
}

TEST_P(GetChromeInstallPathWithPrefsTest, RegistryValueSetWrongScope) {
  base::win::RegKey client_state_key(GetClientStateRegKey());
  ASSERT_EQ(client_state_key.WriteValue(
                kUninstallStringField,
                random_path()
                    .Append(install_static::GetChromeInstallSubDirectory())
                    .Append(kInstallBinaryDir)
                    .AppendASCII("1.0.0.0\\Installer\\setup.exe")
                    .value()
                    .c_str()),
            ERROR_SUCCESS);

  base::win::RegKey client_key(GetClientsRegKey());
  ASSERT_EQ(client_key.WriteValue(google_update::kRegVersionField, L"1.0.0.0"),
            ERROR_SUCCESS);
  EXPECT_EQ(GetChromeInstallPathWithPrefs(!is_system_level(),
                                          InitialPreferences(prefs_json())),
            GetExpectedPathForSetup(!is_system_level(), target_dir()));
}

TEST_P(GetChromeInstallPathWithPrefsTest, RegistryValueSetNoProductVersion) {
  base::win::RegKey client_state_key(GetClientStateRegKey());
  ASSERT_EQ(client_state_key.WriteValue(
                kUninstallStringField,
                random_path()
                    .Append(install_static::GetChromeInstallSubDirectory())
                    .Append(kInstallBinaryDir)
                    .AppendASCII("1.0.0.0\\Installer\\setup.exe")
                    .value()
                    .c_str()),
            ERROR_SUCCESS);
  EXPECT_EQ(GetChromeInstallPathWithPrefs(is_system_level(),
                                          InitialPreferences(prefs_json())),
            GetExpectedPathForSetup(is_system_level(), target_dir()));
}

INSTANTIATE_TEST_SUITE_P(
    UserLevelX86SetupTest,
    GetChromeInstallPathWithPrefsTest,
    testing::Values<Params>(Params(false, base::DIR_PROGRAM_FILESX86)));

INSTANTIATE_TEST_SUITE_P(
    UserLevelX64SetupTest,
    GetChromeInstallPathWithPrefsTest,
    testing::Values<Params>(Params(false, base::DIR_PROGRAM_FILES)));

INSTANTIATE_TEST_SUITE_P(UserLevelUnsupportedPathSetupTest,
                         GetChromeInstallPathWithPrefsTest,
                         testing::Values<Params>(Params(false,
                                                        base::DIR_HOME)));

INSTANTIATE_TEST_SUITE_P(UserLevelEmptyPathSetupTest,
                         GetChromeInstallPathWithPrefsTest,
                         testing::Values<Params>(Params(false, absl::nullopt)));

INSTANTIATE_TEST_SUITE_P(
    MachineLevelX86SetupTest,
    GetChromeInstallPathWithPrefsTest,
    testing::Values<Params>(Params(true, base::DIR_PROGRAM_FILESX86)));

INSTANTIATE_TEST_SUITE_P(
    MachineLevelX64SetupTest,
    GetChromeInstallPathWithPrefsTest,
    testing::Values<Params>(Params(true, base::DIR_PROGRAM_FILES)));

INSTANTIATE_TEST_SUITE_P(MachineLevelUnsupportedPathSetupTest,
                         GetChromeInstallPathWithPrefsTest,
                         testing::Values<Params>(Params(true, base::DIR_HOME)));

INSTANTIATE_TEST_SUITE_P(MachineLevelEmptyPathSetupTest,
                         GetChromeInstallPathWithPrefsTest,
                         testing::Values<Params>(Params(true, absl::nullopt)));

}  // namespace installer
