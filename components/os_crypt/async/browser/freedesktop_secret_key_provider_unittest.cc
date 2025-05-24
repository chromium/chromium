// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/freedesktop_secret_key_provider.h"

#include <memory>
#include <optional>
#include <string>

#include "base/files/scoped_file.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "build/branding_buildflags.h"
#include "components/dbus/properties/types.h"
#include "components/dbus/utils/name_has_owner.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;

namespace os_crypt_async {

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

constexpr char kFakeSecret[] = "c3VwZXJfc2VjcmV0X2tleQ==";

template <typename T>
class MatchArgs {
 public:
  using is_gtest_matcher = void;

  explicit MatchArgs(T&& args) : args_(std::forward<T>(args)) {}

  bool MatchAndExplain(const DbusVariant& match, std::ostream*) const {
    const T* match_args = match.GetAs<T>();
    return match_args && *match_args == args_;
  }

  void DescribeTo(std::ostream* os) const { *os << "DbusTypes match"; }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "DbusTypes mismatch";
  }

 private:
  T args_;
};

template <typename T>
auto RespondWith(T&& args) {
  return [args = std::move(args)](
             const std::string&, const std::string&, DbusVariant,
             dbus::ObjectProxy::ResponseCallback* callback) {
    auto response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    args.Write(&writer);
    std::move(*callback).Run(response.get());
  };
}

class MockObjectProxyWithTypedCalls : public dbus::MockObjectProxy {
 public:
  MockObjectProxyWithTypedCalls(dbus::Bus* bus,
                                const std::string& service_name,
                                const dbus::ObjectPath& object_path)
      : dbus::MockObjectProxy(bus, service_name, object_path) {
    // Forward to Call().
    EXPECT_CALL(*this, DoCallMethod(_, _, _))
        .Times(AtLeast(0))
        .WillRepeatedly([this](dbus::MethodCall* method_call, int timeout_ms,
                               dbus::ObjectProxy::ResponseCallback* callback) {
          dbus::MessageReader reader(method_call);
          auto args = ReadDbusMessage(&reader);
          Call(method_call->GetInterface(), method_call->GetMember(),
               std::move(args), callback);
        });
  }

  MOCK_METHOD4(Call,
               void(const std::string& interface,
                    const std::string& method_name,
                    DbusVariant args,
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
  EXPECT_CALL(*mock_dbus_proxy,
              Call(DBUS_INTERFACE_DBUS, "NameHasOwner",
                   MatchArgs(DbusString(
                       FreedesktopSecretKeyProvider::kSecretServiceName)),
                   _))
      .WillOnce(RespondWith(DbusBoolean(true)));

  // ReadAlias("default")
  EXPECT_CALL(
      *mock_service_proxy,
      Call(FreedesktopSecretKeyProvider::kSecretServiceInterface,
           FreedesktopSecretKeyProvider::kMethodReadAlias,
           MatchArgs(DbusString(FreedesktopSecretKeyProvider::kDefaultAlias)),
           _))
      .WillOnce(RespondWith(DbusObjectPath(dbus::ObjectPath(kCollectionPath))));

  // Get(Label)
  EXPECT_CALL(
      *mock_collection_proxy,
      Call(FreedesktopSecretKeyProvider::kPropertiesInterface,
           FreedesktopSecretKeyProvider::kMethodGet,
           MatchArgs(MakeDbusParameters(
               DbusString(
                   FreedesktopSecretKeyProvider::kSecretCollectionInterface),
               DbusString(FreedesktopSecretKeyProvider::kLabelProperty))),
           _))
      .WillOnce(RespondWith(MakeDbusVariant(
          DbusString(FreedesktopSecretKeyProvider::kDefaultCollectionLabel))));

  // Unlock(default_collection)
  EXPECT_CALL(*mock_service_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretServiceInterface,
                   FreedesktopSecretKeyProvider::kMethodUnlock,
                   MatchArgs(MakeDbusArray(
                       DbusObjectPath(dbus::ObjectPath(kCollectionPath)))),
                   _))
      .WillOnce(RespondWith(MakeDbusParameters(
          MakeDbusArray(DbusObjectPath(dbus::ObjectPath(kCollectionPath))),
          DbusObjectPath(dbus::ObjectPath("/")))));

  // OpenSession
  EXPECT_CALL(
      *mock_service_proxy,
      Call(FreedesktopSecretKeyProvider::kSecretServiceInterface,
           FreedesktopSecretKeyProvider::kMethodOpenSession,
           MatchArgs(MakeDbusParameters(
               DbusString(FreedesktopSecretKeyProvider::kAlgorithmPlain),
               MakeDbusVariant(
                   DbusString(FreedesktopSecretKeyProvider::kInputPlain)))),
           _))
      .WillOnce(RespondWith(
          MakeDbusParameters(MakeDbusVariant(DbusString("")),
                             DbusObjectPath(dbus::ObjectPath(kSessionPath)))));

  // SearchItems
  EXPECT_CALL(
      *mock_collection_proxy,
      Call(FreedesktopSecretKeyProvider::kSecretCollectionInterface,
           FreedesktopSecretKeyProvider::kMethodSearchItems,
           MatchArgs(MakeDbusArray(MakeDbusDictEntry(
               DbusString(
                   FreedesktopSecretKeyProvider::kApplicationAttributeKey),
               DbusString(FreedesktopSecretKeyProvider::kAppName)))),
           _))
      .WillOnce(RespondWith(
          MakeDbusArray(DbusObjectPath(dbus::ObjectPath(kItemPath)))));

  // GetSecret
  EXPECT_CALL(
      *mock_item_proxy,
      Call(FreedesktopSecretKeyProvider::kSecretItemInterface,
           FreedesktopSecretKeyProvider::kMethodGetSecret,
           MatchArgs(DbusObjectPath(dbus::ObjectPath(kSessionPath))), _))
      .WillOnce(RespondWith(MakeDbusStruct(
          DbusObjectPath(dbus::ObjectPath(kSessionPath)),
          DbusByteArray(base::MakeRefCounted<base::RefCountedString>("")),
          DbusByteArray(
              base::MakeRefCounted<base::RefCountedString>(kFakeSecret)),
          DbusString(FreedesktopSecretKeyProvider::kMimePlain))));

  // Close
  EXPECT_CALL(*mock_session_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretSessionInterface,
                   FreedesktopSecretKeyProvider::kMethodClose,
                   MatchArgs(DbusVoid()), _))
      .WillOnce(RespondWith(DbusVoid()));

