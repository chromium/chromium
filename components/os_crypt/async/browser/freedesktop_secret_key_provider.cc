// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/freedesktop_secret_key_provider.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/nix/xdg_util.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/call_method.h"
#include "components/dbus/utils/connect_to_signal.h"
#include "components/dbus/utils/variant.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "crypto/kdf.h"
#include "dbus/message.h"
#include "dbus/object_path.h"

namespace os_crypt_async {

namespace {

constexpr char kUmaInitStatus[] =
    "OSCrypt.FreedesktopSecretKeyProvider.InitStatus";
constexpr char kUmaErrorDetail[] =
    "OSCrypt.FreedesktopSecretKeyProvider.$1.ErrorDetail";

// These constants are duplicated from the sync backend.
constexpr char kEncryptionTag[] = "v11";
constexpr char kSalt[] = "saltysalt";
constexpr size_t kDerivedKeySizeInBits = 128;
constexpr size_t kEncryptionIterations = 1;
constexpr size_t kSecretLengthBytes = 16;

FreedesktopSecretKeyProvider::ErrorDetail DbusErrorToErrorDetail(
    const dbus_utils::CallMethodError& error) {
  switch (error.status) {
    case dbus_utils::CallMethodErrorStatus::kNoResponse:
      return FreedesktopSecretKeyProvider::ErrorDetail::kNoResponse;
    case dbus_utils::CallMethodErrorStatus::kInvalidResponseFormat:
      return FreedesktopSecretKeyProvider::ErrorDetail::kInvalidReplyFormat;
    case dbus_utils::CallMethodErrorStatus::kExtraDataInResponse:
      return FreedesktopSecretKeyProvider::ErrorDetail::kExtraDataInResponse;
    case dbus_utils::CallMethodErrorStatus::kErrorResponse:
      return FreedesktopSecretKeyProvider::ErrorDetail::kErrorResponse;
  }
  NOTREACHED();
}

const char* InitStatusToString(
    FreedesktopSecretKeyProvider::InitStatus status) {
  switch (status) {
    case FreedesktopSecretKeyProvider::InitStatus::kSuccess:
      return "Success";
    case FreedesktopSecretKeyProvider::InitStatus::kCreateCollectionFailed:
      return "CreateCollectionFailed";
    case FreedesktopSecretKeyProvider::InitStatus::kCreateItemFailed:
      return "CreateItemFailed";
    case FreedesktopSecretKeyProvider::InitStatus::kEmptySecret:
      return "EmptySecret";
    case FreedesktopSecretKeyProvider::InitStatus::kGetSecretFailed:
      return "GetSecretFailed";
    case FreedesktopSecretKeyProvider::InitStatus::kGnomeKeyringDeadlock:
      return "GnomeKeyringDeadlock";
    case FreedesktopSecretKeyProvider::InitStatus::kNoService:
      return "NoService";
    case FreedesktopSecretKeyProvider::InitStatus::kReadAliasFailed:
      return "ReadAliasFailed";
    case FreedesktopSecretKeyProvider::InitStatus::kSearchItemsFailed:
      return "SearchItemsFailed";
    case FreedesktopSecretKeyProvider::InitStatus::kSessionFailure:
      return "SessionFailure";
    case FreedesktopSecretKeyProvider::InitStatus::kUnlockFailed:
      return "UnlockFailed";
    case FreedesktopSecretKeyProvider::InitStatus::kDisabled:
      return "Disabled";
    case FreedesktopSecretKeyProvider::InitStatus::kKWalletNoService:
      return "KWalletNoService";
    case FreedesktopSecretKeyProvider::InitStatus::kKWalletDisabled:
      return "KWalletDisabled";
    case FreedesktopSecretKeyProvider::InitStatus::kKWalletNoNetworkWallet:
      return "KWalletNoNetworkWallet";
    case FreedesktopSecretKeyProvider::InitStatus::kKWalletOpenFailed:
      return "KWalletOpenFailed";
    case FreedesktopSecretKeyProvider::InitStatus::kKWalletNoSecret:
      return "KWalletNoSecret";
    case FreedesktopSecretKeyProvider::InitStatus::kKWalletFolderCheckFailed:
      return "KWalletFolderCheckFailed";
    case FreedesktopSecretKeyProvider::InitStatus::kKWalletFolderCreationFailed:
      return "KWalletFolderCreationFailed";
    case FreedesktopSecretKeyProvider::InitStatus::kKWalletEntryCheckFailed:
      return "KWalletEntryCheckFailed";
    case FreedesktopSecretKeyProvider::InitStatus::kKWalletReadFailed:
      return "KWalletReadFailed";
    case FreedesktopSecretKeyProvider::InitStatus::kKWalletWriteFailed:
      return "KWalletWriteFailed";
  }
  NOTREACHED();
}

}  // namespace

// A helper class to handle a Secret Service prompt. It is templated on the
// return type expected from the prompt.
template <typename T>
class FreedesktopSecretKeyProvider::Prompter
    : public base::RefCountedThreadSafe<Prompter<T>> {
 public:
  using PromptCallback = base::OnceCallback<void(
      base::expected<T, FreedesktopSecretKeyProvider::ErrorDetail>)>;

  template <dbus_utils::SignatureLiteral ArgsSig,
            dbus_utils::SignatureLiteral RetsSig,
            typename... Args>
  static void Prompt(scoped_refptr<dbus::Bus> bus,
                     dbus::ObjectProxy* object_proxy,
                     const std::string& interface_name,
                     const std::string& method_name,
                     PromptCallback callback,
                     const Args&... args) {
    auto handler =
        base::MakeRefCounted<Prompter<T>>(std::move(bus), std::move(callback));
    dbus_utils::CallMethod<ArgsSig, RetsSig>(
        object_proxy, interface_name, method_name,
        base::BindOnce(&Prompter::OnReply, handler), args...);
  }

  Prompter(scoped_refptr<dbus::Bus> bus, PromptCallback callback)
      : bus_(std::move(bus)), callback_(std::move(callback)) {}

  Prompter(const Prompter&) = delete;
  Prompter& operator=(const Prompter&) = delete;

 private:
  friend class base::RefCountedThreadSafe<Prompter<T>>;
  using ErrorDetail = FreedesktopSecretKeyProvider::ErrorDetail;

  ~Prompter() {
    Finish(base::unexpected(ErrorDetail::kDestructedBeforeComplete));
  }

  void OnReply(base::expected<std::tuple<T, dbus::ObjectPath>,
                              dbus_utils::CallMethodError> reply) {
    if (!reply.has_value()) {
      Finish(base::unexpected(DbusErrorToErrorDetail(reply.error())));
      return;
    }
    auto& [value, prompt] = reply.value();
    if (prompt.value() == "/") {
      Finish(std::move(value));
    } else {
      prompt_path_ = prompt;
      StartPrompt();
    }
  }

  void StartPrompt() {
    auto* prompt_proxy = bus_->GetObjectProxy(
        FreedesktopSecretKeyProvider::kSecretServiceName, prompt_path_);
    dbus_utils::ConnectToSignal<"bv">(
        prompt_proxy, FreedesktopSecretKeyProvider::kSecretPromptInterface,
        "Completed",
        base::BindRepeating(&Prompter::OnPromptCompletedSignal, this),
        base::BindOnce(&Prompter::OnSignalConnected, this));
    dbus_utils::CallMethod<"s", "">(
        prompt_proxy, FreedesktopSecretKeyProvider::kSecretPromptInterface,
        FreedesktopSecretKeyProvider::kMethodPrompt,
        base::BindOnce(&Prompter::OnPromptResponse, this), "");
  }

  void OnPromptResponse(dbus_utils::CallMethodResult<> response) {
    if (!response.has_value()) {
      LOG(ERROR) << "Prompt call returned no response.";
      Finish(base::unexpected(DbusErrorToErrorDetail(response.error())));
    }
  }

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool connected) {
    if (!connected) {
      LOG(ERROR) << "Failed to connect to Prompt.Completed signal.";
      Finish(base::unexpected(ErrorDetail::kPromptFailedSignalConnection));
    }
  }

