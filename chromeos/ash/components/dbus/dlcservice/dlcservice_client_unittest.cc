// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArg;

namespace ash {

namespace {
std::unique_ptr<dbus::Signal> CreateSignal(
    const dlcservice::DlcState& dlc_state) {
  auto signal =
      std::make_unique<dbus::Signal>("com.example.Interface", "SomeSignal");
  dbus::MessageWriter writer(signal.get());
  writer.AppendProtoAsArrayOfBytes(dlc_state);
  return signal;
}

class DlcserviceClientTest : public testing::Test {
 public:
  void SetUp() override {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;

    mock_bus_ = base::MakeRefCounted<::testing::NiceMock<dbus::MockBus>>(
        dbus::Bus::Options());

    mock_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), dlcservice::kDlcServiceServiceName,
        dbus::ObjectPath(dlcservice::kDlcServiceServicePath));

    EXPECT_CALL(
        *mock_bus_.get(),
        GetObjectProxy(dlcservice::kDlcServiceServiceName,
                       dbus::ObjectPath(dlcservice::kDlcServiceServicePath)))
        .WillOnce(Return(mock_proxy_.get()));

    EXPECT_CALL(*mock_proxy_.get(),
                DoConnectToSignal(dlcservice::kDlcServiceInterface, _, _, _))
        .WillOnce(Invoke(this, &DlcserviceClientTest::ConnectToSignal));

    EXPECT_CALL(*mock_proxy_.get(), DoWaitForServiceToBeAvailable(_)).Times(1);

    DlcserviceClient::Initialize(mock_bus_.get());
    client_ = DlcserviceClient::Get();

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { DlcserviceClient::Shutdown(); }

  void CallMethodWithErrorResponse(
      dbus::MethodCall* method_call,
      int timeout_ms,
      dbus::ObjectProxy::ResponseOrErrorCallback* callback) {
    dbus::Response* response = nullptr;
    dbus::ErrorResponse* err_response = nullptr;
    if (!responses_.empty()) {
      used_responses_.push_back(std::move(responses_.front()));
      responses_.pop_front();
      response = used_responses_.back().get();
    }
    if (!err_responses_.empty()) {
      used_err_responses_.push_back(std::move(err_responses_.front()));
      err_responses_.pop_front();
      err_response = used_err_responses_.back().get();
    }
    CHECK((response != nullptr) != (err_response != nullptr));
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(*callback), response, err_response));
  }

 protected:
  dlcservice::InstallRequest CreateInstallRequest(
      const std::string& id = {},
      const std::string& omaha_url = {},
      bool reserve = false) {
    dlcservice::InstallRequest install_request;
    install_request.set_id(id);
    install_request.set_omaha_url(omaha_url);
    install_request.set_reserve(reserve);
    return install_request;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<DlcserviceClient, DanglingUntriaged> client_;
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
  std::deque<std::unique_ptr<dbus::Response>> responses_;
  std::deque<std::unique_ptr<dbus::ErrorResponse>> err_responses_;

 private:
  void ConnectToSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      dbus::ObjectProxy::SignalCallback signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    EXPECT_EQ(interface_name, dlcservice::kDlcServiceInterface);
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(*on_connected_callback), interface_name,
                       signal_name, true /* success */));
  }

  std::deque<std::unique_ptr<dbus::Response>> used_responses_;
  std::deque<std::unique_ptr<dbus::ErrorResponse>> used_err_responses_;
};

class MockObserver : public DlcserviceClient::Observer {
 public:
  MOCK_METHOD(void,
              OnDlcStateChanged,
              (const dlcservice::DlcState& dlc_state),
              ());
};

TEST_F(DlcserviceClientTest, GetDlcStateSuccessTest) {
  responses_.push_back(dbus::Response::CreateEmpty());
  dbus::Response* response = responses_.front().get();
  dbus::MessageWriter writer(response);
  dlcservice::DlcState dlc_state;
  writer.AppendProtoAsArrayOfBytes(dlc_state);

  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .WillOnce(
          Invoke(this, &DlcserviceClientTest::CallMethodWithErrorResponse));

  DlcserviceClient::GetDlcStateCallback callback =
      base::BindOnce([](std::string_view err, const dlcservice::DlcState&) {
        EXPECT_EQ(dlcservice::kErrorNone, err);
      });
  client_->GetDlcState("some-dlc-id", std::move(callback));
  base::RunLoop().RunUntilIdle();
}

