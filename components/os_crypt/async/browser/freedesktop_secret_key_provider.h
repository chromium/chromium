// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_BROWSER_FREEDESKTOP_SECRET_KEY_PROVIDER_H_
#define COMPONENTS_OS_CRYPT_ASYNC_BROWSER_FREEDESKTOP_SECRET_KEY_PROVIDER_H_

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
#include "components/dbus/properties/types.h"
#include "components/dbus/utils/check_for_service_and_start.h"
#include "components/dbus/utils/name_has_owner.h"
#include "components/os_crypt/async/browser/key_provider.h"
#include "crypto/encryptor.h"
#include "dbus/bus.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace os_crypt_async {

// FreedesktopSecretKeyProvider uses the org.freedesktop.secrets interface
// to retrieve a secret from backend (GNOME Keyring, KWallet, KeePassXC),
// which can then be used to encrypt confidential data.
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
    kMaxValue = kUnlockFailed,
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
    kMaxValue = kPromptFailedSignalConnection,
  };

  FreedesktopSecretKeyProvider(bool use_for_encryption,
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
  FRIEND_TEST_ALL_PREFIXES(FreedesktopSecretKeyProviderTest,
                           MigrateFromKWallet);
  friend class FreedesktopSecretKeyProviderCompatTest;

  template <typename T>
  class Prompter;

  using DbusSecret = DbusStruct</*session=*/DbusObjectPath,
                                /*parameters=*/DbusByteArray,
                                /*value=*/DbusByteArray,
                                /*content_type=*/DbusString>;

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
  static constexpr char kKWalletMethodOpen[] = "open";
  static constexpr char kKWalletMethodReadPassword[] = "readPassword";
  static constexpr char kKWalletMethodClose[] = "close";

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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static constexpr char kKWalletFolder[] = "Chrome Keys";
  static constexpr char kKeyName[] = "Chrome Safe Storage";
  static constexpr char kAppName[] = "chrome";
#else
  static constexpr char kKWalletFolder[] = "Chromium Keys";
  static constexpr char kKeyName[] = "Chromium Safe Storage";
  static constexpr char kAppName[] = "chromium";
#endif

  struct KwalletServiceAndPath {
    const char* kwallet_service;
    const char* kwallet_path;
  };
  static constexpr auto kKWalletCandidates =
      std::to_array<KwalletServiceAndPath>({
          {"org.kde.kwalletd6", "/modules/kwalletd6"},
          {"org.kde.kwalletd5", "/modules/kwalletd5"},
          {"org.kde.kwalletd", "/modules/kwalletd"},
      });

  void OnServiceStarted(std::optional<bool> service_started);

  void OnReadAliasDefault(
      base::expected<DbusObjectPath, ErrorDetail> collection_path);
  void OnGetCollectionLabelResponse(
      base::expected<DbusVariant, ErrorDetail> variant);
  void OnCreateCollection(
      base::expected<DbusObjectPath, ErrorDetail> create_collection_reply);
  void OnUnlock(base::expected<DbusArray<DbusObjectPath>, ErrorDetail>
                    unlocked_collection);
  void OnOpenSession(base::expected<DbusParameters<DbusVariant, DbusObjectPath>,
                                    ErrorDetail> session_reply);
  void OnSearchItems(
      base::expected<DbusArray<DbusObjectPath>, ErrorDetail> results);
  void OnGetSecret(base::expected<DbusSecret, ErrorDetail> secret_reply);

  // KWallet migration
  void TryKWalletMigration();
  void OnNameHasOwnerForKWallet(std::optional<bool> has_owner);
  void OnKWalletIsEnabled(base::expected<DbusBoolean, ErrorDetail> is_enabled);
  void OnKWalletNetworkWallet(
      base::expected<DbusString, ErrorDetail> wallet_name);
  void OnKWalletOpen(base::expected<DbusInt32, ErrorDetail> handle_reply);
  void OnKWalletReadPassword(
      int32_t handle,
      base::expected<DbusString, ErrorDetail> secret_reply);

  void UnlockDefaultCollection();
  void OpenSession();
  void CreateItem(scoped_refptr<base::RefCountedMemory> secret);
  void OnCreateItem(scoped_refptr<base::RefCountedMemory> secret,
                    base::expected<DbusObjectPath, ErrorDetail> created_item);
  void DeriveKeyFromSecret(base::span<const uint8_t> secret);
  void FinalizeSuccess(Encryptor::Key key);
  void FinalizeFailure(InitStatus status, ErrorDetail detail);
  void RecordInitStatus(InitStatus status, ErrorDetail detail);
  void CloseSession();

  raw_ptr<dbus::ObjectProxy> default_collection_proxy_ = nullptr;
  raw_ptr<dbus::ObjectProxy> session_proxy_ = nullptr;
  bool session_opened_ = false;

  // For KWallet migration
  raw_ptr<dbus::ObjectProxy> kwallet_proxy_ = nullptr;
  size_t kwallet_candidate_index_ = 0;

  const bool use_for_encryption_;
  const std::string product_name_;
  scoped_refptr<dbus::Bus> bus_;
  KeyCallback key_callback_;

  std::string secret_for_testing_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FreedesktopSecretKeyProvider> weak_ptr_factory_{this};
};

}  // namespace os_crypt_async

#endif  // COMPONENTS_OS_CRYPT_ASYNC_BROWSER_FREEDESKTOP_SECRET_KEY_PROVIDER_H_
