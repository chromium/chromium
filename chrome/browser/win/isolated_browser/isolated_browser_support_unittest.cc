// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/isolated_browser/isolated_browser_support.h"

#include <objbase.h>

#include <shlobj.h>
#include <wrl/client.h>

#include <optional>
#include <utility>

#include "base/base64.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_reg_util_win.h"
#include "base/test/test_timeouts.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "base/win/com_init_util.h"
#include "base/win/elevation_util.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/os_crypt/app_bound_encryption_provider_win.h"
#include "chrome/browser/os_crypt/app_bound_encryption_win.h"
#include "chrome/browser/os_crypt/test_support.h"
#include "chrome/browser/win/isolated_browser/isolated_browser_test_support.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/elevation_service/elevation_service_idl.h"
#include "chrome/elevation_service/elevator.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "components/crash/core/common/crash_key.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/prefs/mock_pref_change_callback.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
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
  // `prefs_` must be declared before `env_` so that it outlives any objects
  // torn down during `BrowserTaskEnvironment` destruction (such as providers
  // owned by `TestingBrowserProcess` that hold a `raw_ptr<PrefService>` to
  // `prefs_`).
  TestingPrefServiceSimple prefs_;
  content::BrowserTaskEnvironment env_;

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

TEST_F(IsolatedBrowserSupportSystemTest, NonDefaultUserDataDirPreventsWrites) {
  install_static::ScopedInstallDetails details(/*system_level=*/true);

  // Simulate a non-default user data directory.
  chrome::SetUsingDefaultUserDataDirectoryForTesting(false);

  base::test::TestFuture<base::expected<IsolationState, HRESULT>> future;
  SetIsolationState(IsolationState::kProcessIsolation, &prefs_,
                    future.GetCallback());
  const auto result = future.Take();

  // Non-default user data directories must be rejected with E_NOTIMPL and not
  // modify isolation state.
  EXPECT_THAT(result, base::test::ErrorIs(E_NOTIMPL));
  EXPECT_FALSE(IsIsolationEnabled());

  // Restore test override.
  chrome::SetUsingDefaultUserDataDirectoryForTesting(std::nullopt);
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

TEST_F(IsolatedBrowserSupportSystemTest, ScopedKeepAlive) {
  install_static::ScopedInstallDetails details(/*system_level=*/true);

  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::ISOLATION_STATE_CHANGE));
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsRestartAllowed());

  // 1. Enabling isolation: ScopedKeepAlive is registered with restart disabled.
  {
    base::test::TestFuture<base::expected<IsolationState, HRESULT>> future;
    SetIsolationState(IsolationState::kProcessIsolation, &prefs_,
                      future.GetCallback());

    EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
        KeepAliveOrigin::ISOLATION_STATE_CHANGE));
    EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsRestartAllowed());

    const auto result = future.Take();
    EXPECT_THAT(result, base::test::ValueIs(IsolationState::kProcessIsolation));

    EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
        KeepAliveOrigin::ISOLATION_STATE_CHANGE));
    EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsRestartAllowed());
  }

  // 2. Disabling isolation: ScopedKeepAlive remains registered until
  // completion.
  {
    base::test::TestFuture<base::expected<IsolationState, HRESULT>> future;
    SetIsolationState(IsolationState::kIsolationDisabled, &prefs_,
                      future.GetCallback());

    EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
        KeepAliveOrigin::ISOLATION_STATE_CHANGE));
    EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsRestartAllowed());

    const auto result = future.Take();
    EXPECT_THAT(result,
                base::test::ValueIs(IsolationState::kIsolationDisabled));

    EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
        KeepAliveOrigin::ISOLATION_STATE_CHANGE));
    EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsRestartAllowed());
  }
}

TEST_F(IsolatedBrowserSupportSystemTest, ShuttingDownAborts) {
  install_static::ScopedInstallDetails details(/*system_level=*/true);

  KeepAliveRegistry::GetInstance()->SetIsShuttingDown(true);
  absl::Cleanup reset_shutting_down = [] {
    KeepAliveRegistry::GetInstance()->SetIsShuttingDown(false);
  };

  base::test::TestFuture<base::expected<IsolationState, HRESULT>> future;
  SetIsolationState(IsolationState::kProcessIsolation, &prefs_,
                    future.GetCallback());

  const auto result = future.Take();
  EXPECT_THAT(result, base::test::ErrorIs(E_ABORT));
  EXPECT_FALSE(IsIsolationEnabled());
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::ISOLATION_STATE_CHANGE));
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

