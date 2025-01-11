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
#include "crypto/encryptor.h"
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

  FreedesktopSecretKeyProvider provider(/*use_for_encryption=*/true,
                                        kProductName, mock_bus);
  std::string tag;
  std::optional<Encryptor::Key> key;
  provider.GetKey(base::BindLambdaForTesting(
      [&](const std::string& returned_tag,
          std::optional<Encryptor::Key> returned_key) {
        tag = returned_tag;
        key = std::move(returned_key);
      }));
  EXPECT_EQ(tag, "v11");
  EXPECT_TRUE(key.has_value());
}

}  // namespace os_crypt_async
