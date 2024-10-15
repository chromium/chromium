// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/utils/check_for_service_and_start.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace dbus_utils {
namespace {

MATCHER_P2(MatchMethod, interface, member, "") {
  return arg->GetInterface() == interface && arg->GetMember() == member;
}

class CheckForServiceAndStartTest : public testing::Test {
 public:
  void SetUp() override {
    mock_bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

    mock_dbus_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

    EXPECT_CALL(*mock_bus_, GetObjectProxy(DBUS_SERVICE_DBUS,
                                           dbus::ObjectPath(DBUS_PATH_DBUS)))
        .WillRepeatedly(Return(mock_dbus_proxy_.get()));
  }

  void TearDown() override {
    mock_bus_.reset();
    mock_dbus_proxy_.reset();
  }

 protected:
  void ExpectNameHasOwner(const std::string& service_name,
                          bool has_owner,
                          bool error = false) {
    EXPECT_CALL(
        *mock_dbus_proxy_,
        DoCallMethod(MatchMethod(DBUS_INTERFACE_DBUS, "NameHasOwner"), _, _))
        .WillOnce(Invoke([this, service_name, has_owner, error](
                             dbus::MethodCall* method_call, int timeout_ms,
                             dbus::ObjectProxy::ResponseCallback* callback) {
          dbus::MessageReader reader(method_call);
          std::string received_service_name;
          EXPECT_TRUE(reader.PopString(&received_service_name));
          EXPECT_EQ(received_service_name, service_name);

          if (error) {
            // Simulate an error by calling the callback with a null response
            task_environment_.GetMainThreadTaskRunner()->PostTask(
                FROM_HERE, base::BindOnce(std::move(*callback), nullptr));
          } else {
            response_ = dbus::Response::CreateEmpty();
            dbus::MessageWriter writer(response_.get());
            writer.AppendBool(has_owner);
            task_environment_.GetMainThreadTaskRunner()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(*callback), response_.get()));
          }
        }));
  }

  void ExpectListActivatableNames(
      const std::vector<std::string>& activatable_names) {
    EXPECT_CALL(
        *mock_dbus_proxy_,
        DoCallMethod(MatchMethod(DBUS_INTERFACE_DBUS, "ListActivatableNames"),
                     _, _))
        .WillOnce(Invoke([this, activatable_names](
                             dbus::MethodCall* method_call, int timeout_ms,
                             dbus::ObjectProxy::ResponseCallback* callback) {
          response_ = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response_.get());
          writer.AppendArrayOfStrings(activatable_names);
          task_environment_.GetMainThreadTaskRunner()->PostTask(
              FROM_HERE, base::BindOnce(std::move(*callback), response_.get()));
        }));
  }

  void ExpectStartServiceByName(const std::string& service_name,
                                uint32_t reply_code) {
    EXPECT_CALL(
        *mock_dbus_proxy_,
        DoCallMethod(MatchMethod(DBUS_INTERFACE_DBUS, "StartServiceByName"), _,
                     _))
        .WillOnce(Invoke([this, service_name, reply_code](
                             dbus::MethodCall* method_call, int timeout_ms,
                             dbus::ObjectProxy::ResponseCallback* callback) {
          dbus::MessageReader reader(method_call);
          std::string received_service_name;
          uint32_t flags;
          EXPECT_TRUE(reader.PopString(&received_service_name));
          EXPECT_TRUE(reader.PopUint32(&flags));
          EXPECT_EQ(received_service_name, service_name);
          EXPECT_EQ(flags, 0U);

          response_ = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response_.get());
          writer.AppendUint32(reply_code);
          task_environment_.GetMainThreadTaskRunner()->PostTask(
              FROM_HERE, base::BindOnce(std::move(*callback), response_.get()));
        }));
  }

  void RunCheckForServiceAndStart(const std::string& service_name,
                                  std::optional<bool>* service_started_out) {
    base::RunLoop run_loop;
    CheckForServiceAndStartCallback callback = base::BindOnce(
        [](std::optional<bool>* service_started_out,
           base::OnceClosure quit_closure,
           std::optional<bool> service_started_in) {
          *service_started_out = service_started_in;
          std::move(quit_closure).Run();
        },
        service_started_out, run_loop.QuitClosure());

    CheckForServiceAndStart(mock_bus_, service_name, std::move(callback));

    run_loop.Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<dbus::Response> response_;
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_dbus_proxy_;
};

TEST_F(CheckForServiceAndStartTest, ServiceAlreadyRunning) {
  const std::string kServiceName = "org.example.Service";

  ExpectNameHasOwner(kServiceName, true);

  std::optional<bool> service_started;
  RunCheckForServiceAndStart(kServiceName, &service_started);

  EXPECT_TRUE(service_started.has_value());
  EXPECT_TRUE(service_started.value());
}

TEST_F(CheckForServiceAndStartTest, ServiceNotRunningButActivatable) {
  const std::string kServiceName = "org.example.Service";

  ExpectNameHasOwner(kServiceName, false);
  ExpectListActivatableNames({kServiceName, "org.other.Service"});
  ExpectStartServiceByName(kServiceName, DBUS_START_REPLY_SUCCESS);

  std::optional<bool> service_started;
  RunCheckForServiceAndStart(kServiceName, &service_started);

  ASSERT_TRUE(service_started.has_value());
  EXPECT_TRUE(service_started.value());
}

TEST_F(CheckForServiceAndStartTest, ServiceNotRunningAndNotActivatable) {
  const std::string kServiceName = "org.example.Service";

  ExpectNameHasOwner(kServiceName, false);
  ExpectListActivatableNames({"org.other.Service"});

  std::optional<bool> service_started;
  RunCheckForServiceAndStart(kServiceName, &service_started);

  ASSERT_TRUE(service_started.has_value());
  EXPECT_FALSE(service_started.value());
}

TEST_F(CheckForServiceAndStartTest, NameHasOwnerError) {
  const std::string kServiceName = "org.example.Service";

  ExpectNameHasOwner(kServiceName, false, true);  // Simulate an error

  std::optional<bool> service_started;
  RunCheckForServiceAndStart(kServiceName, &service_started);

  EXPECT_FALSE(service_started.has_value());
}

}  // namespace
}  // namespace dbus_utils