class IsolatedBrowserSupportConcurrentOSCryptTest
    : public IsolatedBrowserSupportSystemTest,
      public ::testing::WithParamInterface<bool> {};

TEST_P(IsolatedBrowserSupportConcurrentOSCryptTest,
       DisableWaitsForOSCryptAsyncInit) {
  const bool key_already_stored = GetParam();
  install_static::ScopedInstallDetails details(/*system_level=*/true);

  // Force key encryption to PROTECTION_PATH_VALIDATION_WITH_ISOLATION.
  base::test::ScopedFeatureList encrypt_to_isolate{
      os_crypt_async::features::kEncryptWithIsolatedState};

  ::testing::StrictMock<MockAppBoundEncryptionOverrides> mock_app_bound;
  ScopedOverridesForTesting overrides(mock_app_bound);

  ON_CALL(mock_app_bound, EncryptAppBoundString).WillByDefault(Encrypt);
  ON_CALL(mock_app_bound, DecryptAppBoundString)
      .WillByDefault([](auto&&... args) {
        return Decrypt(std::forward<decltype(args)>(args)...,
                       /*inject_reencrypt_failures=*/false);
      });
  ON_CALL(mock_app_bound, GetAppBoundEncryptionSupportLevel)
      .WillByDefault(::testing::Return(os_crypt::SupportLevel::kSupported));

  if (key_already_stored) {
    // Store an initial unisolated key in prefs.
    EXPECT_CALL(mock_app_bound, GetAppBoundEncryptionSupportLevel).Times(1);
    EXPECT_CALL(mock_app_bound, EncryptAppBoundString).Times(1);
    os_crypt_async::AppBoundEncryptionProviderWin initial_provider(
        &prefs_,
        /*force_protection_level=*/ProtectionLevel::PROTECTION_PATH_VALIDATION);
    base::test::TestFuture<
        const std::string&,
        base::expected<os_crypt_async::Encryptor::Key,
                       os_crypt_async::KeyProvider::KeyError>>
        future;
    initial_provider.GetKey(future.GetCallback());
    auto [_, key] = future.Take();
    ASSERT_TRUE(key.has_value());
    ASSERT_THAT(GetProtectionLevelFromPrefs(&prefs_),
                testing::Optional(ProtectionLevel::PROTECTION_PATH_VALIDATION));
    ::testing::Mock::VerifyAndClearExpectations(&mock_app_bound);
  }

  // Enable isolation.
  {
    base::test::TestFuture<base::expected<IsolationState, HRESULT>> future;
    SetIsolationState(IsolationState::kProcessIsolation, &prefs_,
                      future.GetCallback());
    ASSERT_THAT(future.Take(),
                base::test::ValueIs(IsolationState::kProcessIsolation));
    ASSERT_TRUE(IsIsolationEnabled());
  }

  // Construct an OSCryptAsync with the normal AppBoundEncryptionProviderWin.
  EXPECT_CALL(mock_app_bound, GetAppBoundEncryptionSupportLevel).Times(2);
  if (key_already_stored) {
    // First call is from the normal provider in OSCryptAsync upgrading the key
    // to PROTECTION_PATH_VALIDATION_WITH_ISOLATION; second call is from
    // SetIsolationState downgrading it back to PROTECTION_PATH_VALIDATION.
    ::testing::InSequence seq;
    EXPECT_CALL(mock_app_bound,
                DecryptAppBoundString(
                    ::testing::_, ::testing::_,
                    ProtectionLevel::PROTECTION_PATH_VALIDATION_WITH_ISOLATION,
                    ::testing::_, ::testing::_, ::testing::_))
        .WillOnce([](const std::string& ciphertext, std::string& plaintext,
                     ProtectionLevel protection_level,
                     std::optional<std::string>& new_ciphertext,
                     DWORD& last_error,
                     elevation_service::EncryptFlags* flags) {
          elevation_service::EncryptFlags upgrade_flags{.force_reencrypt =
                                                            true};
          return Decrypt(ciphertext, plaintext, protection_level,
                         new_ciphertext, last_error, &upgrade_flags,
                         /*inject_reencrypt_failures=*/false);
        });
    // The test `Encrypt` implementation stores the data in plaintext with the
    // first byte indicating the `ProtectionLevel` (see `Encrypt` above), so
    // `StartsWith` verifies that the downgrade call is decrypting the key after
    // it was upgraded to `PROTECTION_PATH_VALIDATION_WITH_ISOLATION`.
    EXPECT_CALL(
        mock_app_bound,
        DecryptAppBoundString(
            ::testing::StartsWith(std::string(
                1, ProtectionLevel::PROTECTION_PATH_VALIDATION_WITH_ISOLATION)),
            ::testing::_, ProtectionLevel::PROTECTION_PATH_VALIDATION,
            ::testing::_, ::testing::_, ::testing::_));
  } else {
    // First call is EncryptAppBoundString from the normal provider generating a
    // new isolated key; second call is DecryptAppBoundString from
    // SetIsolationState downgrading that newly generated key.
    ::testing::InSequence seq;
    EXPECT_CALL(mock_app_bound,
                EncryptAppBoundString(
                    ProtectionLevel::PROTECTION_PATH_VALIDATION_WITH_ISOLATION,
                    ::testing::_, ::testing::_, ::testing::_, ::testing::_));
    // As above, the first byte of the test ciphertext indicates the
    // `ProtectionLevel` of the stored key being downgraded.
    EXPECT_CALL(
        mock_app_bound,
        DecryptAppBoundString(
            ::testing::StartsWith(std::string(
                1, ProtectionLevel::PROTECTION_PATH_VALIDATION_WITH_ISOLATION)),
            ::testing::_, ProtectionLevel::PROTECTION_PATH_VALIDATION,
            ::testing::_, ::testing::_, ::testing::_));
  }

  g_browser_process->set_additional_os_crypt_async_provider_for_test(
      /*precedence=*/15u,
      std::make_unique<os_crypt_async::AppBoundEncryptionProviderWin>(
          &prefs_, /*force_protection_level=*/std::nullopt));

  // Kick off async initialization of OSCryptAsync and immediately call
  // SetIsolationState(kIsolationDisabled) while initialization is in flight.
  base::test::TestFuture<scoped_refptr<os_crypt_async::Encryptor>>
      encryptor_future;
  g_browser_process->os_crypt_async()->GetInstance(
      encryptor_future.GetCallback());

  base::test::TestFuture<base::expected<IsolationState, HRESULT>>
      disable_future;
  SetIsolationState(IsolationState::kIsolationDisabled, &prefs_,
                    disable_future.GetCallback());

  EXPECT_THAT(disable_future.Take(),
              base::test::ValueIs(IsolationState::kIsolationDisabled));
  EXPECT_TRUE(encryptor_future.IsReady());
  EXPECT_FALSE(IsIsolationEnabled());
  EXPECT_THAT(GetProtectionLevelFromPrefs(&prefs_),
              testing::Optional(ProtectionLevel::PROTECTION_PATH_VALIDATION));
}

