// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/isolated_browser_support.h"

#include <objbase.h>

#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_reg_util_win.h"
#include "base/uuid.h"
#include "chrome/browser/os_crypt/app_bound_encryption_provider_win.h"
#include "chrome/browser/os_crypt/app_bound_encryption_win.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "components/prefs/mock_pref_change_callback.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace chrome {

namespace {

class MockAppBoundEncryptionOverrides
    : public os_crypt::AppBoundEncryptionOverridesForTesting {
 public:
  MOCK_METHOD(HRESULT,
              EncryptAppBoundString,
              (ProtectionLevel level,
               const std::string& plaintext,
               std::string& ciphertext,
               DWORD& last_error,
               elevation_service::EncryptFlags* flags),
              (override));

  MOCK_METHOD(HRESULT,
              DecryptAppBoundString,
              (const std::string& ciphertext,
               std::string& plaintext,
               ProtectionLevel protection_level,
               std::optional<std::string>& new_ciphertext,
               DWORD& last_error,
               elevation_service::EncryptFlags* flags),
              (override));

  MOCK_METHOD(os_crypt::SupportLevel,
              GetAppBoundEncryptionSupportLevel,
              (PrefService * local_state),
              (override));
};

constexpr std::string_view kPrefix("SECRET");
constexpr std::string_view kSuffix("DATA");

HRESULT Encrypt(ProtectionLevel level,
                const std::string& plaintext,
                std::string& ciphertext,
                DWORD& last_error,
                elevation_service::EncryptFlags* flags) {
  ciphertext = base::StrCat({kPrefix, plaintext, kSuffix});
  ciphertext.insert(ciphertext.cbegin(), level);
  last_error = 0;
  return S_OK;
}

HRESULT Decrypt(const std::string& ciphertext,
                std::string& plaintext,
                ProtectionLevel protection_level,
                std::optional<std::string>& new_ciphertext,
                DWORD& last_error,
                elevation_service::EncryptFlags* flags,
                bool inject_reencrypt_failures) {
  elevation_service::EncryptFlags default_flags;
  if (!flags) {
    flags = &default_flags;
  }

  // The header contains the protection level.
  constexpr size_t kHeaderLen = 1;

  if (ciphertext.size() <
      kHeaderLen + std::size(kPrefix) + std::size(kSuffix)) {
    last_error = ERROR_INVALID_DATA;
    return E_FAIL;
  }

  std::string_view plaintext_view =
      std::string_view(ciphertext)
          .substr(kHeaderLen + std::size(kPrefix),
                  ciphertext.size() - kHeaderLen - std::size(kPrefix) -
                      std::size(kSuffix));

  plaintext.assign(plaintext_view);

  if (flags->force_reencrypt) {
    if (inject_reencrypt_failures) {
      return E_FAIL;
    }
    std::string reencrypted_ciphertext;
    DWORD encrypt_last_error;
    HRESULT reencrypt_res =
        Encrypt(protection_level, plaintext, reencrypted_ciphertext,
                encrypt_last_error, /*EncryptFlags=*/nullptr);
    if (SUCCEEDED(reencrypt_res)) {
      new_ciphertext.emplace(reencrypted_ciphertext);
    }
  }

  last_error = 0;
  return S_OK;
}

class ScopedOverridesForTesting {
 public:
  explicit ScopedOverridesForTesting(
      os_crypt::AppBoundEncryptionOverridesForTesting& overrides) {
    os_crypt::SetOverridesForTesting(&overrides);
  }

  ~ScopedOverridesForTesting() { os_crypt::SetOverridesForTesting(nullptr); }
};

std::optional<ProtectionLevel> GetProtectionLevelFromPrefs(
    const PrefService* prefs) {
  const auto decoded = base::Base64Decode(
      prefs->GetString(os_crypt_async::kAppBoundEncryptedKeyPrefName));

  if (!decoded) {
    return std::nullopt;
  }

  constexpr size_t kMagicLen =
      std::size(os_crypt_async::kCryptAppBoundKeyPrefix);  // "APPB".

  if (decoded->size() <= kMagicLen) {
    return std::nullopt;
  }

  return static_cast<ProtectionLevel>(decoded->at(kMagicLen));
}

}  // namespace

class IsolatedBrowserSupportTestBase : public ::testing::Test {
 public:
  IsolatedBrowserSupportTestBase() {
    rom_.OverrideRegistry(HKEY_CURRENT_USER);
    os_crypt_async::AppBoundEncryptionProviderWin::RegisterLocalPrefs(
        prefs_.registry());
  }

