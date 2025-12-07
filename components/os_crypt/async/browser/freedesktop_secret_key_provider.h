// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_BROWSER_FREEDESKTOP_SECRET_KEY_PROVIDER_H_
#define COMPONENTS_OS_CRYPT_ASYNC_BROWSER_FREEDESKTOP_SECRET_KEY_PROVIDER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "build/branding_buildflags.h"
#include "components/dbus/utils/call_method.h"
#include "components/dbus/utils/check_for_service_and_start.h"
#include "components/dbus/utils/connect_to_signal.h"
#include "components/dbus/utils/name_has_owner.h"
#include "components/os_crypt/async/browser/key_provider.h"
#include "dbus/bus.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace os_crypt_async {

// FreedesktopSecretKeyProvider uses the org.freedesktop.secrets interface
// to retrieve a secret from backend (GNOME Keyring, KWallet, KeePassXC),
// which can then be used to encrypt confidential data. If registration with
// D-Bus services fails, this class will fallback to using PosixKeyProvider.
class FreedesktopSecretKeyProvider : public KeyProvider {
 public:
  enum class InitStatus {
    // These values are persisted to logs. Do not renumber or reuse.
    kSuccess = 0,
    kCreateCollectionFailed = 1,
    kCreateItemFailed = 2,
    kEmptySecret = 3,
    kGetSecretFailed = 4,
    kGnomeKeyringDeadlock = 5,
    kNoService = 6,
    kReadAliasFailed = 7,
    kSearchItemsFailed = 8,
    kSessionFailure = 9,
    kUnlockFailed = 10,
    kDisabled = 11,
    kKWalletNoService = 12,
    kKWalletDisabled = 13,
    kKWalletNoNetworkWallet = 14,
    kKWalletOpenFailed = 15,
    kKWalletNoSecret = 16,
    kKWalletFolderCheckFailed = 17,
    kKWalletFolderCreationFailed = 18,
    kKWalletEntryCheckFailed = 19,
    kKWalletReadFailed = 20,
    kKWalletWriteFailed = 21,
    kMaxValue = kKWalletWriteFailed,
  };

  // Supplements InitStatus in case of errors.
  enum class ErrorDetail {
    // These values are persisted to logs. Do not renumber or reuse.
    kNone = 0,
    kDestructedBeforeComplete = 1,
    kEmptyObjectPaths = 2,
    kInvalidReplyFormat = 3,
    kInvalidSignalFormat = 4,
    kInvalidVariantFormat = 5,
    kNoResponse = 6,
    kPromptDismissed = 7,
    kPromptFailedSignalConnection = 8,
    kKWalletApiReturnedError = 9,
    kKWalletApiReturnedFalse = 10,
    kErrorResponse = 11,
    kExtraDataInResponse = 12,
    kMaxValue = kExtraDataInResponse,
  };

  FreedesktopSecretKeyProvider(const std::string& password_store,
                               const std::string& product_name,
                               scoped_refptr<dbus::Bus> bus);
  ~FreedesktopSecretKeyProvider() override;

  // KeyProvider:
  void GetKey(KeyCallback callback) override;
  bool UseForEncryption() override;
  bool IsCompatibleWithOsCryptSync() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(FreedesktopSecretKeyProviderTest, BasicHappyPath);
  FRIEND_TEST_ALL_PREFIXES(FreedesktopSecretKeyProviderTest,
                           CreateCollectionAndItemWithUnlockPrompt);
  FRIEND_TEST_ALL_PREFIXES(FreedesktopSecretKeyProviderTest, KWallet);
  FRIEND_TEST_ALL_PREFIXES(FreedesktopSecretKeyProviderTest,
                           KWalletCreateFolderAndPassword);
  friend class FreedesktopSecretKeyProviderCompatTest;

  template <typename T>
  class Prompter;

