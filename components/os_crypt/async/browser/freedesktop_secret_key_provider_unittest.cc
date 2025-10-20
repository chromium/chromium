// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/freedesktop_secret_key_provider.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/containers/span.h"
#include "base/files/scoped_file.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "build/branding_buildflags.h"
#include "components/dbus/utils/name_has_owner.h"
#include "components/dbus/utils/read_value.h"
#include "components/dbus/utils/signature.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/utils/write_value.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace os_crypt_async {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;

namespace {

constexpr char kProductName[] = "test_product";
constexpr char kCollectionPath[] =
    "/org/freedesktop/secrets/collection/default";
constexpr char kSessionPath[] = "/org/freedesktop/secrets/session/test_session";
constexpr char kItemPath[] =
    "/org/freedesktop/secrets/collection/default/item0";
constexpr char kCollectionPromptPath[] =
    "/org/freedesktop/secrets/prompt/collection_prompt";
constexpr char kUnlockPromptPath[] =
    "/org/freedesktop/secrets/prompt/unlock_prompt";
constexpr char kItemPromptPath[] =
    "/org/freedesktop/secrets/prompt/item_prompt";
constexpr char kNetworkWallet[] = "kdewallet";
constexpr int32_t kKWalletHandle = 42;
constexpr int32_t kTransactionId = 123;

constexpr char kFakeSecret[] = "c3VwZXJfc2VjcmV0X2tleQ==";

using ArgVariant = std::variant<dbus::ObjectPath,
                                std::string,
                                std::vector<dbus::ObjectPath>,
                                dbus_utils::Variant,
                                std::map<std::string, std::string>,
                                std::map<std::string, dbus_utils::Variant>,
                                std::tuple<dbus::ObjectPath,
                                           std::vector<uint8_t>,
                                           std::vector<uint8_t>,
                                           std::string>,
                                int64_t,
                                int32_t,
                                bool>;
using ArgsVector = std::vector<ArgVariant>;

// Matcher for method call arguments. Use MatchArgs() to create one.
class ArgsMatcher {
 public:
  using is_gtest_matcher = void;

  explicit ArgsMatcher(ArgsVector args) : args_(std::move(args)) {}

  ArgsMatcher(ArgsMatcher&&) = default;
  ArgsMatcher& operator=(ArgsMatcher&&) = default;

  ~ArgsMatcher() = default;

  bool MatchAndExplain(const ArgsVector& match, std::ostream*) const {
    return match == args_;
  }

  void DescribeTo(std::ostream* os) const { *os << "DbusTypes match"; }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "DbusTypes mismatch";
  }

 private:
  ArgsVector args_;
};

template <typename... Args>
void WriteImpl(dbus::MessageWriter* writer, Args&&... args) {
  (dbus_utils::WriteValue(*writer, std::forward<Args>(args)), ...);
}

template <typename... Args>
void Write(dbus::MessageWriter* writer, Args&&... args) {
  WriteImpl(writer, std::forward<Args>(args)...);
}

template <typename... Args>
void PackImpl(ArgsVector& variants, Args&&... args) {
  (variants.push_back(std::forward<Args>(args)), ...);
}

// Create a GTest matcher for method call arguments.
template <typename... Ts>
ArgsMatcher MatchArgs(Ts&&... ts) {
  ArgsVector args;
  PackImpl(args, std::forward<Ts>(ts)...);
  return ArgsMatcher(std::move(args));
}

// Create a response callback that responds with the given arguments.
template <typename... Args>
auto RespondWith(Args&&... args) {
  return [... args = std::forward<Args>(args)](
             const std::string&, const std::string&, const ArgsVector&,
             dbus::ObjectProxy::ResponseOrErrorCallback* callback) mutable {
    auto response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    Write(&writer, std::move(args)...);
    std::move(*callback).Run(response.get(), nullptr);
  };
}

auto RespondWithTrue(const std::string&,
                     const std::string&,
                     const ArgsVector&,
                     dbus::ObjectProxy::ResponseCallback* callback) {
  auto response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  writer.AppendBool(true);
  std::move(*callback).Run(response.get());
}

template <typename T>
bool TryReadValue(dbus::MessageReader& reader,
                  const std::string& signature,
                  ArgsVector& args) {
  if (signature != dbus_utils::GetDBusTypeSignature<T>()) {
    return false;
  }
  auto value = dbus_utils::ReadValue<T>(reader);
  if (!value.has_value()) {
    return false;
  }
  args.push_back(std::move(*value));
  return true;
}