INSTANTIATE_TEST_SUITE_P(,
                         IsolatedBrowserSupportConcurrentOSCryptTest,
                         ::testing::Bool());

class IsolatedBrowserSupportLaunchTest : public ::testing::Test {
 protected:
  void SetUp() override {
    env_ = std::make_unique<chrome::IsolatedBrowserTestEnvironment>();
    if (!env_->is_valid()) {
      GTEST_SKIP() << "Test requires admin rights";
    }
  }

  std::unique_ptr<chrome::IsolatedBrowserTestEnvironment> env_;
};

base::expected<base::Process, DWORD> RunInChildDeElevated(
    std::string_view function_name) {
  base::CommandLine command_line =
      base::GetMultiProcessTestChildBaseCommandLine();
  command_line.AppendSwitchASCII(switches::kTestChildProcess, function_name);

  return base::win::RunDeElevated(command_line);
}

TEST_F(IsolatedBrowserSupportLaunchTest, RunIsolatedChromeLaunchUnisolated) {
  auto child_or_error =
      RunInChildDeElevated("RunIsolatedChromeLaunchUnisolatedInChild");

  if (!child_or_error.has_value()) {
    GTEST_SKIP() << "Cannot de-elevate when UAC is disabled.";
  }
  int exit_code;
  ASSERT_TRUE(child_or_error->WaitForExit(&exit_code));
  ASSERT_EQ(exit_code, 0);
}

