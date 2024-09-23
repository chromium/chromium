// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"

#include <deque>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/hermes/hermes_client_test_base.h"
#include "chromeos/ash/components/dbus/hermes/hermes_test_utils.h"
#include "dbus/bus.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace ash {

namespace {

const char kInvalidPath[] = "/test/invalid/path";
const char kTestActivationCode[] = "abc123";
const char kTestActivationCodeWithSpacing[] = "   abc123   ";
const char kTestConfirmationCode[] = "def456";
const char kTestEuiccPath[] = "/org/chromium/hermes/Euicc/1";
const char kTestCarrierProfilePath[] = "/org/chromium/hermes/Profile/1";
const char kDbusNoResponse[] = "org.freedesktop.DBus.Error.NoReply";

// Matches dbus::MethodCall for UninstallProfile call with given path.
MATCHER_P(MatchUninstallProfileCall, expected_profile_path, "") {
  dbus::MessageReader reader(arg);
  dbus::ObjectPath carrier_profile_path;
  if (arg->GetMember() != hermes::euicc::kUninstallProfile ||
      !reader.PopObjectPath(&carrier_profile_path) ||
      carrier_profile_path != expected_profile_path) {
    *result_listener << "has method_name=" << arg->GetMember()
                     << " carrier_profile_path="
                     << carrier_profile_path.value();
    return false;
  }
  return true;
}

MATCHER_P2(MatchRefreshSmdxProfilesCall,
           expected_activation_code,
           expected_restore_slot,
           "") {
  dbus::MessageReader reader(arg);
  std::string activation_code;
  bool restore_slot;
  if (arg->GetMember() != hermes::euicc::kRefreshSmdxProfiles ||
      !reader.PopString(&activation_code) ||
      activation_code != expected_activation_code ||
      !reader.PopBool(&restore_slot) || restore_slot != expected_restore_slot) {
    *result_listener << "has method_name=" << arg->GetMember()
                     << " activation_code=" << activation_code
                     << " restore_slot=" << restore_slot;
    return false;
  }
  return true;
}

MATCHER_P(MatchRequestPendingProfilesCall, expected_root_smds, "") {
  dbus::MessageReader reader(arg);
  std::string root_smds;
  if (arg->GetMember() != hermes::euicc::kRequestPendingProfiles ||
      !reader.PopString(&root_smds) || root_smds != expected_root_smds) {
    *result_listener << "has method_name=" << arg->GetMember()
                     << " root_smds=" << root_smds;
    return false;
  }
  return true;
}

MATCHER_P(MatchRefreshPendingProfilesCall, expected_restore_slot, "") {
  dbus::MessageReader reader(arg);
  bool restore_slot;
  if (arg->GetMember() != hermes::euicc::kRefreshInstalledProfiles ||
      !reader.PopBool(&restore_slot) || restore_slot != expected_restore_slot) {
    *result_listener << "has method_name=" << arg->GetMember()
                     << " restore_slot=" << restore_slot;
    return false;
  }
  return true;
}

// Matches dbus::MethodCall for ResetMemory call with given
// |expected_reset_option|.
MATCHER_P(MatchResetMemoryCall, expected_reset_option, "") {
  dbus::MessageReader reader(arg);
  int32_t reset_option;
  if (arg->GetMember() != hermes::euicc::kResetMemory ||
      !reader.PopInt32(&reset_option) ||
      reset_option != static_cast<int32_t>(expected_reset_option)) {
    *result_listener << "has method_name=" << arg->GetMember()
                     << " reset_option=" << reset_option;
    return false;
  }
  return true;
}

// Matches dbus::MethodCall for InstrallProfileFromActivationCode call with
// given activation code and confirmation code.
MATCHER_P2(MatchInstallFromActivationCodeCall,
           expected_activation_code,
           expected_confirmation_code,
           "") {
  dbus::MessageReader reader(arg);
  std::string activation_code;
  std::string confirmation_code;
  if (arg->GetMember() != hermes::euicc::kInstallProfileFromActivationCode ||
      !reader.PopString(&activation_code) ||
      activation_code != expected_activation_code ||
      !reader.PopString(&confirmation_code) ||
      confirmation_code != expected_confirmation_code) {
    *result_listener << "has method_name=" << arg->GetMember()
                     << " activation_code=" << activation_code
                     << " confirmation_code=" << confirmation_code;
    return false;
  }
  return true;
}

// Matches dbus::MethodCall for InstallPendingProfile call with given profile
// path and confirmation code.
MATCHER_P2(MatchInstallPendingProfileCall,
           expected_profile_path,
           expected_confirmation_code,
           "") {
  dbus::MessageReader reader(arg);
  dbus::ObjectPath carrier_profile_path;
  std::string confirmation_code;
  if (arg->GetMember() != hermes::euicc::kInstallPendingProfile ||
      !reader.PopObjectPath(&carrier_profile_path) ||
      carrier_profile_path != expected_profile_path ||
      !reader.PopString(&confirmation_code) ||
      confirmation_code != expected_confirmation_code) {
    *result_listener << "has method_name=" << arg->GetMember()
                     << " carrier_profile_path=" << carrier_profile_path.value()
                     << " confirmation_code=" << confirmation_code;
    return false;
  }
  return true;
}

void CopyRefreshSmdxProfilesResult(
    HermesResponseStatus* dest_status,
    std::vector<dbus::ObjectPath>* dest_profiles,
    HermesResponseStatus status,
    const std::vector<dbus::ObjectPath>& profiles) {
  *dest_status = status;
  *dest_profiles = profiles;
}

void CopyInstallResult(HermesResponseStatus* dest_status,
                       dbus::DBusResult* dest_dbus_result,
                       dbus::ObjectPath* dest_path,
                       HermesResponseStatus status,
                       dbus::DBusResult dbus_result,
                       const dbus::ObjectPath* carrier_profile_path) {
  *dest_status = status;
  *dest_dbus_result = dbus_result;
  if (carrier_profile_path) {
    *dest_path = *carrier_profile_path;
  }
}

// Test observer for HermesEuiccClient.
class TestHermesEuiccClientObserver : public HermesEuiccClient::Observer {
 public:
  TestHermesEuiccClientObserver() = default;
  TestHermesEuiccClientObserver(const TestHermesEuiccClientObserver&) = delete;
  ~TestHermesEuiccClientObserver() override = default;