  void OnPromptCompletedSignal(
      dbus_utils::ConnectToSignalResultSig<"bv"> result) {
    if (!result.has_value()) {
      LOG(ERROR) << "Failed to read Prompt.Completed signal args.";
      Finish(base::unexpected(ErrorDetail::kInvalidSignalFormat));
      return;
    }

    auto& [dismissed, variant] = result.value();

    if (dismissed) {
      Finish(base::unexpected(ErrorDetail::kPromptDismissed));
      return;
    }

    auto value = std::move(variant).Take<T>();
    if (!value) {
      LOG(ERROR) << "Failed to parse prompt result.";
      Finish(base::unexpected(ErrorDetail::kInvalidVariantFormat));
      return;
    }

    Finish(base::ok(std::move(*value)));
  }

  void Finish(base::expected<T, ErrorDetail> result) {
    if (!prompt_path_.value().empty()) {
      bus_->RemoveObjectProxy(FreedesktopSecretKeyProvider::kSecretServiceName,
                              prompt_path_, base::DoNothing());
      prompt_path_ = dbus::ObjectPath();
    }
    if (callback_) {
      std::move(callback_).Run(std::move(result));
    }
    bus_.reset();
  }

  scoped_refptr<dbus::Bus> bus_;
  PromptCallback callback_;
  dbus::ObjectPath prompt_path_;
};