TEST_F(DlcserviceClientTest, GetDlcStateFailureTest) {
  dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                               dlcservice::kGetDlcStateMethod);
  method_call.SetSerial(123);
  err_responses_.push_back(dbus::ErrorResponse::FromMethodCall(
      &method_call, DBUS_ERROR_FAILED, "some-unknown-error"));

  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(
          Invoke(this, &DlcserviceClientTest::CallMethodWithErrorResponse));

  client_->GetDlcState(
      "some-dlc-id",
      base::BindOnce([](std::string_view err, const dlcservice::DlcState&) {
        EXPECT_EQ(dlcservice::kErrorInternal, err);
      }));
  base::RunLoop().RunUntilIdle();

  err_responses_.push_back(dbus::ErrorResponse::FromMethodCall(
      &method_call, dlcservice::kErrorInvalidDlc,
      "Some error due to bad DLC."));

  client_->GetDlcState(
      "some-dlc-id",
      base::BindOnce([](std::string_view err, const dlcservice::DlcState&) {
        EXPECT_EQ(dlcservice::kErrorInvalidDlc, err);
      }));
  base::RunLoop().RunUntilIdle();
}

TEST_F(DlcserviceClientTest, GetExistingDlcsSuccessTest) {
  responses_.push_back(dbus::Response::CreateEmpty());
  dbus::Response* response = responses_.front().get();
  dbus::MessageWriter writer(response);
  dlcservice::DlcsWithContent dlcs_with_content;
  writer.AppendProtoAsArrayOfBytes(dlcs_with_content);

  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .WillOnce(
          Invoke(this, &DlcserviceClientTest::CallMethodWithErrorResponse));

  DlcserviceClient::GetExistingDlcsCallback callback = base::BindOnce(
      [](std::string_view err, const dlcservice::DlcsWithContent&) {
        EXPECT_EQ(dlcservice::kErrorNone, err);
      });
  client_->GetExistingDlcs(std::move(callback));
  base::RunLoop().RunUntilIdle();
}

TEST_F(DlcserviceClientTest, GetExistingDlcsFailureTest) {
  dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                               dlcservice::kGetExistingDlcsMethod);
  method_call.SetSerial(123);
  err_responses_.push_back(dbus::ErrorResponse::FromMethodCall(
      &method_call, DBUS_ERROR_FAILED, "some-unknown-error"));

  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(
          Invoke(this, &DlcserviceClientTest::CallMethodWithErrorResponse));

  client_->GetExistingDlcs(base::BindOnce(
      [](std::string_view err, const dlcservice::DlcsWithContent&) {
        EXPECT_EQ(dlcservice::kErrorInternal, err);
      }));
  base::RunLoop().RunUntilIdle();

  err_responses_.push_back(dbus::ErrorResponse::FromMethodCall(
      &method_call, dlcservice::kErrorInvalidDlc,
      "Some error due to bad DLC."));

  client_->GetExistingDlcs(base::BindOnce(
      [](std::string_view err, const dlcservice::DlcsWithContent&) {
        EXPECT_EQ(dlcservice::kErrorInvalidDlc, err);
      }));
  base::RunLoop().RunUntilIdle();
}

TEST_F(DlcserviceClientTest, UninstallSuccessTest) {
  responses_.push_back(dbus::Response::CreateEmpty());

  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .WillOnce(
          Invoke(this, &DlcserviceClientTest::CallMethodWithErrorResponse));

  DlcserviceClient::UninstallCallback callback = base::BindOnce(
      [](std::string_view err) { EXPECT_EQ(dlcservice::kErrorNone, err); });
  client_->Uninstall("some-dlc-id", std::move(callback));
  base::RunLoop().RunUntilIdle();
}

TEST_F(DlcserviceClientTest, UninstallFailureTest) {
  dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                               dlcservice::kUninstallMethod);
  method_call.SetSerial(123);
  err_responses_.push_back(dbus::ErrorResponse::FromMethodCall(
      &method_call, dlcservice::kErrorInternal, ""));

  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(
          Invoke(this, &DlcserviceClientTest::CallMethodWithErrorResponse));

  DlcserviceClient::UninstallCallback callback = base::BindOnce(
      [](std::string_view err) { EXPECT_EQ(dlcservice::kErrorInternal, err); });
  client_->Uninstall("some-dlc-id", std::move(callback));
  base::RunLoop().RunUntilIdle();
}

TEST_F(DlcserviceClientTest, UninstallBusyStatusTest) {
  dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                               dlcservice::kUninstallMethod);
  method_call.SetSerial(123);
  err_responses_.push_back(dbus::ErrorResponse::FromMethodCall(
      &method_call, dlcservice::kErrorBusy, ""));

  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(
          Invoke(this, &DlcserviceClientTest::CallMethodWithErrorResponse));

  DlcserviceClient::UninstallCallback callback = base::BindOnce(
      [](std::string_view err) { EXPECT_EQ(dlcservice::kErrorBusy, err); });
  client_->Uninstall("some-dlc-id", std::move(callback));
  base::RunLoop().RunUntilIdle();
}

