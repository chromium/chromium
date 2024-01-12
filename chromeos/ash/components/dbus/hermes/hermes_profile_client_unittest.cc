// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/hermes/hermes_client_test_base.h"
#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "chromeos/ash/components/dbus/hermes/hermes_test_utils.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace ash {

namespace {

const char* kTestProfilePath = "/org/chromium/hermes/Profile/1";

}  // namespace

class HermesProfileClientTest : public HermesClientTestBase {
 public:
  HermesProfileClientTest() = default;
  HermesProfileClientTest(const HermesProfileClientTest&) = delete;
  ~HermesProfileClientTest() override = default;

  void SetUp() override {
    InitMockBus();

    dbus::ObjectPath test_profile_path(kTestProfilePath);
    proxy_ = new dbus::MockObjectProxy(GetMockBus(), hermes::kHermesServiceName,
                                       test_profile_path);
    EXPECT_CALL(*GetMockBus(),
                GetObjectProxy(hermes::kHermesServiceName, test_profile_path))
        .WillRepeatedly(Return(proxy_.get()));

    HermesProfileClient::Initialize(GetMockBus());
    client_ = HermesProfileClient::Get();

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { HermesProfileClient::Shutdown(); }

  HermesProfileClientTest& operator=(const HermesProfileClientTest&) = delete;

 protected:
  scoped_refptr<dbus::MockObjectProxy> proxy_;
  raw_ptr<HermesProfileClient, DanglingUntriaged> client_;
};

TEST_F(HermesProfileClientTest, TestEnableProfile) {
  dbus::ObjectPath test_profile_path(kTestProfilePath);
  dbus::MethodCall method_call(hermes::kHermesProfileInterface,
                               hermes::profile::kEnable);
  method_call.SetSerial(123);
  EXPECT_CALL(
      *proxy_.get(),
      DoCallMethodWithErrorResponse(
          hermes_test_utils::MatchMethodName(hermes::profile::kEnable), _, _))
      .Times(2)
      .WillRepeatedly(Invoke(this, &HermesProfileClientTest::OnMethodCalled));

  // Verify that client makes corresponding dbus method call with
  // correct arguments.
  HermesResponseStatus status;
  AddPendingMethodCallResult(dbus::Response::CreateEmpty(), nullptr);
  client_->EnableCarrierProfile(
      test_profile_path,
      base::BindOnce(&hermes_test_utils::CopyHermesStatus, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(status, HermesResponseStatus::kSuccess);

  // Verify that error responses are returned properly.
  std::unique_ptr<dbus::ErrorResponse> error_response =
      dbus::ErrorResponse::FromMethodCall(&method_call,
                                          hermes::kErrorAlreadyEnabled, "");
  AddPendingMethodCallResult(nullptr, std::move(error_response));
  client_->EnableCarrierProfile(
      test_profile_path,
      base::BindOnce(&hermes_test_utils::CopyHermesStatus, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(status, HermesResponseStatus::kErrorAlreadyEnabled);
}

TEST_F(HermesProfileClientTest, TestDisableProfile) {
  dbus::ObjectPath test_profile_path(kTestProfilePath);
  dbus::MethodCall method_call(hermes::kHermesProfileInterface,
                               hermes::profile::kDisable);
  method_call.SetSerial(123);
  EXPECT_CALL(
      *proxy_.get(),
      DoCallMethodWithErrorResponse(
          hermes_test_utils::MatchMethodName(hermes::profile::kDisable), _, _))
      .Times(2)
      .WillRepeatedly(Invoke(this, &HermesProfileClientTest::OnMethodCalled));

  // Verify that client makes corresponding dbus method call with
  // correct arguments.
  HermesResponseStatus status;
  AddPendingMethodCallResult(dbus::Response::CreateEmpty(), nullptr);
  client_->DisableCarrierProfile(
      test_profile_path,
      base::BindOnce(&hermes_test_utils::CopyHermesStatus, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(status, HermesResponseStatus::kSuccess);

  // Verify that error responses are returned properly.
  std::unique_ptr<dbus::ErrorResponse> error_response =
      dbus::ErrorResponse::FromMethodCall(&method_call,
                                          hermes::kErrorAlreadyDisabled, "");
  AddPendingMethodCallResult(nullptr, std::move(error_response));
  client_->DisableCarrierProfile(
      test_profile_path,
      base::BindOnce(&hermes_test_utils::CopyHermesStatus, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(status, HermesResponseStatus::kErrorAlreadyDisabled);
}

TEST_F(HermesProfileClientTest, TestRenameProfile) {
  dbus::ObjectPath test_profile_path(kTestProfilePath);
  dbus::MethodCall method_call(hermes::kHermesProfileInterface,
                               hermes::profile::kRename);
  method_call.SetSerial(123);
  EXPECT_CALL(
      *proxy_.get(),
      DoCallMethodWithErrorResponse(
          hermes_test_utils::MatchMethodName(hermes::profile::kRename), _, _))
      .Times(2)
      .WillRepeatedly(Invoke(this, &HermesProfileClientTest::OnMethodCalled));

  // Verify that client makes corresponding dbus method call with
  // correct arguments.
  HermesResponseStatus status;
  AddPendingMethodCallResult(dbus::Response::CreateEmpty(), nullptr);
  client_->RenameProfile(
      test_profile_path, "new_profile_name",
      base::BindOnce(&hermes_test_utils::CopyHermesStatus, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(status, HermesResponseStatus::kSuccess);

  // Verify that error responses are returned properly.
  std::unique_ptr<dbus::ErrorResponse> error_response =
      dbus::ErrorResponse::FromMethodCall(&method_call,
                                          hermes::kErrorInvalidParameter, "");
  AddPendingMethodCallResult(nullptr, std::move(error_response));
  client_->RenameProfile(
      test_profile_path, "another_new_name",
      base::BindOnce(&hermes_test_utils::CopyHermesStatus, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(status, HermesResponseStatus::kErrorInvalidParameter);
}

}  // namespace ash