FreedesktopSecretKeyProvider::FreedesktopSecretKeyProvider(
    const std::string& password_store,
    const std::string& product_name,
    scoped_refptr<dbus::Bus> bus)
    : password_store_(password_store),
      product_name_(product_name),
      bus_(std::move(bus)) {
  if (!bus_) {
    bus_ = dbus_thread_linux::GetSharedSessionBus();
  }
}

FreedesktopSecretKeyProvider::~FreedesktopSecretKeyProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FreedesktopSecretKeyProvider::GetKey(KeyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  key_callback_ = std::move(callback);

  if (!secret_for_testing_.empty()) {
    DeriveKeyFromSecret(base::as_byte_span(secret_for_testing_));
    return;
  }

  // Reset state in case GetKey is called multiple times.
  kwallet_proxy_ = nullptr;
  kwallet_handle_ = kKWalletInvalidHandle;
  kwallet_transaction_id_ = kKWalletInvalidTransactionId;
  session_opened_ = false;
  session_proxy_ = nullptr;
  default_collection_proxy_ = nullptr;

  if (password_store_ == "basic") {
    // Use PosixKeyProvider.
    FinalizeFailure(InitStatus::kDisabled, ErrorDetail::kNone);
  } else if (password_store_ == "gnome-libsecret") {
    InitializeFreedesktopSecretService();
  } else if (password_store_ == "kwallet") {
    InitializeKWallet(kKWalletDService, kKWalletDPath);
  } else if (password_store_ == "kwallet5") {
    InitializeKWallet(kKWalletD5Service, kKWalletD5Path);
  } else if (password_store_ == "kwallet6") {
    InitializeKWallet(kKWalletD6Service, kKWalletD6Path);
  } else {
    if (!password_store_.empty()) {
      LOG(ERROR) << "Unknown password store: " << password_store_;
    }
    auto env = base::Environment::Create();
    switch (base::nix::GetDesktopEnvironment(env.get())) {
      case base::nix::DESKTOP_ENVIRONMENT_KDE3:
      case base::nix::DESKTOP_ENVIRONMENT_KDE4:
        InitializeKWallet(kKWalletDService, kKWalletDPath);
        break;
      case base::nix::DESKTOP_ENVIRONMENT_KDE5:
        InitializeKWallet(kKWalletD5Service, kKWalletD5Path);
        break;
      case base::nix::DESKTOP_ENVIRONMENT_KDE6:
        InitializeKWallet(kKWalletD6Service, kKWalletD6Path);
        break;
      case base::nix::DESKTOP_ENVIRONMENT_OTHER:
      case base::nix::DESKTOP_ENVIRONMENT_CINNAMON:
      case base::nix::DESKTOP_ENVIRONMENT_DEEPIN:
      case base::nix::DESKTOP_ENVIRONMENT_GNOME:
      case base::nix::DESKTOP_ENVIRONMENT_PANTHEON:
      case base::nix::DESKTOP_ENVIRONMENT_UKUI:
      case base::nix::DESKTOP_ENVIRONMENT_UNITY:
      case base::nix::DESKTOP_ENVIRONMENT_XFCE:
      case base::nix::DESKTOP_ENVIRONMENT_LXQT:
      case base::nix::DESKTOP_ENVIRONMENT_COSMIC:
        InitializeFreedesktopSecretService();
        break;
    }
  }
}

bool FreedesktopSecretKeyProvider::UseForEncryption() {
  return true;
}

bool FreedesktopSecretKeyProvider::IsCompatibleWithOsCryptSync() {
  return true;
}