template <typename... Ts>
ArgsVector ReadDbusMessageImpl(dbus::MessageReader& reader,
                               std::variant<Ts...>*) {
  ArgsVector args;
  while (reader.HasMoreData()) {
    std::string signature = reader.GetDataSignature();
    if (!(TryReadValue<Ts>(reader, signature, args) || ...)) {
      break;
    }
  }
  return args;
}

ArgsVector ReadDbusMessage(dbus::MessageReader& reader) {
  ArgVariant* arg = nullptr;
  return ReadDbusMessageImpl(reader, arg);
}

// Used to mock ObjectProxy calls with typed arguments and responses.
class MockObjectProxyWithTypedCalls : public dbus::MockObjectProxy {
 public:
  MockObjectProxyWithTypedCalls(dbus::Bus* bus,
                                const std::string& service_name,
                                const dbus::ObjectPath& object_path)
      : dbus::MockObjectProxy(bus, service_name, object_path) {}

  void CallMethod(dbus::MethodCall* method_call,
                  int timeout_ms,
                  ResponseCallback callback) override {
    dbus::MessageReader reader(method_call);
    auto args = ReadDbusMessage(reader);
    CallWithoutError(method_call->GetInterface(), method_call->GetMember(),
                     std::move(args), &callback);
  }

  void CallMethodWithErrorResponse(dbus::MethodCall* method_call,
                                   int timeout_ms,
                                   ResponseOrErrorCallback callback) override {
    dbus::MessageReader reader(method_call);
    auto args = ReadDbusMessage(reader);
    Call(method_call->GetInterface(), method_call->GetMember(), std::move(args),
         &callback);
  }

  MOCK_METHOD(void,
              Call,
              (const std::string& interface,
               const std::string& method_name,
               const ArgsVector& args,
               dbus::ObjectProxy::ResponseOrErrorCallback* callback));

  MOCK_METHOD(void,
              CallWithoutError,
              (const std::string& interface,
               const std::string& method_name,
               const ArgsVector& args,
               dbus::ObjectProxy::ResponseCallback* callback));

 protected:
  ~MockObjectProxyWithTypedCalls() override = default;
};

}  // namespace

