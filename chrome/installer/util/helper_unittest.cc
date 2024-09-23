// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/helper.h"

#include <windows.h>

#include <optional>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "build/build_config.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/initial_preferences.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

namespace {

// A helper that overrides an environment variable for the lifetime of an
// instance.
class ScopedEnvironmentOverride {
 public:
  ScopedEnvironmentOverride(std::wstring_view name, const wchar_t* new_value)
      : name_(name) {
    std::array<wchar_t, MAX_PATH> value;
    value[0] = L'\0';
    DWORD len =
        ::GetEnvironmentVariableW(name_.c_str(), value.data(), value.size());
    if (len > 0 && len < value.size()) {
      old_value_.emplace(value.data(), len);
    }
    ::SetEnvironmentVariableW(name_.c_str(), new_value);
  }
  ~ScopedEnvironmentOverride() {
    ::SetEnvironmentVariableW(name_.c_str(),
                              old_value_ ? old_value_->c_str() : nullptr);
  }

 private:
  const std::wstring name_;
  std::optional<std::wstring> old_value_;
};

}  // namespace

class GetInstalledDirectoryTest : public testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(random_.CreateUniqueTempDir());
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
  }

  static bool is_system_level() { return GetParam(); }
  base::FilePath random_path() const { return random_.GetPath(); }

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
  base::ScopedTempDir random_;
  registry_util::RegistryOverrideManager registry_override_manager_;
};

TEST_P(GetInstalledDirectoryTest, NoRegistryValue) {
  EXPECT_EQ(GetInstalledDirectory(is_system_level()), base::FilePath());
}

TEST_P(GetInstalledDirectoryTest, RegistryValueSet) {
  const base::FilePath install_path =
      random_path()
          .Append(install_static::GetChromeInstallSubDirectory())
          .Append(kInstallBinaryDir);
  ASSERT_TRUE(base::CreateDirectory(install_path));
  base::win::RegKey client_state_key(GetClientStateRegKey());
  ASSERT_EQ(client_state_key.WriteValue(
                kUninstallStringField,
                install_path.AppendASCII("1.0.0.0\\Installer\\setup.exe")
                    .value()
                    .c_str()),
            ERROR_SUCCESS);

  base::win::RegKey client_key(GetClientsRegKey());
  ASSERT_EQ(client_key.WriteValue(google_update::kRegVersionField, L"1.0.0.0"),
            ERROR_SUCCESS);

  EXPECT_EQ(GetInstalledDirectory(is_system_level()), install_path);
}

TEST_P(GetInstalledDirectoryTest, NoDirectory) {
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
  EXPECT_EQ(GetInstalledDirectory(is_system_level()), base::FilePath());
}

TEST_P(GetInstalledDirectoryTest, ReferencesParent) {
  const base::FilePath install_path =
      random_path()
          .Append(install_static::GetChromeInstallSubDirectory())
          .Append(kInstallBinaryDir);
  ASSERT_TRUE(base::CreateDirectory(install_path));
  base::win::RegKey client_state_key(GetClientStateRegKey());
  ASSERT_EQ(client_state_key.WriteValue(
                kUninstallStringField,
                install_path
                    .AppendASCII("1.0.0.0\\Installer\\..\\Installer\\setup.exe")
                    .value()
                    .c_str()),
            ERROR_SUCCESS);

  base::win::RegKey client_key(GetClientsRegKey());
  ASSERT_EQ(client_key.WriteValue(google_update::kRegVersionField, L"1.0.0.0"),
            ERROR_SUCCESS);

  EXPECT_EQ(GetInstalledDirectory(is_system_level()), base::FilePath());
}

TEST_P(GetInstalledDirectoryTest, NotAbsolute) {
  base::win::RegKey client_state_key(GetClientStateRegKey());
  ASSERT_EQ(client_state_key.WriteValue(
                kUninstallStringField,
                base::FilePath(install_static::GetChromeInstallSubDirectory())
                    .Append(kInstallBinaryDir)
                    .AppendASCII("1.0.0.0\\Installer\\setup.exe")
                    .value()
                    .c_str()),
            ERROR_SUCCESS);

  base::win::RegKey client_key(GetClientsRegKey());
  ASSERT_EQ(client_key.WriteValue(google_update::kRegVersionField, L"1.0.0.0"),
            ERROR_SUCCESS);

  EXPECT_EQ(GetInstalledDirectory(is_system_level()), base::FilePath());
}

TEST_P(GetInstalledDirectoryTest, AtRoot) {
  base::win::RegKey client_state_key(GetClientStateRegKey());
  ASSERT_EQ(
      client_state_key.WriteValue(
          kUninstallStringField,
          base::FilePath(FILE_PATH_LITERAL("C:\\1.0.0.0\\Installer\\setup.exe"))
              .value()
              .c_str()),
      ERROR_SUCCESS);

  base::win::RegKey client_key(GetClientsRegKey());
  ASSERT_EQ(client_key.WriteValue(google_update::kRegVersionField, L"1.0.0.0"),
            ERROR_SUCCESS);

  EXPECT_EQ(GetInstalledDirectory(is_system_level()), base::FilePath());
}