void FreedesktopSecretKeyProvider::InitializeFreedesktopSecretService() {
  dbus_utils::CheckForServiceAndStart(
      bus_, kSecretServiceName,
      base::BindOnce(&FreedesktopSecretKeyProvider::OnServiceStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FreedesktopSecretKeyProvider::OnServiceStarted(
    std::optional<bool> service_started) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!service_started.value_or(false)) {
    FinalizeFailure(InitStatus::kNoService, ErrorDetail::kNone);
    return;
  }

  auto* service_proxy = bus_->GetObjectProxy(
      kSecretServiceName, dbus::ObjectPath(kSecretServicePath));
  dbus_utils::CallMethod<"s", "o">(
      service_proxy, kSecretServiceInterface, kMethodReadAlias,
      base::BindOnce(&FreedesktopSecretKeyProvider::OnReadAliasDefault,
                     weak_ptr_factory_.GetWeakPtr()),
      kDefaultAlias);
}

void FreedesktopSecretKeyProvider::OnReadAliasDefault(
    dbus_utils::CallMethodResultSig<"o"> collection_path_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!collection_path_result.has_value()) {
    FinalizeFailure(InitStatus::kReadAliasFailed,
                    DbusErrorToErrorDetail(collection_path_result.error()));
    return;
  }

  const auto& collection_path = std::get<0>(collection_path_result.value());
  if (collection_path.value() != "/") {
    default_collection_proxy_ =
        bus_->GetObjectProxy(kSecretServiceName, collection_path);
    dbus_utils::CallMethod<"ss", "v">(
        default_collection_proxy_, kPropertiesInterface, kMethodGet,
        base::BindOnce(
            &FreedesktopSecretKeyProvider::OnGetCollectionLabelResponse,
            weak_ptr_factory_.GetWeakPtr()),
        kSecretCollectionInterface, kLabelProperty);
  } else {
    // No default collection, create it
    auto* service_proxy = bus_->GetObjectProxy(
        kSecretServiceName, dbus::ObjectPath(kSecretServicePath));
    std::map<std::string, dbus_utils::Variant> props;
    props.emplace(kSecretCollectionLabelProperty,
                  dbus_utils::Variant::Wrap<"s">(kDefaultCollectionLabel));

    Prompter<dbus::ObjectPath>::Prompt<"a{sv}s", "oo">(
        bus_, service_proxy, kSecretServiceInterface, kMethodCreateCollection,
        base::BindOnce(&FreedesktopSecretKeyProvider::OnCreateCollection,
                       weak_ptr_factory_.GetWeakPtr()),
        props, kDefaultAlias);
  }
}

void FreedesktopSecretKeyProvider::OnGetCollectionLabelResponse(
    dbus_utils::CallMethodResultSig<"v"> variant_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!variant_result.has_value()) {
    LOG(ERROR) << "Get(Label) failed.";
    FinalizeFailure(InitStatus::kGnomeKeyringDeadlock,
                    DbusErrorToErrorDetail(variant_result.error()));
    return;
  }

  auto label_variant =
      std::move(std::get<0>(variant_result.value())).Take<std::string>();
  if (!label_variant) {
    LOG(ERROR) << "Label property missing or invalid.";
    FinalizeFailure(InitStatus::kGnomeKeyringDeadlock,
                    ErrorDetail::kInvalidVariantFormat);
    return;
  }

  // Label property read successfully
  UnlockDefaultCollection();
}

void FreedesktopSecretKeyProvider::OnCreateCollection(
    base::expected<dbus::ObjectPath, ErrorDetail> create_collection_reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!create_collection_reply.has_value()) {
    FinalizeFailure(InitStatus::kCreateCollectionFailed,
                    create_collection_reply.error());
    return;
  }
  if (create_collection_reply->value() == "/") {
    FinalizeFailure(InitStatus::kCreateCollectionFailed,
                    ErrorDetail::kEmptyObjectPaths);
    return;
  }
  default_collection_proxy_ =
      bus_->GetObjectProxy(kSecretServiceName, create_collection_reply.value());
  UnlockDefaultCollection();
}

void FreedesktopSecretKeyProvider::UnlockDefaultCollection() {
  auto* service_proxy = bus_->GetObjectProxy(
      kSecretServiceName, dbus::ObjectPath(kSecretServicePath));

  std::vector<dbus::ObjectPath> objects = {
      default_collection_proxy_->object_path()};
  Prompter<std::vector<dbus::ObjectPath>>::Prompt<"ao", "aoo">(
      bus_, service_proxy, kSecretServiceInterface, kMethodUnlock,
      base::BindOnce(&FreedesktopSecretKeyProvider::OnUnlock,
                     weak_ptr_factory_.GetWeakPtr()),
      objects);
}