TEST_F(DlcserviceClientTest, PurgeSuccessTest) {
  responses_.push_back(dbus::Response::CreateEmpty());

  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .WillOnce(
          Invoke(this, &DlcserviceClientTest::CallMethodWithErrorResponse));

  DlcserviceClient::PurgeCallback callback = base::BindOnce(
      [](std::string_view err) { EXPECT_EQ(dlcservice::kErrorNone, err); });
  client_->Purge("some-dlc-id", std::move(callback));
  base::RunLoop().RunUntilIdle();
}

TEST_F(DlcserviceClientTest, PurgeFailureTest) {
  dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                               dlcservice::kPurgeMethod);
  method_call.SetSerial(123);
  err_responses_.push_back(dbus::ErrorResponse::FromMethodCall(
      &method_call, dlcservice::kErrorInternal, ""));

  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(
          Invoke(this, &DlcserviceClientTest::CallMethodWithErrorResponse));

  DlcserviceClient::PurgeCallback callback = base::BindOnce(
      [](std::string_view err) { EXPECT_EQ(dlcservice::kErrorInternal, err); });
  client_->Purge("some-dlc-id", std::move(callback));
  base::RunLoop().RunUntilIdle();
}

TEST_F(DlcserviceClientTest, PurgeBusyStatusTest) {
  dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                               dlcservice::kPurgeMethod);
  method_call.SetSerial(123);
  err_responses_.push_back(dbus::ErrorResponse::FromMethodCall(
      &method_call, dlcservice::kErrorBusy, ""));

  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(
          Invoke(this, &DlcserviceClientTest::CallMethodWithErrorResponse));

  DlcserviceClient::PurgeCallback callback = base::BindOnce(
      [](std::string_view err) { EXPECT_EQ(dlcservice::kErrorBusy, err); });
  client_->Purge("some-dlc-id", std::move(callback));
  base::RunLoop().RunUntilIdle();
}

TEST_F(DlcserviceClientTest, InstallSuccessTest) {
  responses_.push_back(dbus::Response::CreateEmpty());

  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .WillOnce(
          Invoke(this, &DlcserviceClientTest::CallMethodWithErrorResponse));

  DlcserviceClient::InstallCallback install_callback =
      base::BindOnce([](const DlcserviceClient::InstallResult& install_result) {
        EXPECT_EQ(dlcservice::kErrorNone, install_result.error);
      });
  client_->Install(CreateInstallRequest("foo-dlc"), std::move(install_callback),
                   base::DoNothing());
  base::RunLoop().RunUntilIdle();
}

TEST_F(DlcserviceClientTest, InstallFailureTest) {
  dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                               dlcservice::kInstallMethod);
  method_call.SetSerial(123);
  err_responses_.push_back(dbus::ErrorResponse::FromMethodCall(
      &method_call, dlcservice::kErrorInternal, ""));

  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .WillOnce(
          Invoke(this, &DlcserviceClientTest::CallMethodWithErrorResponse));

  DlcserviceClient::InstallCallback install_callback =
      base::BindOnce([](const DlcserviceClient::InstallResult& install_result) {
        EXPECT_EQ(dlcservice::kErrorInternal, install_result.error);
      });
  client_->Install(CreateInstallRequest("foo-dlc"), std::move(install_callback),
                   base::DoNothing());
  base::RunLoop().RunUntilIdle();
}