TEST(FreedesktopSecretKeyProviderTest, BasicHappyPath) {
  auto mock_bus = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

  // Initialize object proxies
  auto mock_dbus_proxy = base::MakeRefCounted<MockObjectProxyWithTypedCalls>(
      mock_bus.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
  EXPECT_CALL(*mock_bus, GetObjectProxy(DBUS_SERVICE_DBUS,
                                        dbus::ObjectPath(DBUS_PATH_DBUS)))
      .WillRepeatedly(Return(mock_dbus_proxy.get()));
  auto mock_service_proxy = base::MakeRefCounted<MockObjectProxyWithTypedCalls>(
      mock_bus.get(), FreedesktopSecretKeyProvider::kSecretServiceName,
      dbus::ObjectPath(FreedesktopSecretKeyProvider::kSecretServicePath));
  EXPECT_CALL(
      *mock_bus,
      GetObjectProxy(
          FreedesktopSecretKeyProvider::kSecretServiceName,
          dbus::ObjectPath(FreedesktopSecretKeyProvider::kSecretServicePath)))
      .WillRepeatedly(Return(mock_service_proxy.get()));
  auto mock_collection_proxy =
      base::MakeRefCounted<MockObjectProxyWithTypedCalls>(
          mock_bus.get(), FreedesktopSecretKeyProvider::kSecretServiceName,
          dbus::ObjectPath(kCollectionPath));
  EXPECT_CALL(*mock_bus,
              GetObjectProxy(FreedesktopSecretKeyProvider::kSecretServiceName,
                             dbus::ObjectPath(kCollectionPath)))
      .WillRepeatedly(Return(mock_collection_proxy.get()));
  auto mock_item_proxy = base::MakeRefCounted<MockObjectProxyWithTypedCalls>(
      mock_bus.get(), FreedesktopSecretKeyProvider::kSecretServiceName,
      dbus::ObjectPath(kItemPath));
  EXPECT_CALL(*mock_bus,
              GetObjectProxy(FreedesktopSecretKeyProvider::kSecretServiceName,
                             dbus::ObjectPath(kItemPath)))
      .WillRepeatedly(Return(mock_item_proxy.get()));
  auto mock_session_proxy = base::MakeRefCounted<MockObjectProxyWithTypedCalls>(
      mock_bus.get(), FreedesktopSecretKeyProvider::kSecretServiceName,
      dbus::ObjectPath(kSessionPath));
  EXPECT_CALL(*mock_bus,
              GetObjectProxy(FreedesktopSecretKeyProvider::kSecretServiceName,
                             dbus::ObjectPath(kSessionPath)))
      .WillRepeatedly(Return(mock_session_proxy.get()));

  // NameHasOwner for Secret Service
  EXPECT_CALL(
      *mock_dbus_proxy,
      CallWithoutError(
          DBUS_INTERFACE_DBUS, "NameHasOwner",
          MatchArgs(FreedesktopSecretKeyProvider::kSecretServiceName), _))
      .WillOnce(RespondWithTrue);

  // ReadAlias("default")
  EXPECT_CALL(*mock_service_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretServiceInterface,
                   FreedesktopSecretKeyProvider::kMethodReadAlias,
                   MatchArgs(FreedesktopSecretKeyProvider::kDefaultAlias), _))
      .WillOnce(RespondWith(dbus::ObjectPath(kCollectionPath)));

  // Get(Label)
  EXPECT_CALL(
      *mock_collection_proxy,
      Call(FreedesktopSecretKeyProvider::kPropertiesInterface,
           FreedesktopSecretKeyProvider::kMethodGet,
           MatchArgs(FreedesktopSecretKeyProvider::kSecretCollectionInterface,
                     FreedesktopSecretKeyProvider::kLabelProperty),
           _))
      .WillOnce(RespondWith(dbus_utils::Variant::Wrap<"s">(
          FreedesktopSecretKeyProvider::kDefaultCollectionLabel)));

  // Unlock(default_collection)
  EXPECT_CALL(*mock_service_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretServiceInterface,
                   FreedesktopSecretKeyProvider::kMethodUnlock,
                   MatchArgs(std::vector<dbus::ObjectPath>{
                       dbus::ObjectPath(kCollectionPath)}),
                   _))
      .WillOnce(RespondWith(
          std::vector<dbus::ObjectPath>{dbus::ObjectPath(kCollectionPath)},
          dbus::ObjectPath("/")));

  // OpenSession
  EXPECT_CALL(*mock_service_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretServiceInterface,
                   FreedesktopSecretKeyProvider::kMethodOpenSession,
                   MatchArgs(FreedesktopSecretKeyProvider::kAlgorithmPlain,
                             dbus_utils::Variant::Wrap<"s">(
                                 FreedesktopSecretKeyProvider::kInputPlain)),
                   _))
      .WillOnce(RespondWith(dbus_utils::Variant::Wrap<"s">(""),
                            dbus::ObjectPath(kSessionPath)));

  // SearchItems
  std::map<std::string, std::string> attributes;
  attributes[FreedesktopSecretKeyProvider::kApplicationAttributeKey] =
      FreedesktopSecretKeyProvider::kAppName;
  EXPECT_CALL(*mock_collection_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretCollectionInterface,
                   FreedesktopSecretKeyProvider::kMethodSearchItems,
                   MatchArgs(std::move(attributes)), _))
      .WillOnce(RespondWith(
          std::vector<dbus::ObjectPath>{dbus::ObjectPath(kItemPath)}));

  // GetSecret
  auto fake_secret_span = base::as_byte_span(kFakeSecret);
  EXPECT_CALL(*mock_item_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretItemInterface,
                   FreedesktopSecretKeyProvider::kMethodGetSecret,
                   MatchArgs(dbus::ObjectPath(kSessionPath)), _))
      .WillOnce(RespondWith(std::make_tuple(
          dbus::ObjectPath(kSessionPath), std::vector<uint8_t>(),
          std::vector<uint8_t>(fake_secret_span.begin(),
                               fake_secret_span.end()),
          std::string(FreedesktopSecretKeyProvider::kMimePlain))));

  // Close
  EXPECT_CALL(*mock_session_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretSessionInterface,
                   FreedesktopSecretKeyProvider::kMethodClose, MatchArgs(), _))
      .WillOnce(RespondWith());

  FreedesktopSecretKeyProvider provider("gnome-libsecret", kProductName,
                                        mock_bus);
  std::string tag;
  std::optional<Encryptor::Key> key;
  provider.GetKey(base::BindLambdaForTesting(
      [&](const std::string& returned_tag,
          base::expected<Encryptor::Key, KeyProvider::KeyError> returned_key) {
        tag = returned_tag;
        key = std::move(returned_key.value());
      }));
  EXPECT_EQ(tag, "v11");
  EXPECT_TRUE(key.has_value());
}