void FreedesktopSecretKeyProvider::OnUnlock(
    base::expected<std::vector<dbus::ObjectPath>, ErrorDetail>
        unlocked_collection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!unlocked_collection.has_value()) {
    FinalizeFailure(InitStatus::kUnlockFailed, unlocked_collection.error());
    return;
  }
  if (unlocked_collection->empty()) {
    FinalizeFailure(InitStatus::kUnlockFailed, ErrorDetail::kEmptyObjectPaths);
    return;
  }
  // Unlocked now
  OpenSession();
}

void FreedesktopSecretKeyProvider::OpenSession() {
  auto* service_proxy = bus_->GetObjectProxy(
      kSecretServiceName, dbus::ObjectPath(kSecretServicePath));
  dbus_utils::CallMethod<"sv", "vo">(
      service_proxy, kSecretServiceInterface, kMethodOpenSession,
      base::BindOnce(&FreedesktopSecretKeyProvider::OnOpenSession,
                     weak_ptr_factory_.GetWeakPtr()),
      kAlgorithmPlain, dbus_utils::Variant::Wrap<"s">(kInputPlain));
}

void FreedesktopSecretKeyProvider::OnOpenSession(
    dbus_utils::CallMethodResultSig<"vo"> session_reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!session_reply.has_value()) {
    FinalizeFailure(InitStatus::kSessionFailure,
                    DbusErrorToErrorDetail(session_reply.error()));
    return;
  }
  const auto& [_, result] = session_reply.value();
  session_proxy_ = bus_->GetObjectProxy(kSecretServiceName, result);
  session_opened_ = true;

  std::map<std::string, std::string> search_attrs{
      {kApplicationAttributeKey, kAppName}};

  dbus_utils::CallMethod<"a{ss}", "ao">(
      default_collection_proxy_, kSecretCollectionInterface, kMethodSearchItems,
      base::BindOnce(&FreedesktopSecretKeyProvider::OnSearchItems,
                     weak_ptr_factory_.GetWeakPtr()),
      search_attrs);
}

void FreedesktopSecretKeyProvider::OnSearchItems(
    dbus_utils::CallMethodResultSig<"ao"> results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!results.has_value()) {
    FinalizeFailure(InitStatus::kSearchItemsFailed,
                    DbusErrorToErrorDetail(results.error()));
    return;
  }

  const auto& result_paths = std::get<0>(results.value());
  if (result_paths.empty()) {
    // No items found, create a new secret.
    CreateItem(base::MakeRefCounted<base::RefCountedString>(
        base::Base64Encode(base::RandBytesAsVector(kSecretLengthBytes))));
    return;
  }

  auto* item_proxy =
      bus_->GetObjectProxy(kSecretServiceName, result_paths.front());
  dbus_utils::CallMethod<"o", "(oayays)">(
      item_proxy, kSecretItemInterface, kMethodGetSecret,
      base::BindOnce(&FreedesktopSecretKeyProvider::OnGetSecret,
                     weak_ptr_factory_.GetWeakPtr()),
      session_proxy_->object_path());
}

void FreedesktopSecretKeyProvider::OnGetSecret(
    dbus_utils::CallMethodResultSig<"(oayays)"> secret_reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!secret_reply.has_value()) {
    FinalizeFailure(InitStatus::kGetSecretFailed,
                    DbusErrorToErrorDetail(secret_reply.error()));
    return;
  }

  const auto& [session_path, parameters, value, content_type] =
      std::get<0>(secret_reply.value());

  if (value.empty()) {
    LOG(ERROR) << "GetSecret returned an empty secret.";
    FinalizeFailure(InitStatus::kEmptySecret, ErrorDetail::kNone);
    return;
  }

  DeriveKeyFromSecret(base::span(value));
}