TEST_F(DlcserviceClientTest, InstallProgressTest) {
  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .WillOnce(
          Invoke(this, &DlcserviceClientTest::CallMethodWithErrorResponse));
  std::atomic<size_t> counter{0};
  DlcserviceClient::InstallCallback install_callback = base::BindOnce(
      [](const DlcserviceClient::InstallResult& install_result) {});
  DlcserviceClient::ProgressCallback progress_callback = base::BindRepeating(
      [](decltype(counter)* counter, double) { ++*counter; }, &counter);

  responses_.push_back(dbus::Response::CreateEmpty());
  client_->Install(CreateInstallRequest(), std::move(install_callback),
                   std::move(progress_callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, counter.load());

  dlcservice::DlcState dlc_state;
  dlc_state.set_state(dlcservice::DlcState::INSTALLING);
  auto signal = CreateSignal(dlc_state);

  client_->DlcStateChangedForTest(signal.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, counter.load());
}

TEST_F(DlcserviceClientTest, InstallProgressSkipUnheldDlcIdsTest) {
  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .WillOnce(Return());
  std::atomic<size_t> counter{0};
  DlcserviceClient::InstallCallback install_callback = base::BindOnce(
      [](const DlcserviceClient::InstallResult& install_result) {});
  DlcserviceClient::ProgressCallback progress_callback = base::BindRepeating(
      [](decltype(counter)* counter, double) { ++*counter; }, &counter);

  client_->Install(CreateInstallRequest("foo"), std::move(install_callback),
                   std::move(progress_callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, counter.load());

  dlcservice::DlcState dlc_state;
  dlc_state.set_id("bar-is-not-foo");
  dlc_state.set_state(dlcservice::DlcState::INSTALLING);
  auto signal = CreateSignal(dlc_state);

  client_->DlcStateChangedForTest(signal.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, counter.load());
}

TEST_F(DlcserviceClientTest, InstallBusyStatusTest) {
  dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                               dlcservice::kInstallMethod);
  method_call.SetSerial(123);

  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(
          Invoke(this, &DlcserviceClientTest::CallMethodWithErrorResponse));

  err_responses_.push_back(dbus::ErrorResponse::FromMethodCall(
      &method_call, dlcservice::kErrorBusy, ""));
  DlcserviceClient::InstallCallback install_callback =
      base::BindOnce([](const DlcserviceClient::InstallResult& install_result) {
        EXPECT_EQ(dlcservice::kErrorNone, install_result.error);
      });
  client_->Install(CreateInstallRequest("foo-dlc"), std::move(install_callback),
                   base::DoNothing());
  base::RunLoop().RunUntilIdle();

  err_responses_.push_back(dbus::ErrorResponse::FromMethodCall(
      &method_call, dlcservice::kErrorInternal, ""));
  DlcserviceClient::InstallCallback failed_install_callback =
      base::BindOnce([](const DlcserviceClient::InstallResult& install_result) {
        EXPECT_EQ(dlcservice::kErrorInternal, install_result.error);
      });
  client_->Install(CreateInstallRequest("failed-dlc"),
                   std::move(failed_install_callback), base::DoNothing());
  base::RunLoop().RunUntilIdle();

  responses_.push_back(dbus::Response::CreateEmpty());
  task_environment_.FastForwardBy(base::Seconds(10));
  base::RunLoop().RunUntilIdle();

  dlcservice::DlcState dlc_state;
  dlc_state.set_id("foo-dlc");
  dlc_state.set_state(dlcservice::DlcState::INSTALLED);
  dlc_state.set_last_error_code(dlcservice::kErrorNone);
  auto signal = CreateSignal(dlc_state);
  client_->DlcStateChangedForTest(signal.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(DlcserviceClientTest, HoldMultiValidResponsesTaskTest) {
  const size_t kLoopCount = 3;
  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .Times(kLoopCount)
      .WillRepeatedly(
          Invoke(this, &DlcserviceClientTest::CallMethodWithErrorResponse));
  std::atomic<size_t> counter{0};

  // All |Install()| request should NOT be queued.
  for (size_t i = 0; i < kLoopCount; ++i) {
    DlcserviceClient::InstallCallback install_callback = base::BindOnce(
        [](decltype(counter)* counter,
           const DlcserviceClient::InstallResult& install_result) {
          ++*counter;
        },
        &counter);
    responses_.push_back(dbus::Response::CreateEmpty());
    client_->Install(CreateInstallRequest("dlc" + base::NumberToString(i)),
                     std::move(install_callback), base::DoNothing());
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, counter.load());

  for (size_t i = 0; i < kLoopCount; ++i) {
    dlcservice::DlcState dlc_state;
    dlc_state.set_id("dlc" + base::NumberToString(i));
    dlc_state.set_state(dlcservice::DlcState::INSTALLED);
    auto signal = CreateSignal(dlc_state);
    client_->DlcStateChangedForTest(signal.get());
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(i + 1, counter.load());
  }
}

TEST_F(DlcserviceClientTest, StateChangeObserver) {
  dlcservice::DlcState dlc_state;
  dlc_state.set_state(dlcservice::DlcState::INSTALLED);
  auto signal = CreateSignal(dlc_state);

  MockObserver observer;
  // If no observer has been added, nothing should be called.
  EXPECT_CALL(observer, OnDlcStateChanged(_)).Times(0);
  client_->DlcStateChangedForTest(signal.get());

  // Adding one observer should call once.
  EXPECT_CALL(observer, OnDlcStateChanged(_)).Times(1);
  client_->AddObserver(&observer);
  client_->DlcStateChangedForTest(signal.get());

  // Removing the observer causes nothing to be called.
  EXPECT_CALL(observer, OnDlcStateChanged(_)).Times(0);
  client_->RemoveObserver(&observer);
  client_->DlcStateChangedForTest(signal.get());
}

}  // namespace

}  // namespace ash