TEST(FreedesktopSecretKeyProviderTest,
     CreateCollectionAndItemWithUnlockPrompt) {
  auto mock_bus = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

  EXPECT_CALL(*mock_bus, AssertOnOriginThread()).WillRepeatedly([] {});

  // Initialize object proxies
  auto mock_dbus_proxy = base::MakeRefCounted<MockObjectProxyWithTypedCalls>(
      mock_bus.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
  EXPECT_CALL(*mock_bus, GetObjectProxy(DBUS_SERVICE_DBUS,
                                        dbus::ObjectPath(DBUS_PATH_DBUS)))
      .WillRepeatedly(Return(mock_dbus_proxy.get()));
  auto mock_service_proxy = base::MakeRefCounted<MockObjectProxyWithTypedCalls>(
      mock_bus.get(), FreedesktopSecretKeyProvider::kSecretServiceName,
      dbus::ObjectPath(FreedesktopSecretKeyProvider::kSecretServicePath));
  EXPECT_CALL(
      *mock_bus,
      GetObjectProxy(
          FreedesktopSecretKeyProvider::kSecretServiceName,
          dbus::ObjectPath(FreedesktopSecretKeyProvider::kSecretServicePath)))
      .WillRepeatedly(Return(mock_service_proxy.get()));
  auto mock_collection_prompt_proxy =
      base::MakeRefCounted<MockObjectProxyWithTypedCalls>(
          mock_bus.get(), FreedesktopSecretKeyProvider::kSecretServiceName,
          dbus::ObjectPath(kCollectionPromptPath));
  EXPECT_CALL(*mock_bus,
              GetObjectProxy(FreedesktopSecretKeyProvider::kSecretServiceName,
                             dbus::ObjectPath(kCollectionPromptPath)))
      .WillRepeatedly(Return(mock_collection_prompt_proxy.get()));
  auto mock_unlock_prompt_proxy =
      base::MakeRefCounted<MockObjectProxyWithTypedCalls>(
          mock_bus.get(), FreedesktopSecretKeyProvider::kSecretServiceName,
          dbus::ObjectPath(kUnlockPromptPath));
  EXPECT_CALL(*mock_bus,
              GetObjectProxy(FreedesktopSecretKeyProvider::kSecretServiceName,
                             dbus::ObjectPath(kUnlockPromptPath)))
      .WillRepeatedly(Return(mock_unlock_prompt_proxy.get()));
  auto mock_item_prompt_proxy =
      base::MakeRefCounted<MockObjectProxyWithTypedCalls>(
          mock_bus.get(), FreedesktopSecretKeyProvider::kSecretServiceName,
          dbus::ObjectPath(kItemPromptPath));
  EXPECT_CALL(*mock_bus,
              GetObjectProxy(FreedesktopSecretKeyProvider::kSecretServiceName,
                             dbus::ObjectPath(kItemPromptPath)))
      .WillRepeatedly(Return(mock_item_prompt_proxy.get()));
  auto mock_session_proxy = base::MakeRefCounted<MockObjectProxyWithTypedCalls>(
      mock_bus.get(), FreedesktopSecretKeyProvider::kSecretServiceName,
      dbus::ObjectPath(kSessionPath));
  EXPECT_CALL(*mock_bus,
              GetObjectProxy(FreedesktopSecretKeyProvider::kSecretServiceName,
                             dbus::ObjectPath(kSessionPath)))
      .WillRepeatedly(Return(mock_session_proxy.get()));
  auto mock_collection_proxy =
      base::MakeRefCounted<MockObjectProxyWithTypedCalls>(
          mock_bus.get(), FreedesktopSecretKeyProvider::kSecretServiceName,
          dbus::ObjectPath(kCollectionPath));
  EXPECT_CALL(*mock_bus,
              GetObjectProxy(FreedesktopSecretKeyProvider::kSecretServiceName,
                             dbus::ObjectPath(kCollectionPath)))
      .WillRepeatedly(Return(mock_collection_proxy.get()));
  auto mock_item_proxy = base::MakeRefCounted<MockObjectProxyWithTypedCalls>(
      mock_bus.get(), FreedesktopSecretKeyProvider::kSecretServiceName,
      dbus::ObjectPath(kItemPath));
  EXPECT_CALL(*mock_bus,
              GetObjectProxy(FreedesktopSecretKeyProvider::kSecretServiceName,
                             dbus::ObjectPath(kItemPath)))
      .WillRepeatedly(Return(mock_item_proxy.get()));

  // NameHasOwner for Secret Service
  EXPECT_CALL(
      *mock_dbus_proxy,
      CallWithoutError(
          DBUS_INTERFACE_DBUS, "NameHasOwner",
          MatchArgs(FreedesktopSecretKeyProvider::kSecretServiceName), _))
      .WillOnce(RespondWithTrue);

  // ReadAlias("default") returns no default collection
  EXPECT_CALL(*mock_service_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretServiceInterface,
                   FreedesktopSecretKeyProvider::kMethodReadAlias,
                   MatchArgs(FreedesktopSecretKeyProvider::kDefaultAlias), _))
      .WillOnce(RespondWith(dbus::ObjectPath("/")));

  // CreateCollection returns a prompt
  EXPECT_CALL(*mock_service_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretServiceInterface,
                   "CreateCollection", _, _))
      .WillOnce(RespondWith(dbus::ObjectPath("/"),
                            dbus::ObjectPath(kCollectionPromptPath)));

  EXPECT_CALL(*mock_collection_prompt_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretPromptInterface,
                   FreedesktopSecretKeyProvider::kMethodPrompt, _, _))
      .WillOnce(RespondWith());

  EXPECT_CALL(*mock_collection_prompt_proxy, ConnectToSignal(_, _, _, _))
      .WillOnce([](const std::string& interface_name,
                   const std::string& signal_name,
                   dbus::ObjectProxy::SignalCallback signal_callback,
                   dbus::ObjectProxy::OnConnectedCallback on_connected) {
        // Connected successfully
        std::move(on_connected).Run(interface_name, signal_name, true);

        // Trigger the signal callback with a non-empty collection path now
        auto signal = dbus::Signal(interface_name, signal_name);
        dbus::MessageWriter writer(&signal);
        // Prompt completed: dismissed = false, return the newly created
        // collection path
        dbus_utils::WriteValue(writer, false);
        dbus_utils::WriteValue(writer, dbus_utils::Variant::Wrap<"o">(
                                           dbus::ObjectPath(kCollectionPath)));
        signal_callback.Run(&signal);
      });

  // Unlock collection returns a prompt
  EXPECT_CALL(*mock_service_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretServiceInterface,
                   FreedesktopSecretKeyProvider::kMethodUnlock,
                   MatchArgs(std::vector<dbus::ObjectPath>{
                       dbus::ObjectPath(kCollectionPath)}),
                   _))
      .WillOnce(RespondWith(std::vector<dbus::ObjectPath>(),
                            dbus::ObjectPath(kUnlockPromptPath)));

  EXPECT_CALL(*mock_unlock_prompt_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretPromptInterface,
                   FreedesktopSecretKeyProvider::kMethodPrompt, _, _))
      .WillOnce(RespondWith());

  EXPECT_CALL(*mock_unlock_prompt_proxy, ConnectToSignal(_, _, _, _))
      .WillOnce([](const std::string& interface_name,
                   const std::string& signal_name,
                   dbus::ObjectProxy::SignalCallback signal_callback,
                   dbus::ObjectProxy::OnConnectedCallback on_connected) {
        std::move(on_connected).Run(interface_name, signal_name, true);

        auto signal = dbus::Signal(interface_name, signal_name);
        dbus::MessageWriter writer(&signal);
        dbus_utils::WriteValue(writer, false);
        dbus_utils::WriteValue(
            writer,
            dbus_utils::Variant::Wrap<"ao">(std::vector<dbus::ObjectPath>{
                dbus::ObjectPath(kCollectionPath)}));
        signal_callback.Run(&signal);
      });

  // OpenSession
  EXPECT_CALL(*mock_service_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretServiceInterface,
                   FreedesktopSecretKeyProvider::kMethodOpenSession, _, _))
      .WillOnce(RespondWith(dbus_utils::Variant::Wrap<"s">(""),
                            dbus::ObjectPath(kSessionPath)));

  // SearchItems returns empty
  EXPECT_CALL(*mock_collection_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretCollectionInterface,
                   FreedesktopSecretKeyProvider::kMethodSearchItems, _, _))
      .WillOnce(RespondWith(std::vector<dbus::ObjectPath>()));

  // CreateItem
  EXPECT_CALL(*mock_collection_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretCollectionInterface,
                   "CreateItem", _, _))
      .WillOnce(RespondWith(dbus::ObjectPath("/"),
                            dbus::ObjectPath(kItemPromptPath)));

  EXPECT_CALL(*mock_item_prompt_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretPromptInterface,
                   FreedesktopSecretKeyProvider::kMethodPrompt, _, _))
      .WillOnce(RespondWith());

  EXPECT_CALL(*mock_item_prompt_proxy, ConnectToSignal(_, _, _, _))
      .WillOnce([&](const std::string& interface_name,
                    const std::string& signal_name,
                    dbus::ObjectProxy::SignalCallback signal_callback,
                    dbus::ObjectProxy::OnConnectedCallback on_connected) {
        std::move(on_connected).Run(interface_name, signal_name, true);

        auto signal = dbus::Signal(interface_name, signal_name);
        dbus::MessageWriter writer(&signal);
        // Return a valid item path now
        dbus_utils::WriteValue(writer, false);
        dbus_utils::WriteValue(writer, dbus_utils::Variant::Wrap<"o">(
                                           dbus::ObjectPath(kItemPath)));
        signal_callback.Run(&signal);
      });

  // CloseSession
  EXPECT_CALL(*mock_session_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretSessionInterface,
                   FreedesktopSecretKeyProvider::kMethodClose, MatchArgs(), _))
      .WillOnce(RespondWith());

  FreedesktopSecretKeyProvider provider("gnome-libsecret", kProductName,
                                        mock_bus);
  std::string tag;
  std::optional<Encryptor::Key> key;
  provider.GetKey(base::BindLambdaForTesting(
      [&](const std::string& returned_tag,
          base::expected<Encryptor::Key, KeyProvider::KeyError> returned_key) {
        tag = returned_tag;
        key = std::move(returned_key.value());
      }));

  EXPECT_EQ(tag, "v11");
  EXPECT_TRUE(key.has_value());
}

