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
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
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
              .WillOnce([&](const std::string& interface_name,
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

    mock_secret_service_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), SecretPortalKeyProvider::kSecretServiceName,
        dbus::ObjectPath(SecretPortalKeyProvider::kSecretServicePath));
    EXPECT_CALL(
        *mock_bus_,
        GetObjectProxy(
            SecretPortalKeyProvider::kSecretServiceName,
            dbus::ObjectPath(SecretPortalKeyProvider::kSecretServicePath)))
        .WillRepeatedly(Return(mock_secret_service_proxy_.get()));

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
  void ExpectSecretServiceHasOwner(bool has_owner) {
    EXPECT_CALL(
        *mock_dbus_proxy_,
        CallMethod(MatchMethod(DBUS_INTERFACE_DBUS, "NameHasOwner"), _, _))
        .WillOnce([has_owner](dbus::MethodCall* method_call, int timeout_ms,
                              dbus::ObjectProxy::ResponseCallback callback) {
          dbus::MessageReader reader(method_call);
          std::string name;
          EXPECT_TRUE(reader.PopString(&name));
          EXPECT_EQ(name, SecretPortalKeyProvider::kSecretServiceName);
          auto response = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          writer.AppendBool(has_owner);
          std::move(callback).Run(response.get());
        });
  }

  // Expects the Secret Service to be probed, and replies if `reply` is true or
  // times out otherwise.
  void ExpectSecretServiceProbe(bool reply) {
    EXPECT_CALL(*mock_secret_service_proxy_,
                CallMethodWithErrorResponse(
                    MatchMethod(SecretPortalKeyProvider::kPropertiesInterface,
                                SecretPortalKeyProvider::kMethodGet),
                    _, _))
        .WillOnce([reply](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseOrErrorCallback callback) {
          EXPECT_EQ(timeout_ms,
                    SecretPortalKeyProvider::kSecretServiceProbeTimeout
                        .InMilliseconds());
          if (!reply) {
            method_call->SetSerial(1);
            auto error = dbus::ErrorResponse::FromMethodCall(
                method_call, DBUS_ERROR_NO_REPLY, "Did not receive a reply.");
            std::move(callback).Run(nullptr, error.get());
            return;
          }
          auto response = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          dbus_utils::WriteValue(writer, dbus_utils::Variant::Wrap<"ao">(
                                             std::vector<dbus::ObjectPath>()));
          std::move(callback).Run(response.get(), nullptr);
        });
  }

  void ExpectRetrieveSecret() {
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
  }

  // Calls GetKey() and waits for the result.
  std::optional<base::expected<Encryptor::Key, KeyProvider::KeyError>>
  GetKeyAndWait() {
    std::optional<base::expected<Encryptor::Key, KeyProvider::KeyError>> result;
    key_provider_->GetKey(base::BindLambdaForTesting(
        [&](const std::string& tag,
            base::expected<Encryptor::Key, KeyProvider::KeyError> key) {
          EXPECT_EQ(tag, SecretPortalKeyProvider::kKeyTag);
          result = std::move(key);
        }));
    EXPECT_TRUE(base::test::RunUntil([&]() { return result.has_value(); }));
    return result;
  }

  // An IO thread is required for FileDescriptorWatcher.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_dbus_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_response_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_secret_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_secret_service_proxy_;
  dbus::ObjectPath response_path_;
  std::unique_ptr<SecretPortalKeyProvider> key_provider_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(SecretPortalKeyProviderTest, GetKey) {
  ExpectSecretServiceHasOwner(true);
  ExpectSecretServiceProbe(/*reply=*/true);
  ExpectRetrieveSecret();

  auto result = GetKeyAndWait();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->has_value());

  EXPECT_TRUE(pref_service_.HasPrefPath(
      SecretPortalKeyProvider::kOsCryptTokenPrefName));
  EXPECT_EQ(
      pref_service_.GetString(SecretPortalKeyProvider::kOsCryptTokenPrefName),
      kPrefToken);
}

// Without a running Secret Service, there is nothing to probe, and
// xdg-desktop-portal starts the portal backend if needed.
TEST_F(SecretPortalKeyProviderTest, GetKeyWithoutSecretService) {
  ExpectSecretServiceHasOwner(false);
  EXPECT_CALL(*mock_secret_service_proxy_, CallMethodWithErrorResponse(_, _, _))
      .Times(0);
  ExpectRetrieveSecret();

  auto result = GetKeyAndWait();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->has_value());
}

// An unresponsive Secret Service makes the key temporarily unavailable
// without calling RetrieveSecret, which could otherwise wait forever.
TEST_F(SecretPortalKeyProviderTest, SecretServiceUnresponsive) {
  base::HistogramTester histogram_tester;
  ExpectSecretServiceHasOwner(true);
  ExpectSecretServiceProbe(/*reply=*/false);
  EXPECT_CALL(*mock_secret_proxy_, CallMethodWithErrorResponse(_, _, _))
      .Times(0);

  auto result = GetKeyAndWait();
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(result->has_value());
  EXPECT_EQ(result->error(), KeyProvider::KeyError::kTemporarilyUnavailable);
  histogram_tester.ExpectUniqueSample(
      SecretPortalKeyProvider::kUmaInitStatusEnum,
      SecretPortalKeyProvider::InitStatus::kBackendUnresponsive, 1);
}

}  // namespace os_crypt_async