  static constexpr char kSecretServiceName[] = "org.freedesktop.secrets";
  static constexpr char kSecretServicePath[] = "/org/freedesktop/secrets";
  static constexpr char kSecretServiceInterface[] =
      "org.freedesktop.Secret.Service";
  static constexpr char kSecretCollectionInterface[] =
      "org.freedesktop.Secret.Collection";
  static constexpr char kSecretItemInterface[] = "org.freedesktop.Secret.Item";
  static constexpr char kSecretSessionInterface[] =
      "org.freedesktop.Secret.Session";
  static constexpr char kSecretPromptInterface[] =
      "org.freedesktop.Secret.Prompt";

  static constexpr char kMethodReadAlias[] = "ReadAlias";
  static constexpr char kMethodCreateCollection[] = "CreateCollection";
  static constexpr char kMethodGetSecret[] = "GetSecret";
  static constexpr char kMethodOpenSession[] = "OpenSession";
  static constexpr char kMethodCreateItem[] = "CreateItem";
  static constexpr char kMethodUnlock[] = "Unlock";
  static constexpr char kMethodClose[] = "Close";
  static constexpr char kMethodSearchItems[] = "SearchItems";
  static constexpr char kPropertiesInterface[] =
      "org.freedesktop.DBus.Properties";
  static constexpr char kMethodGet[] = "Get";
  static constexpr char kMethodPrompt[] = "Prompt";

  static constexpr char kKWalletInterface[] = "org.kde.KWallet";
  static constexpr char kKWalletMethodIsEnabled[] = "isEnabled";
  static constexpr char kKWalletMethodNetworkWallet[] = "networkWallet";
  static constexpr char kKWalletMethodOpenAsync[] = "openAsync";
  static constexpr char kKWalletMethodReadPassword[] = "readPassword";
  static constexpr char kKWalletMethodClose[] = "close";
  static constexpr char kKWalletMethodHasFolder[] = "hasFolder";
  static constexpr char kKWalletMethodCreateFolder[] = "createFolder";
  static constexpr char kKWalletMethodHasEntry[] = "hasEntry";
  static constexpr char kKWalletMethodWritePassword[] = "writePassword";

  static constexpr char kKWalletSignalWalletAsyncOpened[] = "walletAsyncOpened";

  static constexpr char kDefaultAlias[] = "default";

  // These constants are duplicated from the sync backend.
  static constexpr char kApplicationAttributeKey[] = "application";
  static constexpr char kSchemaAttributeKey[] = "xdg:schema";
  static constexpr char kSchemaAttributeValue[] =
      "chrome_libsecret_os_crypt_password_v2";

  static constexpr char kAlgorithmPlain[] = "plain";
  static constexpr char kInputPlain[] = "";
  static constexpr char kMimePlain[] = "text/plain";

  static constexpr char kSecretCollectionLabelProperty[] =
      "org.freedesktop.Secret.Collection.Label";
  static constexpr char kSecretItemAttributesProperty[] =
      "org.freedesktop.Secret.Item.Attributes";
  static constexpr char kSecretItemLabelProperty[] =
      "org.freedesktop.Secret.Item.Label";
  static constexpr char kDefaultCollectionLabel[] = "Default Keyring";
  static constexpr char kLabelProperty[] = "Label";

  static constexpr char kKWalletDService[] = "org.kde.kwalletd";
  static constexpr char kKWalletDPath[] = "/modules/kwalletd";
  static constexpr char kKWalletD5Service[] = "org.kde.kwalletd5";
  static constexpr char kKWalletD5Path[] = "/modules/kwalletd5";
  static constexpr char kKWalletD6Service[] = "org.kde.kwalletd6";
  static constexpr char kKWalletD6Path[] = "/modules/kwalletd6";

  static constexpr int kKWalletInvalidHandle = -1;
  static constexpr int kKWalletInvalidTransactionId = -1;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static constexpr char kKWalletFolder[] = "Chrome Keys";
  static constexpr char kKeyName[] = "Chrome Safe Storage";
  static constexpr char kAppName[] = "chrome";
#else
  static constexpr char kKWalletFolder[] = "Chromium Keys";
  static constexpr char kKeyName[] = "Chromium Safe Storage";
  static constexpr char kAppName[] = "chromium";
#endif