TEST(FreedesktopSecretKeyProviderTest, KWallet) {
  auto mock_bus = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

  // Initialize object proxies
  auto mock_dbus_proxy = base::MakeRefCounted<MockObjectProxyWithTypedCalls>(
      mock_bus.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
  EXPECT_CALL(*mock_bus, GetObjectProxy(DBUS_SERVICE_DBUS,
                                        dbus::ObjectPath(DBUS_PATH_DBUS)))
      .WillRepeatedly(Return(mock_dbus_proxy.get()));

  auto mock_kwallet5_proxy =
      base::MakeRefCounted<MockObjectProxyWithTypedCalls>(
          mock_bus.get(), FreedesktopSecretKeyProvider::kKWalletD5Service,
          dbus::ObjectPath(FreedesktopSecretKeyProvider::kKWalletD5Path));
  EXPECT_CALL(*mock_bus,
              GetObjectProxy(FreedesktopSecretKeyProvider::kKWalletD5Service,
                             dbus::ObjectPath(
                                 FreedesktopSecretKeyProvider::kKWalletD5Path)))
      .WillRepeatedly(Return(mock_kwallet5_proxy.get()));
  EXPECT_CALL(
      *mock_dbus_proxy,
      CallWithoutError(
          DBUS_INTERFACE_DBUS, "NameHasOwner",
          MatchArgs(FreedesktopSecretKeyProvider::kKWalletD5Service), _))
      .WillOnce(RespondWithTrue);

  // isEnabled -> true
  EXPECT_CALL(*mock_kwallet5_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodIsEnabled,
                   MatchArgs(), _))
      .WillOnce(RespondWith(true));

  // networkWallet
  EXPECT_CALL(*mock_kwallet5_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodNetworkWallet,
                   MatchArgs(), _))
      .WillOnce(RespondWith(std::string(kNetworkWallet)));

  // ConnectToSignal walletAsyncOpened
  dbus::ObjectProxy::SignalCallback signal_callback;
  EXPECT_CALL(
      *mock_kwallet5_proxy,
      ConnectToSignal(
          FreedesktopSecretKeyProvider::kKWalletInterface,
          FreedesktopSecretKeyProvider::kKWalletSignalWalletAsyncOpened, _, _))
      .WillOnce([&](const std::string& interface_name,
                    const std::string& signal_name,
                    dbus::ObjectProxy::SignalCallback callback,
                    dbus::ObjectProxy::OnConnectedCallback on_connected) {
        std::move(on_connected).Run(interface_name, signal_name, true);
        signal_callback = std::move(callback);
      });

  // openAsync -> transaction ID
  EXPECT_CALL(*mock_kwallet5_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodOpenAsync,
                   MatchArgs(kNetworkWallet, static_cast<int64_t>(0),
                             kProductName, true),
                   _))
      .WillOnce(RespondWith(kTransactionId));

  // hasFolder -> true
  EXPECT_CALL(*mock_kwallet5_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodHasFolder,
                   MatchArgs(kKWalletHandle,
                             FreedesktopSecretKeyProvider::kKWalletFolder,
                             kProductName),
                   _))
      .WillOnce(RespondWith(true));

  // hasEntry -> true
  EXPECT_CALL(
      *mock_kwallet5_proxy,
      Call(FreedesktopSecretKeyProvider::kKWalletInterface,
           FreedesktopSecretKeyProvider::kKWalletMethodHasEntry,
           MatchArgs(kKWalletHandle,
                     FreedesktopSecretKeyProvider::kKWalletFolder,
                     FreedesktopSecretKeyProvider::kKeyName, kProductName),
           _))
      .WillOnce(RespondWith(true));

  // readPassword -> return a secret
  EXPECT_CALL(
      *mock_kwallet5_proxy,
      Call(FreedesktopSecretKeyProvider::kKWalletInterface,
           FreedesktopSecretKeyProvider::kKWalletMethodReadPassword,
           MatchArgs(kKWalletHandle,
                     FreedesktopSecretKeyProvider::kKWalletFolder,
                     FreedesktopSecretKeyProvider::kKeyName, kProductName),
           _))
      .WillOnce(RespondWith(std::string(kFakeSecret)));

  // close
  EXPECT_CALL(*mock_kwallet5_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodClose,
                   MatchArgs(kKWalletHandle, false, kProductName), _))
      .WillOnce(RespondWith(kKWalletHandle));

  FreedesktopSecretKeyProvider provider("kwallet5", kProductName, mock_bus);
  std::string tag;
  std::optional<Encryptor::Key> key;
  provider.GetKey(base::BindLambdaForTesting(
      [&](const std::string& returned_tag,
          base::expected<Encryptor::Key, KeyProvider::KeyError> returned_key) {
        tag = returned_tag;
        key = std::move(returned_key.value());
      }));

  // Simulate walletAsyncOpened signal
  ASSERT_TRUE(signal_callback);
  auto signal = dbus::Signal(
      FreedesktopSecretKeyProvider::kKWalletInterface,
      FreedesktopSecretKeyProvider::kKWalletSignalWalletAsyncOpened);
  dbus::MessageWriter writer(&signal);
  dbus_utils::WriteValue(writer, kTransactionId);
  dbus_utils::WriteValue(writer, kKWalletHandle);
  signal_callback.Run(&signal);

  EXPECT_EQ(tag, "v11");
  EXPECT_TRUE(key.has_value());
}