  void OnEuiccReset(const dbus::ObjectPath& euicc_path) override {
    on_euicc_reset_calls_.push_back(euicc_path);
  }

  const std::vector<dbus::ObjectPath>& on_euicc_reset_calls() {
    return on_euicc_reset_calls_;
  }

 private:
  std::vector<dbus::ObjectPath> on_euicc_reset_calls_;
};

}  // namespace

class HermesEuiccClientTest : public HermesClientTestBase {
 public:
  struct HistogramState {
    size_t installation_requested_count = 0u;
    size_t hermes_unavailable_count = 0u;
    size_t installation_started_count = 0u;
    size_t installation_succeeded_count = 0u;
    size_t installation_no_response_count = 0u;
    size_t installation_failed_count = 0u;
  };

  HermesEuiccClientTest() = default;
  HermesEuiccClientTest(const HermesEuiccClientTest&) = delete;
  HermesEuiccClientTest& operator=(const HermesEuiccClientTest&) = delete;
  ~HermesEuiccClientTest() override = default;

  void SetUp() override {
    InitMockBus();

    dbus::ObjectPath euicc_path(kTestEuiccPath);
    proxy_ = new dbus::MockObjectProxy(GetMockBus(), hermes::kHermesServiceName,
                                       euicc_path);
    EXPECT_CALL(*GetMockBus(),
                GetObjectProxy(hermes::kHermesServiceName, euicc_path))
        .WillRepeatedly(Return(proxy_.get()));

    HermesEuiccClient::Initialize(GetMockBus());
    client_ = HermesEuiccClient::Get();
    client_->AddObserver(&test_observer_);

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { HermesEuiccClient::Shutdown(); }

  void CheckHistogramState(const HistogramState& histogram_state) {
    histogram_tester_.ExpectBucketCount(
        HermesEuiccClient::kHermesInstallationAttemptStepsHistogram,
        HermesEuiccClient::InstallationAttemptStep::kInstallationRequested,
        histogram_state.installation_requested_count);
    histogram_tester_.ExpectBucketCount(
        HermesEuiccClient::kHermesInstallationAttemptStepsHistogram,
        HermesEuiccClient::InstallationAttemptStep::kHermesUnavailable,
        histogram_state.hermes_unavailable_count);
    histogram_tester_.ExpectBucketCount(
        HermesEuiccClient::kHermesInstallationAttemptStepsHistogram,
        HermesEuiccClient::InstallationAttemptStep::kInstallationStarted,
        histogram_state.installation_started_count);
    histogram_tester_.ExpectBucketCount(
        HermesEuiccClient::kHermesInstallationAttemptStepsHistogram,
        HermesEuiccClient::InstallationAttemptStep::kInstallationSucceeded,
        histogram_state.installation_succeeded_count);
    histogram_tester_.ExpectBucketCount(
        HermesEuiccClient::kHermesInstallationAttemptStepsHistogram,
        HermesEuiccClient::InstallationAttemptStep::kInstallationNoResponse,
        histogram_state.installation_no_response_count);
    histogram_tester_.ExpectBucketCount(
        HermesEuiccClient::kHermesInstallationAttemptStepsHistogram,
        HermesEuiccClient::InstallationAttemptStep::kInstallationFailed,
        histogram_state.installation_failed_count);
  }

  int MaxInstallAttempts() { return HermesEuiccClient::kMaxInstallAttempts; }

  base::TimeDelta InstallRetryDelay() {
    return HermesEuiccClient::kInstallRetryDelay;
  }

 protected:
  scoped_refptr<dbus::MockObjectProxy> proxy_;

  raw_ptr<HermesEuiccClient, DanglingUntriaged> client_;
  TestHermesEuiccClientObserver test_observer_;
  base::HistogramTester histogram_tester_;
};

TEST_F(HermesEuiccClientTest, TestInstallProfileWhenHermesIsDown) {
  dbus::ObjectPath test_euicc_path(kTestEuiccPath);
  dbus::ObjectPath test_carrier_path(kTestCarrierProfilePath);
  HermesResponseStatus install_status;
  dbus::DBusResult dbus_result;
  dbus::ObjectPath installed_profile_path(kInvalidPath);

  EXPECT_CALL(*proxy_.get(), DoWaitForServiceToBeAvailable(_))
      .WillOnce(testing::WithArg<0>(
          [=](dbus::ObjectProxy::WaitForServiceToBeAvailableCallback*
                  callback) {
            std::move(*callback).Run(/*service_is_available=*/false);
          }));

  HistogramState histogram_state;
  CheckHistogramState(histogram_state);

  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter response_writer(response.get());
  response_writer.AppendObjectPath(test_carrier_path);
  AddPendingMethodCallResult(std::move(response), nullptr);
  client_->InstallProfileFromActivationCode(
      test_euicc_path, kTestActivationCode, kTestConfirmationCode,
      base::BindOnce(&CopyInstallResult, &install_status, &dbus_result,
                     &installed_profile_path));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(install_status, HermesResponseStatus::kErrorWrongState);
  EXPECT_EQ(dbus_result, dbus::DBusResult::kErrorServiceUnknown);

  histogram_state.installation_requested_count++;
  histogram_state.hermes_unavailable_count++;
  CheckHistogramState(histogram_state);
}

TEST_F(HermesEuiccClientTest, TestInstallProfileFromActivationCode) {
  dbus::ObjectPath test_euicc_path(kTestEuiccPath);
  dbus::ObjectPath test_carrier_path(kTestCarrierProfilePath);
  dbus::MethodCall method_call(
      hermes::kHermesEuiccInterface,
      hermes::euicc::kInstallProfileFromActivationCode);
  method_call.SetSerial(123);
  EXPECT_CALL(*proxy_.get(),
              DoCallMethodWithErrorResponse(
                  MatchInstallFromActivationCodeCall(
                      base::TrimWhitespaceASCII(kTestActivationCode,
                                                base::TrimPositions::TRIM_ALL),
                      kTestConfirmationCode),
                  _, _))
      .Times(7)
      .WillRepeatedly(Invoke(this, &HermesEuiccClientTest::OnMethodCalled));

  HermesResponseStatus install_status;
  dbus::DBusResult dbus_result;
  dbus::ObjectPath installed_profile_path(kInvalidPath);

  EXPECT_CALL(*proxy_.get(), DoWaitForServiceToBeAvailable(_))
      .Times(3)
      .WillRepeatedly(testing::WithArg<0>(
          [=](dbus::ObjectProxy::WaitForServiceToBeAvailableCallback*
                  callback) {
            std::move(*callback).Run(/*service_is_available=*/true);
          }));

  HistogramState histogram_state;
  CheckHistogramState(histogram_state);

  // Verify that client makes corresponding dbus method call with
  // correct arguments.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter response_writer(response.get());
  response_writer.AppendObjectPath(test_carrier_path);
  AddPendingMethodCallResult(std::move(response), nullptr);
  client_->InstallProfileFromActivationCode(
      test_euicc_path, kTestActivationCode, kTestConfirmationCode,
      base::BindOnce(&CopyInstallResult, &install_status, &dbus_result,
                     &installed_profile_path));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(install_status, HermesResponseStatus::kSuccess);
  EXPECT_EQ(dbus_result, dbus::DBusResult::kSuccess);
  EXPECT_EQ(installed_profile_path, test_carrier_path);

  histogram_state.installation_requested_count++;
  histogram_state.installation_started_count++;
  histogram_state.installation_succeeded_count++;
  CheckHistogramState(histogram_state);

  // Verify that error responses are returned properly.
  installed_profile_path = dbus::ObjectPath(kInvalidPath);
  std::unique_ptr<dbus::ErrorResponse> error_response =
      dbus::ErrorResponse::FromMethodCall(
          &method_call, hermes::kErrorInvalidActivationCode, "");
  AddPendingMethodCallResult(nullptr, std::move(error_response));
  client_->InstallProfileFromActivationCode(
      test_euicc_path, kTestActivationCode, kTestConfirmationCode,
      base::BindOnce(&CopyInstallResult, &install_status, &dbus_result,
                     &installed_profile_path));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(install_status, HermesResponseStatus::kErrorInvalidActivationCode);
  EXPECT_EQ(dbus_result, dbus::DBusResult::kErrorUnknown);
  EXPECT_EQ(installed_profile_path.value(), kInvalidPath);

  histogram_state.installation_requested_count++;
  histogram_state.installation_started_count++;
  histogram_state.installation_failed_count++;
  CheckHistogramState(histogram_state);

  // Verify that dbus errors are captured properly.
  installed_profile_path = dbus::ObjectPath(kInvalidPath);
  for (int i = 0; i <= MaxInstallAttempts(); i++) {
    error_response = dbus::ErrorResponse::FromMethodCall(
        &method_call, DBUS_ERROR_LIMITS_EXCEEDED, "");
    AddPendingMethodCallResult(nullptr, std::move(error_response));
  }
  client_->InstallProfileFromActivationCode(
      test_euicc_path, kTestActivationCode, kTestConfirmationCode,
      base::BindOnce(&CopyInstallResult, &install_status, &dbus_result,
                     &installed_profile_path));
  base::RunLoop loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), InstallRetryDelay() * 5);
  loop.Run();
  EXPECT_EQ(install_status, HermesResponseStatus::kErrorUnknownResponse);
  EXPECT_EQ(dbus_result, dbus::DBusResult::kErrorLimitsExceeded);

  histogram_state.installation_requested_count++;
  histogram_state.installation_started_count++;
  histogram_state.installation_failed_count++;
  CheckHistogramState(histogram_state);
}