void FreedesktopSecretKeyProvider::InitializeKWallet(
    const char* kwallet_service,
    const char* kwallet_path) {
  kwallet_proxy_ =
      bus_->GetObjectProxy(kwallet_service, dbus::ObjectPath(kwallet_path));
  dbus_utils::CheckForServiceAndStart(
      bus_.get(), kwallet_service,
      base::BindOnce(&FreedesktopSecretKeyProvider::OnKWalletServiceStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FreedesktopSecretKeyProvider::OnKWalletServiceStarted(
    std::optional<bool> service_started) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!service_started.value_or(false)) {
    FinalizeFailure(InitStatus::kKWalletNoService, ErrorDetail::kNone);
    return;
  }

  dbus_utils::CallMethod<"", "b">(
      kwallet_proxy_, kKWalletInterface, kKWalletMethodIsEnabled,
      base::BindOnce(&FreedesktopSecretKeyProvider::OnKWalletIsEnabled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FreedesktopSecretKeyProvider::OnKWalletIsEnabled(
    dbus_utils::CallMethodResultSig<"b"> is_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_enabled.has_value() || !std::get<0>(is_enabled.value())) {
    FinalizeFailure(InitStatus::kKWalletDisabled,
                    is_enabled.has_value()
                        ? ErrorDetail::kNone
                        : DbusErrorToErrorDetail(is_enabled.error()));
    return;
  }
  dbus_utils::CallMethod<"", "s">(
      kwallet_proxy_, kKWalletInterface, kKWalletMethodNetworkWallet,
      base::BindOnce(&FreedesktopSecretKeyProvider::OnKWalletNetworkWallet,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FreedesktopSecretKeyProvider::OnKWalletNetworkWallet(
    dbus_utils::CallMethodResultSig<"s"> wallet_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!wallet_name.has_value()) {
    FinalizeFailure(InitStatus::kKWalletNoNetworkWallet,
                    DbusErrorToErrorDetail(wallet_name.error()));
    return;
  }

  dbus_utils::ConnectToSignal<"ii">(
      kwallet_proxy_, kKWalletInterface, kKWalletSignalWalletAsyncOpened,
      base::BindRepeating(
          &FreedesktopSecretKeyProvider::OnKWalletWalletAsyncOpened,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FreedesktopSecretKeyProvider::OnSignalConnected,
                     weak_ptr_factory_.GetWeakPtr()));

  // handleSession = true means KWallet will take care of keeping the wallet
  // open as long as the application is running.
  constexpr bool kHandleSession = true;
  dbus_utils::CallMethod<"sxsb", "i">(
      kwallet_proxy_, kKWalletInterface, kKWalletMethodOpenAsync,
      base::BindOnce(&FreedesktopSecretKeyProvider::OnKWalletOpenAsync,
                     weak_ptr_factory_.GetWeakPtr()),
      std::get<0>(wallet_name.value()), 0, product_name_, kHandleSession);
}

void FreedesktopSecretKeyProvider::OnKWalletOpenAsync(
    dbus_utils::CallMethodResultSig<"i"> t_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!t_id.has_value()) {
    FinalizeFailure(InitStatus::kKWalletOpenFailed,
                    DbusErrorToErrorDetail(t_id.error()));
    return;
  }

  kwallet_transaction_id_ = std::get<0>(t_id.value());
}

void FreedesktopSecretKeyProvider::OnSignalConnected(
    const std::string& interface_name,
    const std::string& signal_name,
    bool connected) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connected) {
    LOG(ERROR) << "Failed to connect to " << signal_name << " signal.";
    FinalizeFailure(InitStatus::kKWalletOpenFailed,
                    ErrorDetail::kPromptFailedSignalConnection);
  }
}

void FreedesktopSecretKeyProvider::OnKWalletWalletAsyncOpened(
    dbus_utils::ConnectToSignalResultSig<"ii"> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (kwallet_transaction_id_ == kKWalletInvalidTransactionId) {
    return;
  }

  if (!result.has_value()) {
    LOG(ERROR) << "Failed to read walletAsyncOpened signal args.";
    FinalizeFailure(InitStatus::kKWalletOpenFailed,
                    ErrorDetail::kInvalidSignalFormat);
    return;
  }

  auto& [t_id, handle] = result.value();

  if (t_id != kwallet_transaction_id_) {
    return;
  }

  // Reset transaction ID to avoid processing the signal again.
  kwallet_transaction_id_ = kKWalletInvalidTransactionId;

  OnKWalletOpen(handle);
}

void FreedesktopSecretKeyProvider::OnKWalletOpen(int32_t handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  kwallet_handle_ = handle;
  if (kwallet_handle_ == kKWalletInvalidHandle) {
    FinalizeFailure(InitStatus::kKWalletOpenFailed,
                    ErrorDetail::kKWalletApiReturnedError);
    return;
  }

  dbus_utils::CallMethod<"iss", "b">(
      kwallet_proxy_, kKWalletInterface, kKWalletMethodHasFolder,
      base::BindOnce(&FreedesktopSecretKeyProvider::OnKWalletHasFolder,
                     weak_ptr_factory_.GetWeakPtr()),
      kwallet_handle_, kKWalletFolder, product_name_);
}

void FreedesktopSecretKeyProvider::OnKWalletHasFolder(
    dbus_utils::CallMethodResultSig<"b"> has_folder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!has_folder.has_value()) {
    FinalizeFailure(InitStatus::kKWalletFolderCheckFailed,
                    DbusErrorToErrorDetail(has_folder.error()));
    return;
  }

  if (std::get<0>(has_folder.value())) {
    dbus_utils::CallMethod<"isss", "b">(
        kwallet_proxy_, kKWalletInterface, kKWalletMethodHasEntry,
        base::BindOnce(&FreedesktopSecretKeyProvider::OnKWalletHasEntry,
                       weak_ptr_factory_.GetWeakPtr()),
        kwallet_handle_, kKWalletFolder, kKeyName, product_name_);
  } else {
    dbus_utils::CallMethod<"iss", "b">(
        kwallet_proxy_, kKWalletInterface, kKWalletMethodCreateFolder,
        base::BindOnce(&FreedesktopSecretKeyProvider::OnKWalletCreateFolder,
                       weak_ptr_factory_.GetWeakPtr()),
        kwallet_handle_, kKWalletFolder, product_name_);
  }
}

void FreedesktopSecretKeyProvider::OnKWalletCreateFolder(
    dbus_utils::CallMethodResultSig<"b"> success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success.has_value()) {
    FinalizeFailure(InitStatus::kKWalletFolderCreationFailed,
                    DbusErrorToErrorDetail(success.error()));
    return;
  }
  if (!std::get<0>(success.value())) {
    FinalizeFailure(InitStatus::kKWalletFolderCreationFailed,
                    ErrorDetail::kKWalletApiReturnedFalse);
    return;
  }
  GenerateAndWriteKWalletPassword();
}

