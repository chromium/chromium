// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/portal.h"

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "components/dbus/xdg/portal_constants.h"
#include "components/dbus/xdg/systemd.h"
#include "components/dbus/xdg/systemd_constants.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace dbus_xdg {

namespace {

constexpr char kFakeUnitPath[] = "/fake/unit/path";

class RequestXdgDesktopPortalTest : public testing::Test {
 public:
  void SetUp() override {
    SetPortalStateForTesting(PortalRegistrarState::kIdle);
    bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

    EXPECT_CALL(*bus_, GetOriginTaskRunner())
        .WillRepeatedly(
            Return(task_environment_.GetMainThreadTaskRunner().get()));
  }

  void TearDown() override {
    SetPortalStateForTesting(PortalRegistrarState::kIdle);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<dbus::MockBus> bus_;
};

TEST_F(RequestXdgDesktopPortalTest, RequestXdgDesktopPortalSuccessNoSystemd) {
  // Mocks for SetSystemdScopeUnitNameForXdgPortal
  auto mock_dbus_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

  EXPECT_CALL(*bus_, GetObjectProxy(DBUS_SERVICE_DBUS,
                                    dbus::ObjectPath(DBUS_PATH_DBUS)))
      .WillRepeatedly(Return(mock_dbus_proxy.get()));

  // Mock NameHasOwner for systemd (return false to skip systemd setup)
  EXPECT_CALL(*mock_dbus_proxy, CallMethod(_, _, _))
      .WillRepeatedly([](dbus::MethodCall* method_call, int timeout_ms,
                         dbus::ObjectProxy::ResponseCallback callback) {
        if (method_call->GetMember() == "NameHasOwner") {
          dbus::MessageReader reader(method_call);
          std::string name;
          reader.PopString(&name);
          bool has_owner = false;
          if (name == kPortalServiceName) {
            has_owner = true;  // Portal exists
          } else if (name == "org.freedesktop.systemd1") {
            has_owner = false;  // Systemd not present
          }

          auto response = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          writer.AppendBool(has_owner);
          std::move(callback).Run(response.get());
          return;
        }
        std::move(callback).Run(nullptr);
      });

  // Mock Portal Proxy for Register
  auto mock_portal_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));

  EXPECT_CALL(*bus_, GetObjectProxy(kPortalServiceName,
                                    dbus::ObjectPath(kPortalObjectPath)))
      .WillRepeatedly(Return(mock_portal_proxy.get()));

  // Expect Register call because systemd unit creation failed
  // (kNoSystemdService)
  EXPECT_CALL(*mock_portal_proxy, CallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly([](dbus::MethodCall* method_call, int timeout_ms,
                         dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        if (method_call->GetInterface() == kRegistryInterface &&
            method_call->GetMember() == kMethodRegister) {
          auto response = dbus::Response::CreateEmpty();
          std::move(callback).Run(response.get(), nullptr);
        }
      });

  // Expect SetNameOwnerChangedCallback
  EXPECT_CALL(*mock_portal_proxy, SetNameOwnerChangedCallback(_));

  bool success = false;
  base::RunLoop run_loop;
  RequestXdgDesktopPortal(
      bus_.get(), base::BindOnce(
                      [](bool* out, base::OnceClosure quit_closure, bool res) {
                        *out = res;
                        std::move(quit_closure).Run();
                      },
                      &success, run_loop.QuitClosure()));

  run_loop.Run();
  EXPECT_TRUE(success);
}

TEST_F(RequestXdgDesktopPortalTest, RequestXdgDesktopPortalSuccessWithSystemd) {
  // Mocks for SetSystemdScopeUnitNameForXdgPortal
  auto mock_dbus_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

  EXPECT_CALL(*bus_, GetObjectProxy(DBUS_SERVICE_DBUS,
                                    dbus::ObjectPath(DBUS_PATH_DBUS)))
      .WillRepeatedly(Return(mock_dbus_proxy.get()));

  // Mock NameHasOwner for systemd (return true to enable systemd setup)
  EXPECT_CALL(*mock_dbus_proxy, CallMethod(_, _, _))
      .WillRepeatedly([](dbus::MethodCall* method_call, int timeout_ms,
                         dbus::ObjectProxy::ResponseCallback callback) {
        if (method_call->GetMember() == "NameHasOwner") {
          dbus::MessageReader reader(method_call);
          std::string name;
          reader.PopString(&name);
          const bool has_owner = true;  // Both Portal and Systemd exist

          auto response = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          writer.AppendBool(has_owner);
          std::move(callback).Run(response.get());
          return;
        }
        std::move(callback).Run(nullptr);
      });

  // Mock Systemd Proxy and interactions

  auto mock_systemd_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kServiceNameSystemd, dbus::ObjectPath(kObjectPathSystemd));