// Hermes does not allow leading and trailing whitespace in activation codes; if provided,
// these spaces can result in unexpected behavior e.g. failing to discover available eSIM
// profiles that would have been found had the spaces been removed. All activation codes that
// are provided to Hermes should be sanitized, and this test ensures we correctly handle any
// activation codes with leading and/or trailing whitespace.
TEST_F(HermesEuiccClientTest, TestInstallProfileFromActivationCodeWithSpacing) {
  dbus::ObjectPath test_euicc_path(kTestEuiccPath);
  dbus::ObjectPath test_carrier_path(kTestCarrierProfilePath);
  dbus::MethodCall method_call(
      hermes::kHermesEuiccInterface,
      hermes::euicc::kInstallProfileFromActivationCode);
  method_call.SetSerial(123);
  EXPECT_CALL(*proxy_.get(),
              DoCallMethodWithErrorResponse(
                  MatchInstallFromActivationCodeCall(
                      base::TrimWhitespaceASCII(kTestActivationCodeWithSpacing,
                                                base::TrimPositions::TRIM_ALL),
                      kTestConfirmationCode),
                  _, _))
      .WillOnce(Invoke(this, &HermesEuiccClientTest::OnMethodCalled));

  HermesResponseStatus install_status;
  dbus::DBusResult dbus_result;
  dbus::ObjectPath installed_profile_path(kInvalidPath);

  EXPECT_CALL(*proxy_.get(), DoWaitForServiceToBeAvailable(_))
      .WillOnce(testing::WithArg<0>(
          [=](dbus::ObjectProxy::WaitForServiceToBeAvailableCallback*
                  callback) {
            std::move(*callback).Run(/*service_is_available=*/true);
          }));

  // Verify that client makes corresponding dbus method call with
  // correct arguments.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter response_writer(response.get());
  response_writer.AppendObjectPath(test_carrier_path);
  AddPendingMethodCallResult(std::move(response), nullptr);
  client_->InstallProfileFromActivationCode(
      test_euicc_path, kTestActivationCodeWithSpacing, kTestConfirmationCode,
      base::BindOnce(&CopyInstallResult, &install_status, &dbus_result,
                     &installed_profile_path));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(install_status, HermesResponseStatus::kSuccess);
  EXPECT_EQ(dbus_result, dbus::DBusResult::kSuccess);
  EXPECT_EQ(installed_profile_path, test_carrier_path);
}