void FreedesktopSecretKeyProvider::OnKWalletHasEntry(
    dbus_utils::CallMethodResultSig<"b"> has_entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!has_entry.has_value()) {
    FinalizeFailure(InitStatus::kKWalletEntryCheckFailed,
                    DbusErrorToErrorDetail(has_entry.error()));
    return;
  }

  if (std::get<0>(has_entry.value())) {
    dbus_utils::CallMethod<"isss", "s">(
        kwallet_proxy_, kKWalletInterface, kKWalletMethodReadPassword,
        base::BindOnce(&FreedesktopSecretKeyProvider::OnKWalletReadPassword,
                       weak_ptr_factory_.GetWeakPtr()),
        kwallet_handle_, kKWalletFolder, kKeyName, product_name_);
  } else {
    GenerateAndWriteKWalletPassword();
  }
}

void FreedesktopSecretKeyProvider::OnKWalletReadPassword(
    dbus_utils::CallMethodResultSig<"s"> secret_reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!secret_reply.has_value()) {
    FinalizeFailure(InitStatus::kKWalletReadFailed,
                    DbusErrorToErrorDetail(secret_reply.error()));
    return;
  }

  std::string secret = std::move(std::get<0>(secret_reply.value()));
  if (secret.empty()) {
    // The synchronous KWallet backend generates a new password if the
    // existing one is empty, so that logic is duplicated here.
    LOG(ERROR) << "Existing KWallet password is empty. Generating a new one.";
    GenerateAndWriteKWalletPassword();
    return;
  }

  DeriveKeyFromSecret(base::as_byte_span(secret));
}

void FreedesktopSecretKeyProvider::GenerateAndWriteKWalletPassword() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto secret = base::MakeRefCounted<base::RefCountedString>(
      base::Base64Encode(base::RandBytesAsVector(kSecretLengthBytes)));
  dbus_utils::CallMethod<"issss", "i">(
      kwallet_proxy_, kKWalletInterface, kKWalletMethodWritePassword,
      base::BindOnce(&FreedesktopSecretKeyProvider::OnKWalletWritePassword,
                     weak_ptr_factory_.GetWeakPtr(), secret),
      kwallet_handle_, kKWalletFolder, kKeyName, secret->as_string(),
      product_name_);
}

void FreedesktopSecretKeyProvider::OnKWalletWritePassword(
    scoped_refptr<base::RefCountedMemory> generated_secret,
    dbus_utils::CallMethodResultSig<"i"> return_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!return_code.has_value()) {
    FinalizeFailure(InitStatus::kKWalletWriteFailed,
                    DbusErrorToErrorDetail(return_code.error()));
    return;
  }

  int32_t kwallet_code = std::get<0>(return_code.value());
  if (kwallet_code != 0) {
    LOG(ERROR) << "KWallet writePassword failed with code: " << kwallet_code;
    FinalizeFailure(InitStatus::kKWalletWriteFailed,
                    ErrorDetail::kKWalletApiReturnedError);
    return;
  }

  DeriveKeyFromSecret(*generated_secret);
}