  EXPECT_CALL(*bus_, GetObjectProxy(kServiceNameSystemd,
                                    dbus::ObjectPath(kObjectPathSystemd)))
      .WillRepeatedly(Return(mock_systemd_proxy.get()));

  auto mock_dbus_unit_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kServiceNameSystemd, dbus::ObjectPath(kFakeUnitPath));
  EXPECT_CALL(*bus_, GetObjectProxy(kServiceNameSystemd,
                                    dbus::ObjectPath(kFakeUnitPath)))
      .WillRepeatedly(Return(mock_dbus_unit_proxy.get()));

  EXPECT_CALL(*mock_systemd_proxy, CallMethod(_, _, _))
      .WillRepeatedly([&](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback callback) {
        if (method_call->GetInterface() == kInterfaceSystemdManager &&
            method_call->GetMember() == kMethodStartTransientUnit) {
          auto response = dbus::Response::CreateEmpty();
          std::move(callback).Run(response.get());
        } else if (method_call->GetInterface() == kInterfaceSystemdManager &&
                   method_call->GetMember() == kMethodGetUnit) {
          auto response = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          writer.AppendObjectPath(dbus::ObjectPath(kFakeUnitPath));
          std::move(callback).Run(response.get());
        }
      });

  EXPECT_CALL(*mock_dbus_unit_proxy, CallMethod(_, _, _))
      .WillRepeatedly([](dbus::MethodCall* method_call, int timeout_ms,
                         dbus::ObjectProxy::ResponseCallback callback) {
        // Simulate a successful response with "active" state.
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        dbus::MessageWriter array_writer(nullptr);
        dbus::MessageWriter dict_entry_writer(nullptr);
        writer.OpenArray("{sv}", &array_writer);
        array_writer.OpenDictEntry(&dict_entry_writer);
        dict_entry_writer.AppendString(kSystemdActiveStateProp);
        dict_entry_writer.AppendVariantOfString(kSystemdStateActive);
        array_writer.CloseContainer(&dict_entry_writer);
        writer.CloseContainer(&array_writer);
        std::move(callback).Run(response.get());
      });

  // Mock Portal Proxy
  auto mock_portal_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));

  EXPECT_CALL(*bus_, GetObjectProxy(kPortalServiceName,
                                    dbus::ObjectPath(kPortalObjectPath)))
      .WillRepeatedly(Return(mock_portal_proxy.get()));

  // Expect NO Register call because systemd unit creation succeeded
  EXPECT_CALL(*mock_portal_proxy, CallMethod(_, _, _)).Times(0);

  // Expect NO SetNameOwnerChangedCallback
  EXPECT_CALL(*mock_portal_proxy, SetNameOwnerChangedCallback(_)).Times(0);

  bool success = false;
  base::RunLoop run_loop;
  RequestXdgDesktopPortal(
      bus_.get(), base::BindOnce(
                      [](bool* out, base::OnceClosure quit_closure, bool res) {
                        *out = res;
                        std::move(quit_closure).Run();
                      },
                      &success, run_loop.QuitClosure()));

  run_loop.Run();
  EXPECT_TRUE(success);
}

}  // namespace

}  // namespace dbus_xdg
