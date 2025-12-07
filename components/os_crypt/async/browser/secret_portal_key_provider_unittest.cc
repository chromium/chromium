// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/secret_portal_key_provider.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/nix/xdg_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/dbus/utils/name_has_owner.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/utils/write_value.h"
#include "components/dbus/xdg/portal.h"
#include "components/prefs/testing_pref_service.h"
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

constexpr char kBusName[] = ":1.123";
constexpr char kSecret[] = "secret_for_testing";
constexpr char kPrefToken[] = "token_for_testing";

MATCHER_P2(MatchMethod, interface, member, "") {
  return arg->GetInterface() == interface && arg->GetMember() == member;
}

}  // namespace

class SecretPortalKeyProviderTest : public testing::Test {
 public:
  SecretPortalKeyProviderTest() = default;

  void SetUp() override {
    dbus_xdg::SetPortalStateForTesting(
        dbus_xdg::PortalRegistrarState::kSuccess);
    SecretPortalKeyProvider::RegisterLocalPrefs(pref_service_.registry());

    mock_bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

    mock_dbus_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
    EXPECT_CALL(*mock_bus_, GetObjectProxy(DBUS_SERVICE_DBUS,
                                           dbus::ObjectPath(DBUS_PATH_DBUS)))
        .WillRepeatedly(Return(mock_dbus_proxy_.get()));

    EXPECT_CALL(*mock_bus_, AssertOnOriginThread()).WillRepeatedly([] {});

    EXPECT_CALL(*mock_bus_, GetDBusTaskRunner())
        .WillRepeatedly(
            Return(task_environment_.GetMainThreadTaskRunner().get()));

    EXPECT_CALL(*mock_bus_,
                GetObjectProxy(SecretPortalKeyProvider::kServiceSecret, _))
        .WillRepeatedly([&](std::string_view service_name,
                            const dbus::ObjectPath& object_path) {
          response_path_ = object_path;
          mock_response_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
              mock_bus_.get(), SecretPortalKeyProvider::kServiceSecret,
              response_path_);
          EXPECT_CALL(*mock_response_proxy_, ConnectToSignal(_, _, _, _))
              .WillOnce(
                  [&](const std::string& interface_name,
                      const std::string& signal_name,
                      dbus::ObjectProxy::SignalCallback signal_callback,
                      dbus::ObjectProxy::OnConnectedCallback
                          on_connected_callback) {
                    EXPECT_EQ(interface_name,
                              SecretPortalKeyProvider::kInterfaceRequest);
                    EXPECT_EQ(signal_name,
                              SecretPortalKeyProvider::kSignalResponse);

                    std::move(on_connected_callback)
                        .Run(interface_name, signal_name, true);

                    dbus::Signal signal(interface_name, signal_name);
                    dbus::MessageWriter writer(&signal);
                    constexpr uint32_t kResponseSuccess = 0;
                    writer.AppendUint32(kResponseSuccess);
                    std::map<std::string, dbus_utils::Variant> dict;
                    dict.emplace("token",
                                 dbus_utils::Variant::Wrap<"s">(kPrefToken));
                    dbus_utils::WriteValue(writer, dict);
                    signal_callback.Run(&signal);
                  });
          return mock_response_proxy_.get();
        });

    mock_secret_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), SecretPortalKeyProvider::kServiceSecret,
        dbus::ObjectPath(SecretPortalKeyProvider::kObjectPathSecret));
    EXPECT_CALL(*mock_bus_,
                GetObjectProxy(SecretPortalKeyProvider::kServiceSecret,
                               dbus::ObjectPath(
                                   SecretPortalKeyProvider::kObjectPathSecret)))
        .WillRepeatedly(Return(mock_secret_proxy_.get()));

    key_provider_ = base::WrapUnique(
        new SecretPortalKeyProvider(&pref_service_, mock_bus_, true));
  }

  void TearDown() override {
    // Shutdown the bus to ensure clean-up
    key_provider_.reset();

    base::RunLoop run_loop;
    run_loop.RunUntilIdle();

    dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kIdle);
  }

 protected:
  // An IO thread is required for FileDescriptorWatcher.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_dbus_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_response_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_secret_proxy_;
  dbus::ObjectPath response_path_;
  std::unique_ptr<SecretPortalKeyProvider> key_provider_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(SecretPortalKeyProviderTest, GetKey) {
  EXPECT_CALL(*mock_bus_, GetConnectionName()).WillOnce(Return(kBusName));

  EXPECT_CALL(*mock_secret_proxy_,
              CallMethodWithErrorResponse(
                  MatchMethod(SecretPortalKeyProvider::kInterfaceSecret,
                              SecretPortalKeyProvider::kMethodRetrieveSecret),
                  _, _))
      .WillOnce([&](dbus::MethodCall* method_call, int timeout_ms,
                    dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        dbus::MessageReader reader(method_call);
        base::ScopedFD write_fd;
        EXPECT_TRUE(reader.PopFileDescriptor(&write_fd));
        EXPECT_EQ(write(write_fd.get(), kSecret, sizeof(kSecret)),
                  static_cast<ssize_t>(sizeof(kSecret)));
        write_fd.reset();

        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendObjectPath(dbus::ObjectPath(response_path_));
        std::move(callback).Run(response.get(), nullptr);
      });

  bool callback_called = false;
  std::string key_tag;
  std::optional<Encryptor::Key> key;
  key_provider_->GetKey(base::BindOnce(
      [](bool& callback_called, std::string& key_tag,
         std::optional<Encryptor::Key>& key,
         const std::string& returned_key_tag,
         base::expected<Encryptor::Key, KeyProvider::KeyError> returned_key) {
        callback_called = true;
        key_tag = returned_key_tag;
        key = std::move(returned_key.value());
      },
      std::ref(callback_called), std::ref(key_tag), std::ref(key)));
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(key_tag, SecretPortalKeyProvider::kKeyTag);
  EXPECT_TRUE(key.has_value());

  EXPECT_TRUE(pref_service_.HasPrefPath(
      SecretPortalKeyProvider::kOsCryptTokenPrefName));
  EXPECT_EQ(
      pref_service_.GetString(SecretPortalKeyProvider::kOsCryptTokenPrefName),
      kPrefToken);
}

}  // namespace os_crypt_async