void FreedesktopSecretKeyProvider::CreateItem(
    scoped_refptr<base::RefCountedMemory> secret) {
  std::map<std::string, std::string> attributes{
      {kApplicationAttributeKey, kAppName},
      {kSchemaAttributeKey, kSchemaAttributeValue}};

  std::map<std::string, dbus_utils::Variant> props;
  props.emplace(kSecretItemAttributesProperty,
                dbus_utils::Variant::Wrap<"a{ss}">(std::move(attributes)));
  props.emplace(kSecretItemLabelProperty,
                dbus_utils::Variant::Wrap<"s">(kKeyName));

  std::vector<uint8_t> secret_bytes(secret->begin(), secret->end());
  auto secret_struct =
      std::make_tuple(session_proxy_->object_path(), std::vector<uint8_t>(),
                      std::move(secret_bytes), kMimePlain);

  auto* collection_proxy = bus_->GetObjectProxy(
      kSecretServiceName, default_collection_proxy_->object_path());
  Prompter<dbus::ObjectPath>::Prompt<"a{sv}(oayays)b", "oo">(
      bus_, collection_proxy, kSecretCollectionInterface, kMethodCreateItem,
      base::BindOnce(&FreedesktopSecretKeyProvider::OnCreateItem,
                     weak_ptr_factory_.GetWeakPtr(), secret),
      props, secret_struct, false);
}

void FreedesktopSecretKeyProvider::OnCreateItem(
    scoped_refptr<base::RefCountedMemory> secret,
    base::expected<dbus::ObjectPath, ErrorDetail> created_item) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!created_item.has_value()) {
    FinalizeFailure(InitStatus::kCreateItemFailed, created_item.error());
    return;
  }
  if (created_item->value().empty()) {
    FinalizeFailure(InitStatus::kCreateItemFailed,
                    ErrorDetail::kEmptyObjectPaths);
    return;
  }
  DeriveKeyFromSecret(*secret);
}

void FreedesktopSecretKeyProvider::DeriveKeyFromSecret(
    base::span<const uint8_t> secret) {
  static_assert(kDerivedKeySizeInBits % 8 == 0);
  std::array<uint8_t, kDerivedKeySizeInBits / 8> key_bytes;
  crypto::kdf::DeriveKeyPbkdf2HmacSha1(
      {kEncryptionIterations}, secret,
      base::as_byte_span(base::span_from_cstring(kSalt)), key_bytes,
      crypto::SubtlePassKey{});
  Encryptor::Key key(key_bytes, mojom::Algorithm::kAES128CBC);
  FinalizeSuccess(std::move(key));
}

void FreedesktopSecretKeyProvider::FinalizeSuccess(Encryptor::Key key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordInitStatus(InitStatus::kSuccess, ErrorDetail::kNone);
  std::move(key_callback_).Run(kEncryptionTag, std::move(key));
  CloseSession();
}

void FreedesktopSecretKeyProvider::FinalizeFailure(InitStatus status,
                                                   ErrorDetail detail) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!key_callback_) {
    return;
  }
  RecordInitStatus(status, detail);
  std::move(key_callback_)
      .Run(kEncryptionTag,
           base::unexpected(KeyProvider::KeyError::kPermanentlyUnavailable));
  CloseSession();
}

void FreedesktopSecretKeyProvider::RecordInitStatus(InitStatus status,
                                                    ErrorDetail detail) {
  // Log the high-level InitStatus.
  base::UmaHistogramEnumeration(kUmaInitStatus, status);

  // If there was an error, also log the error detail.
  if (status != InitStatus::kSuccess) {
    auto histogram_name = base::ReplaceStringPlaceholders(
        kUmaErrorDetail, std::vector<std::string>{InitStatusToString(status)},
        nullptr);
    base::UmaHistogramEnumeration(histogram_name, detail);
  }
}

void FreedesktopSecretKeyProvider::CloseSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (session_opened_) {
    dbus_utils::CallMethod<"", "">(session_proxy_, kSecretSessionInterface,
                                   kMethodClose, base::DoNothing());
  }
  if (kwallet_handle_ != kKWalletInvalidHandle) {
    dbus_utils::CallMethod<"ibs", "i">(kwallet_proxy_, kKWalletInterface,
                                       kKWalletMethodClose, base::DoNothing(),
                                       kwallet_handle_, false, product_name_);
    kwallet_handle_ = kKWalletInvalidHandle;
  }
}

}  // namespace os_crypt_async