TEST_P(GetInstalledDirectoryTest, RegistryValueSetWrongScope) {
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
  EXPECT_EQ(GetInstalledDirectory(!is_system_level()), base::FilePath());
}

TEST_P(GetInstalledDirectoryTest, RegistryValueSetNoProductVersion) {
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
  EXPECT_EQ(GetInstalledDirectory(is_system_level()), base::FilePath());
}

INSTANTIATE_TEST_SUITE_P(UserLevelTest,
                         GetInstalledDirectoryTest,
                         testing::Values(false));
INSTANTIATE_TEST_SUITE_P(SystemLevelTest,
                         GetInstalledDirectoryTest,
                         testing::Values(true));

class GetDefaultChromeInstallPathTest : public testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(program_files_.CreateUniqueTempDir());
    ASSERT_TRUE(program_files_x86_.CreateUniqueTempDir());
    ASSERT_TRUE(local_app_data_.CreateUniqueTempDir());
    program_files_override_.emplace(base::DIR_PROGRAM_FILES,
                                    program_files_path());
    program_files_x86_override_.emplace(base::DIR_PROGRAM_FILESX86,
                                        program_files_x86_path());
    local_data_app_override_.emplace(base::DIR_LOCAL_APP_DATA,
                                     local_app_data_path());
  }

  static bool is_system_level() { return GetParam(); }
  base::FilePath program_files_path() const { return program_files_.GetPath(); }
  base::FilePath program_files_x86_path() const {
    return program_files_x86_.GetPath();
  }
  base::FilePath local_app_data_path() const {
    return local_app_data_.GetPath();
  }

  base::FilePath GetExpectedPath(bool system_level) const {
    auto path = system_level ? program_files_path() : local_app_data_path();
    return path.Append(install_static::GetChromeInstallSubDirectory())
        .Append(kInstallBinaryDir);
  }

 private:
  base::ScopedTempDir program_files_;
  base::ScopedTempDir program_files_x86_;
  base::ScopedTempDir local_app_data_;
  std::optional<base::ScopedPathOverride> program_files_override_;
  std::optional<base::ScopedPathOverride> program_files_x86_override_;
  std::optional<base::ScopedPathOverride> local_data_app_override_;
};

// Tests that the PathService is used to get the default install path.
TEST_P(GetDefaultChromeInstallPathTest, PathService) {
  EXPECT_EQ(GetDefaultChromeInstallPath(is_system_level()),
            GetExpectedPath(is_system_level()));
}

// Tests that the environment variable fallback is used if the PathService
// returns a path that doesn't exist.
TEST_P(GetDefaultChromeInstallPathTest, EnvironmentFallback) {
  // Override the relevant paths with directories that do not exist so that the
  // env var fallback is reached.
  base::ScopedPathOverride bad_program_files_override(
      base::DIR_PROGRAM_FILES, program_files_path().AppendASCII("doesnotexist"),
      /*is_absolute=*/true, /*create=*/false);
  base::ScopedPathOverride bad_program_files_x86_override(
      base::DIR_PROGRAM_FILESX86,
      program_files_x86_path().AppendASCII("doesnotexist"),
      /*is_absolute=*/true, /*create=*/false);
  base::ScopedPathOverride bad_local_ap_data_override(
      base::DIR_LOCAL_APP_DATA,
      local_app_data_path().AppendASCII("doesnotexist"),
      /*is_absolute=*/true, /*create=*/false);

  ScopedEnvironmentOverride program_files_env(
      L"PROGRAMFILES", program_files_path().value().c_str());
  ScopedEnvironmentOverride program_files_x86_env(
      L"PROGRAMFILES(X86)", program_files_x86_path().value().c_str());
  ScopedEnvironmentOverride local_app_data_env(
      L"LOCALAPPDATA", local_app_data_path().value().c_str());

  EXPECT_EQ(GetDefaultChromeInstallPath(is_system_level()),
            GetExpectedPath(is_system_level()));
}

INSTANTIATE_TEST_SUITE_P(UserLevelTest,
                         GetDefaultChromeInstallPathTest,
                         testing::Values(false));
INSTANTIATE_TEST_SUITE_P(SystemLevelTest,
                         GetDefaultChromeInstallPathTest,
                         testing::Values(true));

struct Params {
  Params(bool system_level, std::optional<int> target_dir_key)
      : system_level(system_level), target_dir_key(target_dir_key) {}
  bool system_level;
  std::optional<int> target_dir_key;
};

// Tests GetChromeInstallPath with a params object that contains a boolean
// |system_level| which is |true| if the test must use system-level values or
// |false| it the test must use user-level values, and a |target_dir| path in
// which the installation should be made.
class GetChromeInstallPathWithPrefsTest
    : public testing::TestWithParam<Params> {
 protected:
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

 private:
  base::ScopedTempDir program_files_;
  base::ScopedTempDir program_files_x86_;
  base::ScopedTempDir random_;
  base::ScopedTempDir local_app_data_;
  registry_util::RegistryOverrideManager registry_override_manager_;
  std::optional<base::ScopedPathOverride> program_files_override_;
  std::optional<base::ScopedPathOverride> program_files_x86_override_;
  std::optional<base::ScopedPathOverride> local_data_app_override_;
};