TEST_F(IsolatedBrowserSupportLaunchTest, LaunchFailure) {
  // Configure `FakeInstallDetails` so that `install_static::GetElevatorClsid()`
  // and `install_static::GetElevatorIid()` inside
  // `IsolatedBrowserProcess::Launch()` resolve to the test elevation service
  // registered by `IsolatedBrowserTestEnvironment`.
  install_static::ScopedInstallDetails install_details(
      std::make_unique<os_crypt::FakeInstallDetails>());

  // Passing a non-existent executable causes `CoCreateInstance`,
  // `CreateJobObjectW`, and `AssignProcessToJobObject` to succeed in
  // `IsolatedBrowserProcess::Launch()`, and then causes `RunIsolatedChrome` to
  // fail inside the elevation service IPC call. Verify that `Launch()` returns
  // the service error and that destroying the scoped job handle on the failure
  // return path does not terminate the calling process.
  base::CommandLine cmd(
      base::FilePath(FILE_PATH_LITERAL("invalid_process_name.exe")));
  EXPECT_THAT(chrome::IsolatedBrowserProcess::Launch(cmd),
              base::test::ErrorIs(
                  elevation_service::Elevator::kErrorCouldNotLaunchBrowser));
}

TEST_F(IsolatedBrowserSupportLaunchTest, CheckIsolatedCommandline) {
  base::Process process = chrome::SpawnIsolatedMultiProcessTestChild(
      "CheckIsolatedCommandlineInChild");
  ASSERT_TRUE(process.IsValid());
  int exit_code = -1;
  ASSERT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_EQ(exit_code, 0);
}

MULTIPROCESS_TEST_MAIN(CheckIsolatedCommandlineInChild) {
  const auto* cmd_line = base::CommandLine::ForCurrentProcess();
  // Only verify the command line here has passed via elevation service, as the
  // security attribute is not present for Chromium builds.
  if (cmd_line->HasSwitch(::switches::kIsolated)) {
    return 0;
  }
  return 1;
}

TEST_F(IsolatedBrowserSupportLaunchTest, CheckIsolatedCrashKey) {
  base::Process process = chrome::SpawnIsolatedMultiProcessTestChild(
      "CheckIsolatedCrashKeyInChild");
  ASSERT_TRUE(process.IsValid());
  int exit_code = -1;
  ASSERT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_EQ(exit_code, 0);
}

MULTIPROCESS_TEST_MAIN(CheckIsolatedCrashKeyInChild) {
  crash_reporter::InitializeCrashKeys();
  crash_keys::SetCrashKeysFromCommandLine(
      *base::CommandLine::ForCurrentProcess());

  // Only Chrome branded builds actually set the security attribute.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (crash_reporter::GetCrashKeyValue("is-isolated") == "yes") {
    return 0;
  }
#else
  if (crash_reporter::GetCrashKeyValue("is-isolated").empty()) {
    return 0;
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return 1;
}

MULTIPROCESS_TEST_MAIN(RunIsolatedChromeLaunchUnisolatedInChild) {
  const std::string event_name =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  base::CommandLine cmd = base::GetMultiProcessTestChildBaseCommandLine();
  cmd.AppendArg(event_name);

  base::WaitableEvent event(base::win::ScopedHandle(::CreateEventA(
      /*lpEventAttributes=*/nullptr, /*bManualReset=*/FALSE,
      /*bInitialState=*/FALSE, /*lpName=*/event_name.c_str())));

  base::Process process = chrome::SpawnIsolatedMultiProcessTestChild(
      "RunIsolatedChromeLaunchUnisolatedInGrandchild", cmd);
  if (!process.IsValid()) {
    return 1;
  }

  HANDLE handles[] = {event.handle(), process.Handle()};
  DWORD wait_res = ::WaitForMultipleObjects(2, handles, FALSE, INFINITE);
  if (wait_res == WAIT_OBJECT_0 + 1) {
    int exit_code = 0;
    process.WaitForExit(&exit_code);
    LOG(ERROR) << "Child process exited prematurely with exit code: "
               << exit_code;
    ADD_FAILURE() << "Child process exited with code " << exit_code;
  } else {
    EXPECT_EQ(WAIT_OBJECT_0, wait_res);
  }

  EXPECT_TRUE(process.IsRunning());
  EXPECT_TRUE(process.Terminate(0, /*wait=*/true));

  return ::testing::Test::HasFailure() ? 1 : 0;
}