  FreedesktopSecretKeyProvider provider(
      "gnome-libsecret", /*use_for_encryption=*/true, kProductName, mock_bus);
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
  EXPECT_CALL(*mock_dbus_proxy,
              Call(DBUS_INTERFACE_DBUS, "NameHasOwner",
                   MatchArgs(DbusString(
                       FreedesktopSecretKeyProvider::kSecretServiceName)),
                   _))
      .WillOnce(RespondWith(DbusBoolean(true)));

  // ReadAlias("default") returns no default collection
  EXPECT_CALL(
      *mock_service_proxy,
      Call(FreedesktopSecretKeyProvider::kSecretServiceInterface,
           FreedesktopSecretKeyProvider::kMethodReadAlias,
           MatchArgs(DbusString(FreedesktopSecretKeyProvider::kDefaultAlias)),
           _))
      .WillOnce(RespondWith(DbusObjectPath(dbus::ObjectPath("/"))));

  // CreateCollection returns a prompt
  EXPECT_CALL(*mock_service_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretServiceInterface,
                   "CreateCollection", _, _))
      .WillOnce(RespondWith(MakeDbusParameters(
          DbusObjectPath(dbus::ObjectPath("/")),
          DbusObjectPath(dbus::ObjectPath(kCollectionPromptPath)))));

  EXPECT_CALL(*mock_collection_prompt_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretPromptInterface,
                   FreedesktopSecretKeyProvider::kMethodPrompt, _, _))
      .WillOnce(RespondWith(DbusVoid()));

  EXPECT_CALL(*mock_collection_prompt_proxy, DoConnectToSignal(_, _, _, _))
      .WillOnce([](const std::string& interface_name,
                   const std::string& signal_name,
                   dbus::ObjectProxy::SignalCallback signal_callback,
                   dbus::ObjectProxy::OnConnectedCallback* on_connected) {
        // Connected successfully
        std::move(*on_connected).Run(interface_name, signal_name, true);

        // Trigger the signal callback with a non-empty collection path now
        auto signal = dbus::Signal(interface_name, signal_name);
        dbus::MessageWriter writer(&signal);
        // Prompt completed: dismissed = false, return the newly created
        // collection path
        DbusParameters<DbusBoolean, DbusVariant> args(
            DbusBoolean(false),
            MakeDbusVariant(DbusObjectPath(dbus::ObjectPath(kCollectionPath))));
        args.Write(&writer);
        signal_callback.Run(&signal);
      });

  // Unlock collection returns a prompt
  EXPECT_CALL(*mock_service_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretServiceInterface,
                   FreedesktopSecretKeyProvider::kMethodUnlock,
                   MatchArgs(MakeDbusArray(
                       DbusObjectPath(dbus::ObjectPath(kCollectionPath)))),
                   _))
      .WillOnce(RespondWith(MakeDbusParameters(
          DbusArray<DbusObjectPath>(),
          DbusObjectPath(dbus::ObjectPath(kUnlockPromptPath)))));

  EXPECT_CALL(*mock_unlock_prompt_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretPromptInterface,
                   FreedesktopSecretKeyProvider::kMethodPrompt, _, _))
      .WillOnce(RespondWith(DbusVoid()));

  EXPECT_CALL(*mock_unlock_prompt_proxy, DoConnectToSignal(_, _, _, _))
      .WillOnce([](const std::string& interface_name,
                   const std::string& signal_name,
                   dbus::ObjectProxy::SignalCallback signal_callback,
                   dbus::ObjectProxy::OnConnectedCallback* on_connected) {
        std::move(*on_connected).Run(interface_name, signal_name, true);

        auto signal = dbus::Signal(interface_name, signal_name);
        dbus::MessageWriter writer(&signal);
        DbusParameters<DbusBoolean, DbusVariant> args(
            DbusBoolean(false), MakeDbusVariant(MakeDbusArray(DbusObjectPath(
                                    dbus::ObjectPath(kCollectionPath)))));
        args.Write(&writer);
        signal_callback.Run(&signal);
      });

  // OpenSession
  EXPECT_CALL(*mock_service_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretServiceInterface,
                   FreedesktopSecretKeyProvider::kMethodOpenSession, _, _))
      .WillOnce(RespondWith(
          MakeDbusParameters(MakeDbusVariant(DbusString("")),
                             DbusObjectPath(dbus::ObjectPath(kSessionPath)))));

  // SearchItems returns empty
  EXPECT_CALL(*mock_collection_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretCollectionInterface,
                   FreedesktopSecretKeyProvider::kMethodSearchItems, _, _))
      .WillOnce(RespondWith(DbusArray<DbusObjectPath>()));

  // CreateItem
  EXPECT_CALL(*mock_collection_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretCollectionInterface,
                   "CreateItem", _, _))
      .WillOnce(RespondWith(MakeDbusParameters(
          DbusObjectPath(dbus::ObjectPath("/")),
          DbusObjectPath(dbus::ObjectPath(kItemPromptPath)))));

  EXPECT_CALL(*mock_item_prompt_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretPromptInterface,
                   FreedesktopSecretKeyProvider::kMethodPrompt, _, _))
      .WillOnce(RespondWith(DbusVoid()));

  EXPECT_CALL(*mock_item_prompt_proxy, DoConnectToSignal(_, _, _, _))
      .WillOnce([&](const std::string& interface_name,
                    const std::string& signal_name,
                    dbus::ObjectProxy::SignalCallback signal_callback,
                    dbus::ObjectProxy::OnConnectedCallback* on_connected) {
        std::move(*on_connected).Run(interface_name, signal_name, true);

        auto signal = dbus::Signal(interface_name, signal_name);
        dbus::MessageWriter writer(&signal);
        // Return a valid item path now
        DbusParameters<DbusBoolean, DbusVariant> args(
            DbusBoolean(false),
            MakeDbusVariant(DbusObjectPath(dbus::ObjectPath(kItemPath))));
        args.Write(&writer);
        signal_callback.Run(&signal);
      });

  // CloseSession
  EXPECT_CALL(*mock_session_proxy,
              Call(FreedesktopSecretKeyProvider::kSecretSessionInterface,
                   FreedesktopSecretKeyProvider::kMethodClose,
                   MatchArgs(DbusVoid()), _))
      .WillOnce(RespondWith(DbusVoid()));

  FreedesktopSecretKeyProvider provider(
      "gnome-libsecret", /*use_for_encryption=*/true, kProductName, mock_bus);
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
  EXPECT_CALL(*mock_dbus_proxy,
              Call(DBUS_INTERFACE_DBUS, "NameHasOwner",
                   MatchArgs(DbusString(
                       FreedesktopSecretKeyProvider::kKWalletD5Service)),
                   _))
      .WillOnce(RespondWith(DbusBoolean(true)));

  // isEnabled -> true
  EXPECT_CALL(*mock_kwallet5_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodIsEnabled,
                   MatchArgs(DbusVoid()), _))
      .WillOnce(RespondWith(DbusBoolean(true)));

  // networkWallet
  EXPECT_CALL(*mock_kwallet5_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodNetworkWallet,
                   MatchArgs(DbusVoid()), _))
      .WillOnce(RespondWith(DbusString(kNetworkWallet)));

  // open -> non-negative handle
  EXPECT_CALL(*mock_kwallet5_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodOpen,
                   MatchArgs(MakeDbusParameters(DbusString(kNetworkWallet),
                                                DbusInt64(0),
                                                DbusString(kProductName))),
                   _))
      .WillOnce(RespondWith(DbusInt32(kKWalletHandle)));

  // hasFolder -> true
  EXPECT_CALL(*mock_kwallet5_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodHasFolder,
                   MatchArgs(MakeDbusParameters(
                       DbusInt32(kKWalletHandle),
                       DbusString(FreedesktopSecretKeyProvider::kKWalletFolder),
                       DbusString(kProductName))),
                   _))
      .WillOnce(RespondWith(DbusBoolean(true)));

  // hasEntry -> true
  EXPECT_CALL(*mock_kwallet5_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodHasEntry,
                   MatchArgs(MakeDbusParameters(
                       DbusInt32(kKWalletHandle),
                       DbusString(FreedesktopSecretKeyProvider::kKWalletFolder),
                       DbusString(FreedesktopSecretKeyProvider::kKeyName),
                       DbusString(kProductName))),
                   _))
      .WillOnce(RespondWith(DbusBoolean(true)));

  // readPassword -> return a secret
  EXPECT_CALL(*mock_kwallet5_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodReadPassword,
                   MatchArgs(MakeDbusParameters(
                       DbusInt32(kKWalletHandle),
                       DbusString(FreedesktopSecretKeyProvider::kKWalletFolder),
                       DbusString(FreedesktopSecretKeyProvider::kKeyName),
                       DbusString(kProductName))),
                   _))
      .WillOnce(RespondWith(DbusString(kFakeSecret)));

  // close
  EXPECT_CALL(*mock_kwallet5_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodClose,
                   MatchArgs(MakeDbusParameters(DbusInt32(kKWalletHandle),
                                                DbusBoolean(false),
                                                DbusString(kProductName))),
                   _))
      .WillOnce(RespondWith(DbusInt32(kKWalletHandle)));

  FreedesktopSecretKeyProvider provider("kwallet5", /*use_for_encryption=*/true,
                                        kProductName, mock_bus);
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
  EXPECT_CALL(*mock_dbus_proxy,
              Call(DBUS_INTERFACE_DBUS, "NameHasOwner",
                   MatchArgs(DbusString(
                       FreedesktopSecretKeyProvider::kKWalletD6Service)),
                   _))
      .WillOnce(RespondWith(DbusBoolean(true)));

  // isEnabled -> true
  EXPECT_CALL(*mock_kwallet6_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodIsEnabled,
                   MatchArgs(DbusVoid()), _))
      .WillOnce(RespondWith(DbusBoolean(true)));

  // networkWallet
  EXPECT_CALL(*mock_kwallet6_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodNetworkWallet,
                   MatchArgs(DbusVoid()), _))
      .WillOnce(RespondWith(DbusString(kNetworkWallet)));

  // open -> non-negative handle
  EXPECT_CALL(*mock_kwallet6_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodOpen,
                   MatchArgs(MakeDbusParameters(DbusString(kNetworkWallet),
                                                DbusInt64(0),
                                                DbusString(kProductName))),
                   _))
      .WillOnce(RespondWith(DbusInt32(kKWalletHandle)));

  // hasFolder -> false
  EXPECT_CALL(*mock_kwallet6_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodHasFolder,
                   MatchArgs(MakeDbusParameters(
                       DbusInt32(kKWalletHandle),
                       DbusString(FreedesktopSecretKeyProvider::kKWalletFolder),
                       DbusString(kProductName))),
                   _))
      .WillOnce(RespondWith(DbusBoolean(false)));

  // createFolder
  EXPECT_CALL(*mock_kwallet6_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodCreateFolder,
                   MatchArgs(MakeDbusParameters(
                       DbusInt32(kKWalletHandle),
                       DbusString(FreedesktopSecretKeyProvider::kKWalletFolder),
                       DbusString(kProductName))),
                   _))
      .WillOnce(RespondWith(DbusBoolean(true)));

  // writePassword
  EXPECT_CALL(
      *mock_kwallet6_proxy,
      Call(FreedesktopSecretKeyProvider::kKWalletInterface,
           FreedesktopSecretKeyProvider::kKWalletMethodWritePassword, _, _))
      .WillOnce(RespondWith(DbusInt32(0)));

  // close
  EXPECT_CALL(*mock_kwallet6_proxy,
              Call(FreedesktopSecretKeyProvider::kKWalletInterface,
                   FreedesktopSecretKeyProvider::kKWalletMethodClose,
                   MatchArgs(MakeDbusParameters(DbusInt32(kKWalletHandle),
                                                DbusBoolean(false),
                                                DbusString(kProductName))),
                   _))
      .WillOnce(RespondWith(DbusInt32(kKWalletHandle)));

  FreedesktopSecretKeyProvider provider("kwallet6", /*use_for_encryption=*/true,
                                        kProductName, mock_bus);
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

}  // namespace os_crypt_async