TEST_P(GetChromeInstallPathWithPrefsTest, NoRegistryValue) {
  EXPECT_EQ(GetChromeInstallPathWithPrefs(is_system_level(),
                                          InitialPreferences(prefs_json())),
            GetExpectedPathForSetup(is_system_level(), target_dir()));
}

TEST_P(GetChromeInstallPathWithPrefsTest, RegistryValueSet) {
  const base::FilePath install_path =
      random_path()
          .Append(install_static::GetChromeInstallSubDirectory())
          .Append(kInstallBinaryDir);
  ASSERT_TRUE(base::CreateDirectory(install_path));
  base::win::RegKey client_state_key(GetClientStateRegKey());
  ASSERT_EQ(client_state_key.WriteValue(
                kUninstallStringField,
                install_path.AppendASCII("1.0.0.0\\Installer\\setup.exe")
                    .value()
                    .c_str()),
            ERROR_SUCCESS);

  base::win::RegKey client_key(GetClientsRegKey());
  ASSERT_EQ(client_key.WriteValue(google_update::kRegVersionField, L"1.0.0.0"),
            ERROR_SUCCESS);
  EXPECT_EQ(GetChromeInstallPathWithPrefs(is_system_level(),
                                          InitialPreferences(prefs_json())),
            install_path);
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
                         testing::Values<Params>(Params(false, std::nullopt)));

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
                         testing::Values<Params>(Params(true, std::nullopt)));

class FindInstallPathTest
    : public ::testing::TestWithParam<std::tuple<bool, int>> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(program_files_.CreateUniqueTempDir());
    ASSERT_TRUE(program_files_x86_.CreateUniqueTempDir());
    ASSERT_TRUE(program_files_6432_.CreateUniqueTempDir());
    ASSERT_TRUE(local_app_data_.CreateUniqueTempDir());
    program_files_override_.emplace(base::DIR_PROGRAM_FILES,
                                    program_files_path());
    program_files_x86_override_.emplace(base::DIR_PROGRAM_FILESX86,
                                        program_files_x86_path());
    program_files_6432_override_.emplace(base::DIR_PROGRAM_FILES6432,
                                         program_files_6432_path());
    local_data_app_override_.emplace(base::DIR_LOCAL_APP_DATA,
                                     local_app_data_path());
  }

  static bool is_system_level() { return std::get<0>(GetParam()); }
  static int target_path_key() { return std::get<1>(GetParam()); }
  base::FilePath program_files_path() const { return program_files_.GetPath(); }
  base::FilePath program_files_x86_path() const {
    return program_files_x86_.GetPath();
  }
  base::FilePath program_files_6432_path() const {
    return program_files_6432_.GetPath();
  }
  base::FilePath local_app_data_path() const {
    return local_app_data_.GetPath();
  }

 private:
  base::ScopedTempDir program_files_;
  base::ScopedTempDir program_files_x86_;
  base::ScopedTempDir program_files_6432_;
  base::ScopedTempDir local_app_data_;
  std::optional<base::ScopedPathOverride> program_files_override_;
  std::optional<base::ScopedPathOverride> program_files_x86_override_;
  std::optional<base::ScopedPathOverride> program_files_6432_override_;
  std::optional<base::ScopedPathOverride> local_data_app_override_;
};

// Tests that FindInstallPath returns an empty string when no install directory
// is present.
TEST_P(FindInstallPathTest, NoDirectory) {
  EXPECT_EQ(FindInstallPath(is_system_level(), base::Version("1.0.0.0")),
            base::FilePath());
}

// Tests that FindInstallPath returns the path to the installed version
// directory.
TEST_P(FindInstallPathTest, Installed) {
  const auto path = base::PathService::CheckedGet(target_path_key())
                        .Append(install_static::GetChromeInstallSubDirectory())
                        .Append(kInstallBinaryDir)
                        .Append(L"1.0.0.0");
  ASSERT_TRUE(base::CreateDirectoryAndGetError(path, nullptr));
  EXPECT_EQ(FindInstallPath(is_system_level(), base::Version("1.0.0.0")), path);
}

INSTANTIATE_TEST_SUITE_P(
    UserLevelTest,
    FindInstallPathTest,
    testing::Values(std::make_tuple(false, base::DIR_LOCAL_APP_DATA)));
INSTANTIATE_TEST_SUITE_P(
    SystemLevelTest,
    FindInstallPathTest,
    testing::Values(std::make_tuple(true, base::DIR_PROGRAM_FILES),
#if defined(ARCH_CPU_64_BITS)
                    std::make_tuple(true, base::DIR_PROGRAM_FILESX86)
#else
                    std::make_tuple(true, base::DIR_PROGRAM_FILES6432)
#endif

                        ));

}  // namespace installer