MULTIPROCESS_TEST_MAIN(RunIsolatedChromeLaunchUnisolatedInGrandchild) {
  base::CommandLine cmd = base::GetMultiProcessTestChildBaseCommandLine();
  cmd.AppendSwitchASCII(switches::kTestChildProcess,
                        "RunIsolatedChromeLaunchUnisolatedInGreatGrandchild");
  base::LaunchOptions options;
  options.wait = true;
  const auto unisolated_token = chrome::GetUnisolatedAccessToken();
  if (!unisolated_token.has_value()) {
    return -103;
  }
  options.as_user = unisolated_token->get();
  base::Process process = base::LaunchProcess(cmd, options);
  if (!process.IsValid()) {
    return -104;
  }
  int exit_code = -1;
  if (!process.WaitForExit(&exit_code)) {
    return -105;
  }
  if (exit_code != 0) {
    return exit_code;
  }

  const auto* cmd_line = base::CommandLine::ForCurrentProcess();
  const auto args = cmd_line->GetArgs();
  if (args.size() > 0) {
    ::SetEvent(::OpenEventW(EVENT_MODIFY_STATE, /*bInheritHandle=*/FALSE,
                            args[0].c_str()));
  }
  base::PlatformThread::Sleep(TestTimeouts::action_timeout());
  return 0;
}

MULTIPROCESS_TEST_MAIN(RunIsolatedChromeLaunchUnisolatedInGreatGrandchild) {
  if (chrome::IsRunningIsolated()) {
    return -102;
  }
  return 0;
}

constexpr char kStubReadyEventSwitch[] = "stub-ready-event";
constexpr char kGrandchildStartedEventSwitch[] = "grandchild-started-event";
constexpr char kStubCheckedJobsEventSwitch[] = "stub-checked-jobs-event";
constexpr char kPingGrandchildEventSwitch[] = "ping-grandchild-event";
constexpr char kPongGrandchildEventSwitch[] = "pong-grandchild-event";
constexpr char kGrandchildPidFileSwitch[] = "grandchild-pid-file";
constexpr int kExpectedIsolatedBrowserExitCode = 42;