  void InitializeFreedesktopSecretService();
  void OnServiceStarted(std::optional<bool> service_started);
  void OnReadAliasDefault(dbus_utils::CallMethodResultSig<"o"> collection_path);
  void OnGetCollectionLabelResponse(
      dbus_utils::CallMethodResultSig<"v"> variant);
  void OnCreateCollection(
      base::expected<dbus::ObjectPath, ErrorDetail> create_collection_reply);
  void OnUnlock(base::expected<std::vector<dbus::ObjectPath>, ErrorDetail>
                    unlocked_collection);
  void OnOpenSession(dbus_utils::CallMethodResultSig<"vo"> session_reply);
  void OnSearchItems(dbus_utils::CallMethodResultSig<"ao"> results);
  void OnGetSecret(dbus_utils::CallMethodResultSig<"(oayays)"> secret_reply);

  // KWallet password storage
  void InitializeKWallet(const char* kwallet_service, const char* kwallet_path);
  void OnKWalletServiceStarted(std::optional<bool> has_owner);
  void OnKWalletIsEnabled(dbus_utils::CallMethodResultSig<"b"> is_enabled);
  void OnKWalletNetworkWallet(dbus_utils::CallMethodResultSig<"s"> wallet_name);
  void OnKWalletOpenAsync(dbus_utils::CallMethodResultSig<"i"> t_id);
  void OnKWalletWalletAsyncOpened(
      dbus_utils::ConnectToSignalResultSig<"ii"> result);
  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool connected);
  void OnKWalletOpen(int32_t handle);
  void OnKWalletHasFolder(dbus_utils::CallMethodResultSig<"b"> has_folder);
  void OnKWalletCreateFolder(dbus_utils::CallMethodResultSig<"b"> success);
  void OnKWalletHasEntry(dbus_utils::CallMethodResultSig<"b"> has_entry);
  void OnKWalletReadPassword(dbus_utils::CallMethodResultSig<"s"> secret_reply);
  void GenerateAndWriteKWalletPassword();
  void OnKWalletWritePassword(
      scoped_refptr<base::RefCountedMemory> generated_secret,
      dbus_utils::CallMethodResultSig<"i"> return_code);

  void UnlockDefaultCollection();
  void OpenSession();
  void CreateItem(scoped_refptr<base::RefCountedMemory> secret);
  void OnCreateItem(scoped_refptr<base::RefCountedMemory> secret,
                    base::expected<dbus::ObjectPath, ErrorDetail> created_item);
  void DeriveKeyFromSecret(base::span<const uint8_t> secret);
  void FinalizeSuccess(Encryptor::Key key);
  void FinalizeFailure(InitStatus status, ErrorDetail detail);
  void RecordInitStatus(InitStatus status, ErrorDetail detail);
  void CloseSession();

  raw_ptr<dbus::ObjectProxy> default_collection_proxy_ = nullptr;
  raw_ptr<dbus::ObjectProxy> session_proxy_ = nullptr;
  bool session_opened_ = false;

  // For KWallet password storage
  raw_ptr<dbus::ObjectProxy> kwallet_proxy_ = nullptr;
  int32_t kwallet_handle_ = kKWalletInvalidHandle;
  int32_t kwallet_transaction_id_ = kKWalletInvalidTransactionId;

  const std::string password_store_;
  const std::string product_name_;
  scoped_refptr<dbus::Bus> bus_;
  KeyCallback key_callback_;

  std::string secret_for_testing_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FreedesktopSecretKeyProvider> weak_ptr_factory_{this};
};

}  // namespace os_crypt_async

#endif  // COMPONENTS_OS_CRYPT_ASYNC_BROWSER_FREEDESKTOP_SECRET_KEY_PROVIDER_H_