 protected:
  content::BrowserTaskEnvironment env_;
  TestingPrefServiceSimple prefs_;

 private:
  registry_util::RegistryOverrideManager rom_;
};

TEST_F(IsolatedBrowserSupportTestBase, TestEncryptDecrypt) {
  std::string ciphertext;
  {
    DWORD last_error;
    EXPECT_HRESULT_SUCCEEDED(
        Encrypt(ProtectionLevel::PROTECTION_PATH_VALIDATION, "text", ciphertext,
                last_error, nullptr));
  }
  {
    DWORD last_error;
    std::string plaintext;
    std::optional<std::string> new_ciphertext;
    EXPECT_HRESULT_SUCCEEDED(Decrypt(
        ciphertext, plaintext, ProtectionLevel::PROTECTION_PATH_VALIDATION,
        new_ciphertext, last_error, /*flags=*/nullptr,
        /*inject_reencrypt_failures=*/false));
    EXPECT_EQ(plaintext, "text");
  }
}

class IsolatedBrowserSupportTest : public IsolatedBrowserSupportTestBase,
                                   public ::testing::WithParamInterface<bool> {
};

TEST_P(IsolatedBrowserSupportTest, SetState) {
  const auto IsSystemInstall = GetParam;

  install_static::ScopedInstallDetails details(
      /*system_level=*/IsSystemInstall());
  EXPECT_FALSE(IsIsolationEnabled());

  base::test::TestFuture<base::expected<IsolationState, HRESULT>> future;
  SetIsolationState(IsolationState::kProcessIsolation, &prefs_,
                    future.GetCallback());
  const auto result = future.Take();
  if (IsSystemInstall()) {
    EXPECT_THAT(result, base::test::ValueIs(IsolationState::kProcessIsolation));
    EXPECT_TRUE(IsIsolationEnabled());
  } else {
    EXPECT_THAT(result, base::test::ErrorIs(E_NOTIMPL));
    EXPECT_FALSE(IsIsolationEnabled());
  }
}

INSTANTIATE_TEST_SUITE_P(, IsolatedBrowserSupportTest, ::testing::Bool());

using IsolatedBrowserSupportSystemTest = IsolatedBrowserSupportTestBase;

TEST_F(IsolatedBrowserSupportSystemTest, CommandLine) {
  install_static::ScopedInstallDetails details(/*system_level=*/true);

  base::test::TestFuture<base::expected<IsolationState, HRESULT>> future;
  SetIsolationState(IsolationState::kProcessIsolation, &prefs_,
                    future.GetCallback());
  const auto result = future.Take();

  EXPECT_THAT(result, base::test::ValueIs(IsolationState::kProcessIsolation));
  EXPECT_TRUE(IsIsolationEnabled());

  // Verify isolation is never triggered if the --isolated switch is present.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(::switches::kIsolated);
  EXPECT_FALSE(IsIsolationEnabled(&command_line));
}

TEST_F(IsolatedBrowserSupportSystemTest, SetIsolationStateTwice) {
  install_static::ScopedInstallDetails details(/*system_level=*/true);

  for (size_t i = 0; i < 2; ++i) {
    base::test::TestFuture<base::expected<IsolationState, HRESULT>> future;
    SetIsolationState(IsolationState::kProcessIsolation, &prefs_,
                      future.GetCallback());
    const auto result = future.Take();

    EXPECT_THAT(result, base::test::ValueIs(IsolationState::kProcessIsolation));
    EXPECT_TRUE(IsIsolationEnabled());
  }

  for (size_t i = 0; i < 2; ++i) {
    base::test::TestFuture<base::expected<IsolationState, HRESULT>> future;
    SetIsolationState(IsolationState::kIsolationDisabled, &prefs_,
                      future.GetCallback());
    const auto result = future.Take();

    EXPECT_THAT(result,
                base::test::ValueIs(IsolationState::kIsolationDisabled));
    EXPECT_FALSE(IsIsolationEnabled());
  }
}

class IsolatedBrowserSupportSystemTestWithFailures
    : public IsolatedBrowserSupportSystemTest,
      public ::testing::WithParamInterface<bool> {};