// Verifies that `IsolatedBrowserProcess::Launch()` places the isolated browser
// process inside the stub's Job Object while configuring
// `JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK` so child processes spawned by the
// isolated browser automatically break away from the stub's Job Object:
// 1. Querying `::IsProcessInJob` against the stub's explicit Job Object handle
//    confirms that both the stub and the isolated browser ARE in the job, while
//    the child process spawned by the isolated browser is NOT in the job.
// 2. `IsolatedBrowserProcess::WaitForExit()` returns the isolated browser's
//    exit code as soon as the isolated browser exits, without blocking on a
//    surviving child process spawned by the isolated browser.
// 3. When the stub process subsequently exits (closing its Job Object handle
//    and triggering `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`), the surviving child
//    process is not terminated and continues running until explicitly signaled.
TEST_F(IsolatedBrowserSupportLaunchTest, ChildProcessesBreakAwayFromJob) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath grandchild_pid_file =
      temp_dir.GetPath().AppendASCII("grandchild.pid");

  const std::string stub_ready_name =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string grandchild_started_name =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string stub_checked_jobs_name =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string ping_grandchild_name =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string pong_grandchild_name =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  base::win::ScopedHandle stub_ready_handle(::CreateEventA(
      /*lpEventAttributes=*/nullptr, /*bManualReset=*/TRUE,
      /*bInitialState=*/FALSE, /*lpName=*/stub_ready_name.c_str()));
  ASSERT_TRUE(stub_ready_handle.is_valid());
  base::WaitableEvent stub_ready(std::move(stub_ready_handle));

  base::win::ScopedHandle grandchild_started_handle(::CreateEventA(
      /*lpEventAttributes=*/nullptr, /*bManualReset=*/TRUE,
      /*bInitialState=*/FALSE, /*lpName=*/grandchild_started_name.c_str()));
  ASSERT_TRUE(grandchild_started_handle.is_valid());
  base::WaitableEvent grandchild_started(std::move(grandchild_started_handle));

  base::win::ScopedHandle stub_checked_jobs_handle(::CreateEventA(
      /*lpEventAttributes=*/nullptr, /*bManualReset=*/TRUE,
      /*bInitialState=*/FALSE, /*lpName=*/stub_checked_jobs_name.c_str()));
  ASSERT_TRUE(stub_checked_jobs_handle.is_valid());
  base::WaitableEvent stub_checked_jobs(std::move(stub_checked_jobs_handle));

  base::win::ScopedHandle ping_grandchild_handle(::CreateEventA(
      /*lpEventAttributes=*/nullptr, /*bManualReset=*/TRUE,
      /*bInitialState=*/FALSE, /*lpName=*/ping_grandchild_name.c_str()));
  ASSERT_TRUE(ping_grandchild_handle.is_valid());
  base::WaitableEvent ping_grandchild(std::move(ping_grandchild_handle));

  // Ensure waiting child processes are always unblocked on scope exit even if
  // an assertion fails early.
  absl::Cleanup unblock_children = [&] {
    stub_checked_jobs.Signal();
    ping_grandchild.Signal();
  };

  base::win::ScopedHandle pong_grandchild_handle(::CreateEventA(
      /*lpEventAttributes=*/nullptr, /*bManualReset=*/TRUE,
      /*bInitialState=*/FALSE, /*lpName=*/pong_grandchild_name.c_str()));
  ASSERT_TRUE(pong_grandchild_handle.is_valid());
  base::WaitableEvent pong_grandchild(std::move(pong_grandchild_handle));

  base::CommandLine stub_cmd = base::GetMultiProcessTestChildBaseCommandLine();
  stub_cmd.RemoveSwitch(switches::kTestChildProcess);
  stub_cmd.AppendSwitchASCII(kStubReadyEventSwitch, stub_ready_name);
  stub_cmd.AppendSwitchASCII(kGrandchildStartedEventSwitch,
                             grandchild_started_name);
  stub_cmd.AppendSwitchASCII(kStubCheckedJobsEventSwitch,
                             stub_checked_jobs_name);
  stub_cmd.AppendSwitchASCII(kPingGrandchildEventSwitch, ping_grandchild_name);
  stub_cmd.AppendSwitchASCII(kPongGrandchildEventSwitch, pong_grandchild_name);
  stub_cmd.AppendSwitchPath(kGrandchildPidFileSwitch, grandchild_pid_file);

  base::LaunchOptions options;
  options.start_hidden = true;
  base::Process stub_process = base::SpawnMultiProcessTestChild(
      "JobBreakawayStubProcess", stub_cmd, options);
  ASSERT_TRUE(stub_process.IsValid());

  // Wait for the grandchild (spawned by the isolated browser) to confirm it is
  // running before the isolated browser and stub exit.
  ASSERT_TRUE(grandchild_started.TimedWait(TestTimeouts::action_timeout()));

  // Inside `JobBreakawayStubProcess`, the stub directly queries
  // `::IsProcessInJob` against its explicit Job Object handle for both the
  // isolated browser (`true`) and the grandchild (`false`), then signals the
  // isolated browser to exit with `kExpectedIsolatedBrowserExitCode` while the
  // grandchild remains blocked on `ping_grandchild`. Verify that all job
  // membership checks succeeded and `WaitForExit()` returned promptly.
  int stub_exit_code = -1;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      stub_process, TestTimeouts::action_timeout(), &stub_exit_code));
  EXPECT_EQ(stub_exit_code, kExpectedIsolatedBrowserExitCode);

  // Both the isolated browser and the stub process have now exited, closing the
  // stub's Job Object (`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`). Verify that the
  // grandchild process survived job closure and responds to `ping_grandchild`.
  ping_grandchild.Signal();
  EXPECT_TRUE(pong_grandchild.TimedWait(TestTimeouts::action_timeout()));
}