TEST(FreedesktopSecretKeyProviderTest, KWalletCreateFolderAndPassword) {
  auto mock_bus = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

  // Initialize object proxies
  auto mock_dbus_proxy = base::MakeRefCounted<MockObjectProxyWithTypedCalls>(
      mock_bus.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
  EXPECT_CALL(*mock_bus, GetObjectProxy(DBUS_SERVICE_DBUS,
                                        dbus::ObjectPath(DBUS_PATH_DBUS)))
      .WillRepeatedly(Return(mock_dbus_proxy.get()));

  auto mock_kwallet6_proxy =
      base::MakeRefCounted<MockObjectProxyWithTypedCalls>(
          mock_bus.get(), FreedesktopSecretKeyProvider::kKWalletD6Service,
          dbus::ObjectPath(FreedesktopSecretKeyProvider::kKWalletD6Path));
  EXPECT_CALL(*mock_bus,
              GetObjectProxy(FreedesktopSecretKeyProvider::kKWalletD6Service,
                             dbus::ObjectPath(
                                 FreedesktopSecretKeyProvider::kKWalletD6Path)))
      .WillRepeatedly(Return(mock_kwallet6_proxy.get()));
  EXPECT_CALL(
      *mock_dbus_proxy,
      CallWithoutError(
          DBUS_INTERFACE_DBUS, "NameHasOwner",
          MatchArgs(FreedesktopSecretKeyProvider::kKWalletD6Service), _))
      .WillOnce(RespondWithTrue);

  // isEnabled -> true
  EXPECT_CALL(*mock_kwallet6_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodIsEnabled,
                   MatchArgs(), _))
      .WillOnce(RespondWith(true));

  // networkWallet
  EXPECT_CALL(*mock_kwallet6_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodNetworkWallet,
                   MatchArgs(), _))
      .WillOnce(RespondWith(std::string(kNetworkWallet)));

  // ConnectToSignal walletAsyncOpened
  dbus::ObjectProxy::SignalCallback signal_callback;
  EXPECT_CALL(
      *mock_kwallet6_proxy,
      ConnectToSignal(
          FreedesktopSecretKeyProvider::kKWalletInterface,
          FreedesktopSecretKeyProvider::kKWalletSignalWalletAsyncOpened, _, _))
      .WillOnce([&](const std::string& interface_name,
                    const std::string& signal_name,
                    dbus::ObjectProxy::SignalCallback callback,
                    dbus::ObjectProxy::OnConnectedCallback on_connected) {
        std::move(on_connected).Run(interface_name, signal_name, true);
        signal_callback = std::move(callback);
      });

  // openAsync -> transaction ID
  EXPECT_CALL(*mock_kwallet6_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodOpenAsync,
                   MatchArgs(kNetworkWallet, static_cast<int64_t>(0),
                             kProductName, true),
                   _))
      .WillOnce(RespondWith(kTransactionId));

  // hasFolder -> false
  EXPECT_CALL(*mock_kwallet6_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodHasFolder,
                   MatchArgs(kKWalletHandle,
                             FreedesktopSecretKeyProvider::kKWalletFolder,
                             kProductName),
                   _))
      .WillOnce(RespondWith(false));

  // createFolder
  EXPECT_CALL(*mock_kwallet6_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodCreateFolder,
                   MatchArgs(kKWalletHandle,
                             FreedesktopSecretKeyProvider::kKWalletFolder,
                             kProductName),
                   _))
      .WillOnce(RespondWith(true));

  // writePassword
  EXPECT_CALL(
      *mock_kwallet6_proxy,
      Call(FreedesktopSecretKeyProvider::kKWalletInterface,
           FreedesktopSecretKeyProvider::kKWalletMethodWritePassword, _, _))
      .WillOnce(RespondWith(0));

  // close
  EXPECT_CALL(*mock_kwallet6_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodClose,
                   MatchArgs(kKWalletHandle, false, kProductName), _))
      .WillOnce(RespondWith(kKWalletHandle));

  FreedesktopSecretKeyProvider provider("kwallet6", kProductName, mock_bus);
  std::string tag;
  std::optional<Encryptor::Key> key;
  provider.GetKey(base::BindLambdaForTesting(
      [&](const std::string& returned_tag,
          base::expected<Encryptor::Key, KeyProvider::KeyError> returned_key) {
        tag = returned_tag;
        key = std::move(returned_key.value());
      }));

  // Simulate walletAsyncOpened signal
  ASSERT_TRUE(signal_callback);
  auto signal = dbus::Signal(
      FreedesktopSecretKeyProvider::kKWalletInterface,
      FreedesktopSecretKeyProvider::kKWalletSignalWalletAsyncOpened);
  dbus::MessageWriter writer(&signal);
  dbus_utils::WriteValue(writer, kTransactionId);
  dbus_utils::WriteValue(writer, kKWalletHandle);
  signal_callback.Run(&signal);

  EXPECT_EQ(tag, "v11");
  EXPECT_TRUE(key.has_value());
}

}  // namespace os_crypt_async