TEST_P(IsolatedBrowserSupportSystemTestWithFailures, InjectFailures) {
  const auto AreFailuresInjected = GetParam;
  install_static::ScopedInstallDetails details(/*system_level=*/true);
  TestingPrefServiceSimple prefs;
  os_crypt_async::AppBoundEncryptionProviderWin::RegisterLocalPrefs(
      prefs.registry());

  // Force key encryption to PROTECTION_PATH_VALIDATION_WITH_ISOLATION.
  base::test::ScopedFeatureList encrypt_to_isolate{
      os_crypt_async::features::kEncryptWithIsolatedState};

  ::testing::StrictMock<MockAppBoundEncryptionOverrides> mock_app_bound;
  EXPECT_FALSE(IsIsolationEnabled());

  ScopedOverridesForTesting overrides(mock_app_bound);

  ON_CALL(mock_app_bound, EncryptAppBoundString).WillByDefault(Encrypt);
  ON_CALL(mock_app_bound, DecryptAppBoundString)
      .WillByDefault([](auto&&... args) {
        return Decrypt(std::forward<decltype(args)>(args)...,
                       /*inject_reencrypt_failures=*/false);
      });
  ON_CALL(mock_app_bound, GetAppBoundEncryptionSupportLevel)
      .WillByDefault(::testing::Return(os_crypt::SupportLevel::kSupported));

  EXPECT_CALL(mock_app_bound, GetAppBoundEncryptionSupportLevel).Times(1);
  EXPECT_CALL(mock_app_bound, EncryptAppBoundString).Times(1);
  {
    os_crypt_async::AppBoundEncryptionProviderWin provider(
        &prefs_, /*force_protection_level=*/std::nullopt);
    base::test::TestFuture<
        const std::string&,
        base::expected<os_crypt_async::Encryptor::Key,
                       os_crypt_async::KeyProvider::KeyError>>
        future;
    provider.GetKey(future.GetCallback());
    auto [_, key] = future.Take();
    ASSERT_TRUE(key.has_value());
    ASSERT_TRUE(provider.IsKeyStored());
    ASSERT_THAT(
        GetProtectionLevelFromPrefs(&prefs_),
        testing::Optional(
            ProtectionLevel::PROTECTION_PATH_VALIDATION_WITH_ISOLATION));
  }

  ::testing::Mock::VerifyAndClearExpectations(&mock_app_bound);

  {
    // Enable isolation. This does not persistent modifications to pref, as they
    // are done on next restart.
    base::test::TestFuture<base::expected<IsolationState, HRESULT>> future;
    SetIsolationState(IsolationState::kProcessIsolation, &prefs_,
                      future.GetCallback());
    const auto result = future.Take();
    EXPECT_THAT(result, base::test::ValueIs(IsolationState::kProcessIsolation));
    EXPECT_TRUE(IsIsolationEnabled());
  }

  {
    // Disable isolation. This should cause protection to downgrade to an
    // unisolated state.
    EXPECT_CALL(mock_app_bound, GetAppBoundEncryptionSupportLevel).Times(1);
    if (AreFailuresInjected()) {
      EXPECT_CALL(mock_app_bound, DecryptAppBoundString)
          .WillOnce([](auto&&... args) {
            return Decrypt(std::forward<decltype(args)>(args)...,
                           /*inject_reencrypt_failures=*/true);
          });
    } else {
      EXPECT_CALL(mock_app_bound, DecryptAppBoundString)
          .WillOnce([](auto&&... args) {
            return Decrypt(std::forward<decltype(args)>(args)...,
                           /*inject_reencrypt_failures=*/false);
          });
    }
    base::test::TestFuture<base::expected<IsolationState, HRESULT>> future;
    SetIsolationState(IsolationState::kIsolationDisabled, &prefs_,
                      future.GetCallback());
    const auto result = future.Take();
    if (!AreFailuresInjected()) {
      EXPECT_THAT(result,
                  base::test::ValueIs(IsolationState::kIsolationDisabled));
      EXPECT_FALSE(IsIsolationEnabled());
      ASSERT_THAT(
          GetProtectionLevelFromPrefs(&prefs_),
          testing::Optional(ProtectionLevel::PROTECTION_PATH_VALIDATION));
    } else {
      EXPECT_THAT(result, base::test::ErrorIs(E_FAIL));
      // Isolation never gets disabled if the protection can't be downgraded, to
      // avoid data loss.
      EXPECT_TRUE(IsIsolationEnabled());
      // Protection never downgraded.
      ASSERT_THAT(
          GetProtectionLevelFromPrefs(&prefs_),
          testing::Optional(
              ProtectionLevel::PROTECTION_PATH_VALIDATION_WITH_ISOLATION));
    }
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         IsolatedBrowserSupportSystemTestWithFailures,
                         ::testing::Bool());

}  // namespace chrome