MULTIPROCESS_TEST_MAIN(JobBreakawayStubProcess) {
  install_static::ScopedInstallDetails install_details(
      std::make_unique<os_crypt::FakeInstallDetails>());

  const auto* cmd_line = base::CommandLine::ForCurrentProcess();
  base::win::ScopedHandle stub_ready_handle(::OpenEventA(
      EVENT_MODIFY_STATE, /*bInheritHandle=*/FALSE,
      cmd_line->GetSwitchValueASCII(kStubReadyEventSwitch).c_str()));
  if (!stub_ready_handle.is_valid()) {
    return -101;
  }
  base::WaitableEvent stub_ready(std::move(stub_ready_handle));

  base::win::ScopedHandle grandchild_started_handle(::OpenEventA(
      SYNCHRONIZE, /*bInheritHandle=*/FALSE,
      cmd_line->GetSwitchValueASCII(kGrandchildStartedEventSwitch).c_str()));
  if (!grandchild_started_handle.is_valid()) {
    return -102;
  }
  base::WaitableEvent grandchild_started(std::move(grandchild_started_handle));

  base::win::ScopedHandle stub_checked_jobs_handle(::OpenEventA(
      EVENT_MODIFY_STATE, /*bInheritHandle=*/FALSE,
      cmd_line->GetSwitchValueASCII(kStubCheckedJobsEventSwitch).c_str()));
  if (!stub_checked_jobs_handle.is_valid()) {
    return -103;
  }
  base::WaitableEvent stub_checked_jobs(std::move(stub_checked_jobs_handle));

  base::CommandLine isolated_cmd =
      base::GetMultiProcessTestChildBaseCommandLine();
  isolated_cmd.RemoveSwitch(switches::kTestChildProcess);
  isolated_cmd.AppendSwitchASCII(switches::kTestChildProcess,
                                 "JobBreakawayIsolatedBrowserProcess");

  auto isolated_process = chrome::IsolatedBrowserProcess::Launch(isolated_cmd);
  if (!isolated_process.has_value()) {
    return -104;
  }

  // Verify via `::IsProcessInJob` against the explicit stub Job Object handle
  // that both the stub itself and the newly launched isolated browser process
  // ARE members of the stub's Job Object.
  BOOL stub_in_job = FALSE;
  if (::IsProcessInJob(::GetCurrentProcess(),
                       isolated_process->job_for_testing(),
                       &stub_in_job) != TRUE ||
      stub_in_job != TRUE) {
    return -105;
  }
  BOOL browser_in_job = FALSE;
  if (::IsProcessInJob(isolated_process->process_for_testing().Handle(),
                       isolated_process->job_for_testing(),
                       &browser_in_job) != TRUE ||
      browser_in_job != TRUE) {
    return -106;
  }

  // Signal after `Launch()` has configured
  // `JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK` on the stub's Job Object so the
  // isolated browser spawns its child deterministically after the flag is set.
  stub_ready.Signal();

  // Wait for the grandchild to write its PID and confirm it is running.
  if (!grandchild_started.TimedWait(TestTimeouts::action_timeout())) {
    return -107;
  }

  std::string pid_str;
  if (!base::ReadFileToString(
          cmd_line->GetSwitchValuePath(kGrandchildPidFileSwitch), &pid_str)) {
    return -108;
  }
  unsigned int grandchild_pid = 0;
  if (!base::StringToUint(pid_str, &grandchild_pid) || grandchild_pid == 0) {
    return -109;
  }

  base::Process grandchild = base::Process::OpenWithAccess(
      static_cast<base::ProcessId>(grandchild_pid),
      PROCESS_QUERY_LIMITED_INFORMATION);
  if (!grandchild.IsValid()) {
    return -110;
  }

  // Verify directly via `::IsProcessInJob` against the explicit stub Job Object
  // handle that the child spawned by the isolated browser is NOT in the stub's
  // Job Object.
  BOOL grandchild_in_job = TRUE;
  if (::IsProcessInJob(grandchild.Handle(), isolated_process->job_for_testing(),
                       &grandchild_in_job) != TRUE ||
      grandchild_in_job != FALSE) {
    return -111;
  }
  grandchild.Close();

  // Allow the isolated browser process to exit while the grandchild stays
  // running.
  stub_checked_jobs.Signal();

  auto exit_code = isolated_process->WaitForExit();
  if (!exit_code.has_value()) {
    return -112;
  }

  return *exit_code;
}