TEST_F(HermesEuiccClientTest, TestInstallPendingProfile) {
  dbus::ObjectPath test_euicc_path(kTestEuiccPath);
  dbus::ObjectPath test_carrier_path(kTestCarrierProfilePath);
  dbus::MethodCall method_call(hermes::kHermesEuiccInterface,
                               hermes::euicc::kInstallPendingProfile);
  method_call.SetSerial(123);
  EXPECT_CALL(*proxy_.get(), DoCallMethodWithErrorResponse(
                                 MatchInstallPendingProfileCall(
                                     test_carrier_path, kTestConfirmationCode),
                                 _, _))
      .Times(2)
      .WillRepeatedly(Invoke(this, &HermesEuiccClientTest::OnMethodCalled));

  HermesResponseStatus install_status;

  // Verify that client makes corresponding dbus method call with
  // correct arguments.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter response_writer(response.get());
  response_writer.AppendObjectPath(test_carrier_path);
  AddPendingMethodCallResult(std::move(response), nullptr);
  client_->InstallPendingProfile(
      test_euicc_path, test_carrier_path, kTestConfirmationCode,
      base::BindOnce(&hermes_test_utils::CopyHermesStatus, &install_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(install_status, HermesResponseStatus::kSuccess);

  // Verify that error responses are returned properly.
  std::unique_ptr<dbus::ErrorResponse> error_response =
      dbus::ErrorResponse::FromMethodCall(&method_call,
                                          hermes::kErrorInvalidParameter, "");
  AddPendingMethodCallResult(nullptr, std::move(error_response));
  client_->InstallPendingProfile(
      test_euicc_path, test_carrier_path, kTestConfirmationCode,
      base::BindOnce(&hermes_test_utils::CopyHermesStatus, &install_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(install_status, HermesResponseStatus::kErrorInvalidParameter);
}

TEST_F(HermesEuiccClientTest, TestRefreshInstalledProfiles) {
  dbus::ObjectPath test_euicc_path(kTestEuiccPath);
  dbus::MethodCall method_call(hermes::kHermesEuiccInterface,
                               hermes::euicc::kRefreshInstalledProfiles);
  method_call.SetSerial(123);
  EXPECT_CALL(*proxy_.get(), DoCallMethodWithErrorResponse(
                                 MatchRefreshPendingProfilesCall(
                                     /*expected_restore_slot=*/false),
                                 _, _))
      .Times(2)
      .WillRepeatedly(Invoke(this, &HermesEuiccClientTest::OnMethodCalled));

  HermesResponseStatus status;

  // Verify that client makes corresponding dbus method call with
  // correct arguments.
  AddPendingMethodCallResult(dbus::Response::CreateEmpty(), nullptr);
  client_->RefreshInstalledProfiles(
      test_euicc_path,
      /*restore_slot=*/false,
      base::BindOnce(&hermes_test_utils::CopyHermesStatus, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(status, HermesResponseStatus::kSuccess);

  // Verify that error responses are returned properly.
  std::unique_ptr<dbus::ErrorResponse> error_response =
      dbus::ErrorResponse::FromMethodCall(&method_call, hermes::kErrorUnknown,
                                          "");
  AddPendingMethodCallResult(nullptr, std::move(error_response));
  client_->RefreshInstalledProfiles(
      test_euicc_path,
      /*restore_slot=*/false,
      base::BindOnce(&hermes_test_utils::CopyHermesStatus, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(status, HermesResponseStatus::kErrorUnknown);
}

TEST_F(HermesEuiccClientTest, TestRefreshSmdxProfiles) {

  dbus::ObjectPath test_euicc_path(kTestEuiccPath);
  dbus::MethodCall method_call(hermes::kHermesEuiccInterface,
                               hermes::euicc::kRefreshSmdxProfiles);
  method_call.SetSerial(123);
  EXPECT_CALL(*proxy_.get(),
              DoCallMethodWithErrorResponse(
                  MatchRefreshSmdxProfilesCall(
                      base::TrimWhitespaceASCII(kTestActivationCode,
                                                base::TrimPositions::TRIM_ALL),
                      /*expected_restore_slot=*/true),
                  _, _))
      .Times(2)
      .WillRepeatedly(Invoke(this, &HermesEuiccClientTest::OnMethodCalled));

  HermesResponseStatus status;
  std::vector<dbus::ObjectPath> profiles;

  const std::vector<dbus::ObjectPath> kProfilesToReturn{
      {dbus::ObjectPath(kTestCarrierProfilePath)}};

  // Verify that client makes corresponding dbus method call with
  // correct arguments.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter response_writer(response.get());
  response_writer.AppendArrayOfObjectPaths(kProfilesToReturn);
  AddPendingMethodCallResult(std::move(response), nullptr);
  client_->RefreshSmdxProfiles(
      test_euicc_path, kTestActivationCode, /*restore_slot=*/true,
      base::BindOnce(CopyRefreshSmdxProfilesResult, &status, &profiles));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(status, HermesResponseStatus::kSuccess);
  ASSERT_EQ(profiles.size(), 1u);
  EXPECT_EQ(profiles.front(), kProfilesToReturn.front());

  // Verify that error responses are returned properly.
  std::unique_ptr<dbus::ErrorResponse> error_response =
      dbus::ErrorResponse::FromMethodCall(&method_call,
                                          hermes::kErrorNoResponse, "");
  AddPendingMethodCallResult(nullptr, std::move(error_response));
  client_->RefreshSmdxProfiles(
      test_euicc_path, kTestActivationCode, /*restore_slot=*/true,
      base::BindOnce(CopyRefreshSmdxProfilesResult, &status, &profiles));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(status, HermesResponseStatus::kErrorNoResponse);
  EXPECT_TRUE(profiles.empty());
}

TEST_F(HermesEuiccClientTest, TestRequestPendingProfiles) {
  dbus::ObjectPath test_euicc_path(kTestEuiccPath);
  dbus::MethodCall method_call(hermes::kHermesEuiccInterface,
                               hermes::euicc::kRequestPendingProfiles);
  method_call.SetSerial(123);
  EXPECT_CALL(*proxy_.get(),
              DoCallMethodWithErrorResponse(
                  MatchRequestPendingProfilesCall(kTestActivationCode), _, _))
      .Times(2)
      .WillRepeatedly(Invoke(this, &HermesEuiccClientTest::OnMethodCalled));

  HermesResponseStatus status;

  // Verify that client makes corresponding dbus method call with
  // correct arguments.
  AddPendingMethodCallResult(dbus::Response::CreateEmpty(), nullptr);
  client_->RequestPendingProfiles(
      test_euicc_path, kTestActivationCode,
      base::BindOnce(&hermes_test_utils::CopyHermesStatus, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(status, HermesResponseStatus::kSuccess);

  // Verify that error responses are returned properly.
  std::unique_ptr<dbus::ErrorResponse> error_response =
      dbus::ErrorResponse::FromMethodCall(&method_call, hermes::kErrorUnknown,
                                          "");
  AddPendingMethodCallResult(nullptr, std::move(error_response));
  client_->RequestPendingProfiles(
      test_euicc_path, kTestActivationCode,
      base::BindOnce(&hermes_test_utils::CopyHermesStatus, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(status, HermesResponseStatus::kErrorUnknown);
}

TEST_F(HermesEuiccClientTest, TestUninstallProfile) {
  dbus::ObjectPath test_euicc_path(kTestEuiccPath);
  dbus::ObjectPath test_carrier_path(kTestCarrierProfilePath);
  dbus::MethodCall method_call(hermes::kHermesEuiccInterface,
                               hermes::euicc::kUninstallProfile);
  method_call.SetSerial(123);
  EXPECT_CALL(*proxy_.get(),
              DoCallMethodWithErrorResponse(
                  MatchUninstallProfileCall(test_carrier_path), _, _))
      .Times(2)
      .WillRepeatedly(Invoke(this, &HermesEuiccClientTest::OnMethodCalled));

  HermesResponseStatus status;

  // Verify that client makes corresponding dbus method call with
  // correct arguments.
  AddPendingMethodCallResult(dbus::Response::CreateEmpty(), nullptr);
  client_->UninstallProfile(
      test_euicc_path, test_carrier_path,
      base::BindOnce(&hermes_test_utils::CopyHermesStatus, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(status, HermesResponseStatus::kSuccess);

  // Verify that error responses are returned properly.
  std::unique_ptr<dbus::ErrorResponse> error_response =
      dbus::ErrorResponse::FromMethodCall(&method_call, hermes::kErrorUnknown,
                                          "");
  AddPendingMethodCallResult(nullptr, std::move(error_response));
  client_->UninstallProfile(
      test_euicc_path, test_carrier_path,
      base::BindOnce(&hermes_test_utils::CopyHermesStatus, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(status, HermesResponseStatus::kErrorUnknown);
}

TEST_F(HermesEuiccClientTest, TestResetMemory) {
  const hermes::euicc::ResetOptions kTestResetOption =
      hermes::euicc::ResetOptions::kDeleteOperationalProfiles;
  dbus::ObjectPath test_euicc_path(kTestEuiccPath);
  dbus::MethodCall method_call(hermes::kHermesEuiccInterface,
                               hermes::euicc::kResetMemory);

  method_call.SetSerial(123);
  EXPECT_CALL(*proxy_.get(), DoCallMethodWithErrorResponse(
                                 MatchResetMemoryCall(kTestResetOption), _, _))
      .Times(3)
      .WillRepeatedly(Invoke(this, &HermesEuiccClientTest::OnMethodCalled));

  HermesResponseStatus status;
  const std::vector<dbus::ObjectPath>& on_euicc_reset_calls =
      test_observer_.on_euicc_reset_calls();

  // Verify that client makes corresponding dbus method call with
  // correct arguments.
  AddPendingMethodCallResult(dbus::Response::CreateEmpty(), nullptr);
  client_->ResetMemory(
      test_euicc_path, kTestResetOption,
      base::BindOnce(&hermes_test_utils::CopyHermesStatus, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(status, HermesResponseStatus::kSuccess);
  EXPECT_EQ(1u, on_euicc_reset_calls.size());
  EXPECT_EQ(test_euicc_path, on_euicc_reset_calls.front());

  // Verify that error responses are returned properly.
  std::unique_ptr<dbus::ErrorResponse> error_response =
      dbus::ErrorResponse::FromMethodCall(&method_call, hermes::kErrorUnknown,
                                          "");
  AddPendingMethodCallResult(nullptr, std::move(error_response));
  client_->ResetMemory(
      test_euicc_path, kTestResetOption,
      base::BindOnce(&hermes_test_utils::CopyHermesStatus, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(status, HermesResponseStatus::kErrorUnknown);
  EXPECT_EQ(1u, on_euicc_reset_calls.size());

  error_response =
      dbus::ErrorResponse::FromMethodCall(&method_call, kDbusNoResponse, "");
  AddPendingMethodCallResult(nullptr, std::move(error_response));
  client_->ResetMemory(
      test_euicc_path, kTestResetOption,
      base::BindOnce(&hermes_test_utils::CopyHermesStatus, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(status, HermesResponseStatus::kErrorNoResponse);
  EXPECT_EQ(1u, on_euicc_reset_calls.size());
}

}  // namespace ash
