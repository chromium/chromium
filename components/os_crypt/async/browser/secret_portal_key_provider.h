// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_BROWSER_SECRET_PORTAL_KEY_PROVIDER_H_
#define COMPONENTS_OS_CRYPT_ASYNC_BROWSER_SECRET_PORTAL_KEY_PROVIDER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/scoped_file.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/os_crypt/async/browser/key_provider.h"

class CookieEncryptionProviderBrowserTest;
class PrefRegistrySimple;
class PrefService;

namespace dbus {
class Bus;
class ObjectPath;
class Response;
class Signal;
}  // namespace dbus

namespace os_crypt_async {

class SecretPortalKeyProviderTest;

// The SecretPortalKeyProvider uses the org.freedesktop.portal.Secret interface
// to retrieve an application-specific secret from the portal, which can then
// be used to encrypt confidential data. This is the only interface exposed
// from sandboxed environments like Flatpak.
class SecretPortalKeyProvider : public KeyProvider {
 public:
  enum class InitStatus {
    // Values are logged in metrics, so should not be changed.
    kSuccess = 0,
    kNoService = 1,
    kPipeFailed = 2,
    kInvalidResponseFormat = 3,
    kResponsePathMismatch = 4,
    kPipeReadFailed = 5,
    kEmptySecret = 6,
    kNoResponse = 7,
    kSignalReadFailed = 8,
    kUserCancelledUnlock = 9,
    // kDestructedBeforeComplete = 10,
    kSignalConnectFailed = 11,
    kSignalParseFailed = 12,
    kMaxValue = kSignalParseFailed,
  };

  static void RegisterLocalPrefs(PrefRegistrySimple* registry);

  SecretPortalKeyProvider(PrefService* local_state, bool use_for_encryption);

  ~SecretPortalKeyProvider() override;

 private:
  friend class ::CookieEncryptionProviderBrowserTest;
  friend class SecretPortalKeyProviderTest;
  friend class TestSecretPortal;
  FRIEND_TEST_ALL_PREFIXES(SecretPortalKeyProviderTest, GetKey);

  // DBus constants.  Exposed in header for tests.
  static constexpr char kServiceSecret[] = "org.freedesktop.portal.Desktop";
  static constexpr char kObjectPathSecret[] = "/org/freedesktop/portal/desktop";
  static constexpr char kInterfaceSecret[] = "org.freedesktop.portal.Secret";
  static constexpr char kInterfaceRequest[] = "org.freedesktop.portal.Request";
  static constexpr char kMethodRetrieveSecret[] = "RetrieveSecret";
  static constexpr char kSignalResponse[] = "Response";

  static constexpr char kOsCryptTokenPrefName[] = "os_crypt.portal.token";
  static constexpr char kOsCryptPrevDesktopPrefName[] =
      "os_crypt.portal.prev_desktop";
  static constexpr char kOsCryptPrevInitSuccessPrefName[] =
      "os_crypt.portal.prev_init_success";

  static constexpr char kHandleToken[] = "cr_secret_portal_response_token";

  static constexpr char kKeyTag[] = "v12";

  static constexpr char kUmaInitStatusEnum[] =
      "OSCrypt.SecretPortalKeyProvider.InitStatus";
  static constexpr char kUmaNewInitFailureEnum[] =
      "OSCrypt.SecretPortalKeyProvider.NewInitFailure";
  static constexpr char kUmaGotTokenBoolean[] =
      "OSCrypt.SecretPortalKeyProvider.GotToken";

  SecretPortalKeyProvider(PrefService* local_state,
                          scoped_refptr<dbus::Bus> bus,
                          bool use_for_encryption);

  // KeyProvider:
  void GetKey(KeyCallback callback) override;
  bool UseForEncryption() override;
  bool IsCompatibleWithOsCryptSync() override;

  void OnNameHasOwnerResponse(std::optional<bool> name_has_owner);

  void OnRetrieveSecretResponse(dbus::Response* response);

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool connected);

  void OnResponseSignal(dbus::Signal* signal);

  void OnFdReadable();

  void ReceivedSecret();

  // Finalize with an empty tag and key for error cases.
  void Finalize(InitStatus init_status);

  void Finalize(InitStatus init_status,
                const std::string& tag,
                std::optional<Encryptor::Key> key);

  static std::string GetSecretServiceName();

  static std::optional<std::string>& GetSecretServiceNameForTest();

  const raw_ptr<PrefService> local_state_;

  const bool use_for_encryption_;

  scoped_refptr<dbus::Bus> bus_;

  KeyCallback key_callback_;
  std::unique_ptr<dbus::ObjectPath> response_path_;
  base::ScopedFD read_fd_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> read_watcher_;
  std::vector<uint8_t> secret_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SecretPortalKeyProvider> weak_ptr_factory_{this};
};

}  // namespace os_crypt_async

#endif  // COMPONENTS_OS_CRYPT_ASYNC_BROWSER_SECRET_PORTAL_KEY_PROVIDER_H_