MULTIPROCESS_TEST_MAIN(JobBreakawayIsolatedBrowserProcess) {
  const auto* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(::switches::kIsolated)) {
    return -201;
  }

  base::win::ScopedHandle stub_ready_handle(::OpenEventA(
      SYNCHRONIZE, /*bInheritHandle=*/FALSE,
      cmd_line->GetSwitchValueASCII(kStubReadyEventSwitch).c_str()));
  if (!stub_ready_handle.is_valid()) {
    return -202;
  }
  base::WaitableEvent stub_ready(std::move(stub_ready_handle));
  if (!stub_ready.TimedWait(TestTimeouts::action_timeout())) {
    return -203;
  }

  base::win::ScopedHandle stub_checked_jobs_handle(::OpenEventA(
      SYNCHRONIZE, /*bInheritHandle=*/FALSE,
      cmd_line->GetSwitchValueASCII(kStubCheckedJobsEventSwitch).c_str()));
  if (!stub_checked_jobs_handle.is_valid()) {
    return -204;
  }
  base::WaitableEvent stub_checked_jobs(std::move(stub_checked_jobs_handle));

  // Spawn a child process using default `LaunchOptions` (without
  // `force_breakaway_from_job_`), matching how external applications and
  // downloads are launched without explicit breakaway flags.
  base::CommandLine grandchild_cmd =
      base::GetMultiProcessTestChildBaseCommandLine();
  grandchild_cmd.RemoveSwitch(::switches::kIsolated);
  grandchild_cmd.RemoveSwitch(switches::kTestChildProcess);
  base::LaunchOptions options;
  options.start_hidden = true;
  base::Process grandchild = base::SpawnMultiProcessTestChild(
      "JobBreakawayGrandchildProcess", grandchild_cmd, options);
  if (!grandchild.IsValid()) {
    return -205;
  }

  // Stay alive until the stub has queried `::IsProcessInJob` on both this
  // isolated browser process and `grandchild`, then exit while `grandchild`
  // remains running.
  if (!stub_checked_jobs.TimedWait(TestTimeouts::action_timeout())) {
    return -206;
  }

  return kExpectedIsolatedBrowserExitCode;
}

MULTIPROCESS_TEST_MAIN(JobBreakawayGrandchildProcess) {
  const auto* cmd_line = base::CommandLine::ForCurrentProcess();

  base::win::ScopedHandle grandchild_started_handle(::OpenEventA(
      EVENT_MODIFY_STATE, /*bInheritHandle=*/FALSE,
      cmd_line->GetSwitchValueASCII(kGrandchildStartedEventSwitch).c_str()));
  if (!grandchild_started_handle.is_valid()) {
    return -301;
  }
  base::WaitableEvent grandchild_started(std::move(grandchild_started_handle));

  base::win::ScopedHandle ping_grandchild_handle(::OpenEventA(
      SYNCHRONIZE, /*bInheritHandle=*/FALSE,
      cmd_line->GetSwitchValueASCII(kPingGrandchildEventSwitch).c_str()));
  if (!ping_grandchild_handle.is_valid()) {
    return -302;
  }
  base::WaitableEvent ping_grandchild(std::move(ping_grandchild_handle));

  base::win::ScopedHandle pong_grandchild_handle(::OpenEventA(
      EVENT_MODIFY_STATE, /*bInheritHandle=*/FALSE,
      cmd_line->GetSwitchValueASCII(kPongGrandchildEventSwitch).c_str()));
  if (!pong_grandchild_handle.is_valid()) {
    return -303;
  }
  base::WaitableEvent pong_grandchild(std::move(pong_grandchild_handle));

  // Publish this grandchild's PID before signaling `grandchild_started` so the
  // stub can open a query handle and pass it to `::IsProcessInJob`.
  if (!base::WriteFile(cmd_line->GetSwitchValuePath(kGrandchildPidFileSwitch),
                       base::NumberToString(::GetCurrentProcessId()))) {
    return -304;
  }

  grandchild_started.Signal();

  if (!ping_grandchild.TimedWait(TestTimeouts::action_timeout())) {
    return -305;
  }

  pong_grandchild.Signal();
  return 0;
}

}  // namespace chrome
