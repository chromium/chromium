// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/printscanmgr/printscanmgr_client.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/printscanmgr/printscanmgr_service.pb.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::base::test::EqualsProto;
using ::testing::_;
using ::testing::Return;

namespace ash {

namespace {

// Convenience method for creating a CupsAddManuallyConfiguredPrinterRequest.
printscanmgr::CupsAddManuallyConfiguredPrinterRequest
CreateCupsAddManuallyConfiguredPrinterRequest() {
  printscanmgr::CupsAddManuallyConfiguredPrinterRequest request;
  request.set_name("Manually configured printer name");
  request.set_uri("Manually configured printer URI");
  std::vector<uint8_t> ppd_contents = {0xDE, 0xAD, 0xBE, 0xEF};
  request.set_ppd_contents(
      std::string(ppd_contents.begin(), ppd_contents.end()));
  return request;
}

// Convenience method for creating a CupsAddManuallyConfiguredPrinterResponse.
printscanmgr::CupsAddManuallyConfiguredPrinterResponse
CreateCupsAddManuallyConfiguredPrinterResponse(
    printscanmgr::AddPrinterResult result) {
  printscanmgr::CupsAddManuallyConfiguredPrinterResponse response;
  response.set_result(result);
  return response;
}

// Convenience method for creating a CupsAddAutoConfiguredPrinterRequest.
printscanmgr::CupsAddAutoConfiguredPrinterRequest
CreateCupsAddAutoConfiguredPrinterRequest() {
  printscanmgr::CupsAddAutoConfiguredPrinterRequest request;
  request.set_name("Autoconfigured printer name");
  request.set_uri("Autoconfigured printer URI");
  return request;
}

// Convenience method for creating a CupsAddAutoConfiguredPrinterResponse.
printscanmgr::CupsAddAutoConfiguredPrinterResponse
CreateCupsAddAutoConfiguredPrinterResponse(
    printscanmgr::AddPrinterResult result) {
  printscanmgr::CupsAddAutoConfiguredPrinterResponse response;
  response.set_result(result);
  return response;
}

// Convenience method for creating a CupsRemovePrinterRequest.
printscanmgr::CupsRemovePrinterRequest CreateCupsRemovePrinterRequest() {
  printscanmgr::CupsRemovePrinterRequest request;
  request.set_name("Removed printer name");
  return request;
}

// Convenience method for creating a CupsRemovePrinterResponse.
printscanmgr::CupsRemovePrinterResponse CreateCupsRemovePrinterResponse() {
  printscanmgr::CupsRemovePrinterResponse response;
  response.set_result(true);
  return response;
}

// Convenience method for creating a CupsRetrievePpdRequest.
printscanmgr::CupsRetrievePpdRequest CreateCupsRetrievePpdRequest() {
  printscanmgr::CupsRetrievePpdRequest request;
  request.set_name("Retrieve PPD printer name");
  return request;
}

// Convenience method for creating a CupsRetrievePpdResponse.
printscanmgr::CupsRetrievePpdResponse CreateCupsRetrievePpdResponse() {
  printscanmgr::CupsRetrievePpdResponse response;
  std::vector<uint8_t> ppd = {0xBE, 0xEF, 0xDE, 0xAD};
  response.set_ppd(std::string(ppd.begin(), ppd.end()));
  return response;
}

// Matcher that verifies that a dbus::Message has member `name`.
MATCHER_P(HasMember, name, "") {
  if (arg->GetMember() != name) {
    *result_listener << "has member " << arg->GetMember();
    return false;
  }
  return true;
}

}  // namespace

class PrintscanmgrClientTest : public testing::Test {
 public:
  PrintscanmgrClientTest() = default;
  PrintscanmgrClientTest(const PrintscanmgrClientTest&) = delete;
  PrintscanmgrClientTest& operator=(const PrintscanmgrClientTest&) = delete;

  void SetUp() override {
    // Create mock D-Bus objects for printscanmgr.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options);
    mock_proxy_ = new dbus::MockObjectProxy(
        mock_bus_.get(), printscanmgr::kPrintscanmgrServiceName,
        dbus::ObjectPath(printscanmgr::kPrintscanmgrServicePath));

    // The client's Init() method should request a proxy for communicating with
    // the printscanmgr daemon.
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(
                    printscanmgr::kPrintscanmgrServiceName,
                    dbus::ObjectPath(printscanmgr::kPrintscanmgrServicePath)))
        .WillOnce(Return(mock_proxy_.get()));

    // ShutdownAndBlock() will be called in TearDown().
    EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock()).WillOnce(Return());

    // Create and initialize a client with the mock bus.
    PrintscanmgrClient::Initialize(mock_bus_.get());
  }

  void TearDown() override {
    PrintscanmgrClient::Shutdown();
    mock_bus_->ShutdownAndBlock();
  }

  // A shorter name to return the PrintscanmgrClient under test.
  PrintscanmgrClient* GetClient() { return PrintscanmgrClient::Get(); }

  // Adds an expectation to `mock_proxy_` that kCupsAddManuallyConfiguredPrinter
  // will be called. When called, `mock_proxy_` will respond with `response`.
  void SetCupsAddManuallyConfiguredPrinterExpectation(
      dbus::Response* response) {
    cups_add_manually_configured_printer_response_ = response;
    EXPECT_CALL(*mock_proxy_.get(),
                DoCallMethodWithErrorResponse(
                    HasMember(printscanmgr::kCupsAddManuallyConfiguredPrinter),
                    dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
        .WillOnce(Invoke(
            this,
            &PrintscanmgrClientTest::OnCallCupsAddManuallyConfiguredPrinter));
  }

  // Adds an expectation to `mock_proxy_` that kCupsAddAutoConfiguredPrinter
  // will be called. When called, `mock_proxy_` will respond with `response`.
  void SetCupsAddAutoConfiguredPrinterExpectation(dbus::Response* response) {
    cups_add_autoconfigured_printer_response_ = response;
    EXPECT_CALL(*mock_proxy_.get(),
                DoCallMethodWithErrorResponse(
                    HasMember(printscanmgr::kCupsAddAutoConfiguredPrinter),
                    dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
        .WillOnce(Invoke(
            this, &PrintscanmgrClientTest::OnCallCupsAddAutoConfiguredPrinter));
  }

  // Adds an expectation to `mock_proxy_` that kCupsRemovePrinter will be
  // called. When called, `mock_proxy_` will respond with `response`.
  void SetCupsRemovePrinterExpectation(dbus::Response* response) {
    cups_remove_printer_response_ = response;
    EXPECT_CALL(*mock_proxy_.get(),
                DoCallMethod(HasMember(printscanmgr::kCupsRemovePrinter),
                             dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
        .WillOnce(
            Invoke(this, &PrintscanmgrClientTest::OnCallCupsRemovePrinter));
  }

  // Adds an expectation to `mock_proxy_` that kCupsRetrievePpd will be called.
  // When called, `mock_proxy_` will respond with `response`.
  void SetCupsRetrievePpdExpectation(dbus::Response* response) {
    cups_retrieve_ppd_response_ = response;
    EXPECT_CALL(*mock_proxy_.get(),
                DoCallMethod(HasMember(printscanmgr::kCupsRetrievePpd),
                             dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
        .WillOnce(Invoke(this, &PrintscanmgrClientTest::OnCallCupsRetrievePpd));
  }

 private:
  // Responsible for responding to a kCupsAddManuallyConfiguredPrinter call.
  void OnCallCupsAddManuallyConfiguredPrinter(
      dbus::MethodCall* method_call,
      int timeout_ms,
      dbus::ObjectProxy::ResponseOrErrorCallback* callback) {
    // Verify that the request was created and sent correctly.
    printscanmgr::CupsAddManuallyConfiguredPrinterRequest request;
    ASSERT_TRUE(
        dbus::MessageReader(method_call).PopArrayOfBytesAsProto(&request));
    EXPECT_THAT(request,
                EqualsProto(CreateCupsAddManuallyConfiguredPrinterRequest()));
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(*callback),
                       cups_add_manually_configured_printer_response_,
                       /*ErrorResponse=*/nullptr));
  }

  // Responsible for responding to a kCupsAddAutoConfiguredPrinter call.
  void OnCallCupsAddAutoConfiguredPrinter(
      dbus::MethodCall* method_call,
      int timeout_ms,
      dbus::ObjectProxy::ResponseOrErrorCallback* callback) {
    // Verify that the request was created and sent correctly.
    printscanmgr::CupsAddAutoConfiguredPrinterRequest request;
    ASSERT_TRUE(
        dbus::MessageReader(method_call).PopArrayOfBytesAsProto(&request));
    EXPECT_THAT(request,
                EqualsProto(CreateCupsAddAutoConfiguredPrinterRequest()));
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*callback),
                                  cups_add_autoconfigured_printer_response_,
                                  /*ErrorResponse=*/nullptr));
  }

  // Responsible for responding to a kCupsRemovePrinter call.
  void OnCallCupsRemovePrinter(dbus::MethodCall* method_call,
                               int timeout_ms,
                               dbus::ObjectProxy::ResponseCallback* callback) {
    // Verify that the request was created and sent correctly.
    printscanmgr::CupsRemovePrinterRequest request;
    ASSERT_TRUE(
        dbus::MessageReader(method_call).PopArrayOfBytesAsProto(&request));
    EXPECT_THAT(request, EqualsProto(CreateCupsRemovePrinterRequest()));
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(*callback), cups_remove_printer_response_));
  }

  // Responsible for responding to a kCupsRetrievePpd call.
  void OnCallCupsRetrievePpd(dbus::MethodCall* method_call,
                             int timeout_ms,
                             dbus::ObjectProxy::ResponseCallback* callback) {
    // Verify that the request was created and sent correctly.
    printscanmgr::CupsRetrievePpdRequest request;
    ASSERT_TRUE(
        dbus::MessageReader(method_call).PopArrayOfBytesAsProto(&request));
    EXPECT_THAT(request, EqualsProto(CreateCupsRetrievePpdRequest()));
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(*callback), cups_retrieve_ppd_response_));
  }

  // A message loop to emulate asynchronous behavior.
  base::test::TaskEnvironment task_environment_;
  // Mock D-Bus objects for the client to interact with.
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
  // Used to respond to kCupsAddManuallyConfiguredPrinter D-Bus calls.
  raw_ptr<dbus::Response, DanglingUntriaged>
      cups_add_manually_configured_printer_response_ = nullptr;
  // Used to respond to kCupsAddAutoConfiguredPrinter D-Bus calls.
  raw_ptr<dbus::Response, DanglingUntriaged>
      cups_add_autoconfigured_printer_response_ = nullptr;
  // Used to respond to kCupsRemovePrinter D-Bus calls.
  raw_ptr<dbus::Response, DanglingUntriaged> cups_remove_printer_response_ =
      nullptr;
  // Used to respond to kCupsRetrievePpd D-Bus calls.
  raw_ptr<dbus::Response, DanglingUntriaged> cups_retrieve_ppd_response_ =
      nullptr;
};

// Test that the client can request that cupsd adds a manually configured
// printer.
TEST_F(PrintscanmgrClientTest, CupsAddManuallyConfiguredPrinter) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  const printscanmgr::CupsAddManuallyConfiguredPrinterResponse
      kExpectedResponse = CreateCupsAddManuallyConfiguredPrinterResponse(
          printscanmgr::AddPrinterResult::ADD_PRINTER_RESULT_SUCCESS);
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(kExpectedResponse));
  SetCupsAddManuallyConfiguredPrinterExpectation(response.get());

  base::RunLoop run_loop;
  GetClient()->CupsAddManuallyConfiguredPrinter(
      CreateCupsAddManuallyConfiguredPrinterRequest(),
      base::BindLambdaForTesting(
          [&](std::optional<
              printscanmgr::CupsAddManuallyConfiguredPrinterResponse> result) {
            ASSERT_TRUE(result.has_value());
            EXPECT_THAT(result.value(), EqualsProto(kExpectedResponse));
            run_loop.Quit();
          }));

  run_loop.Run();
}

// Test that the client handles a null response to a
// kCupsAddManuallyConfiguredPrinter D-Bus call.
TEST_F(PrintscanmgrClientTest, NullResponseToCupsAddManuallyConfiguredPrinter) {
  SetCupsAddManuallyConfiguredPrinterExpectation(nullptr);
  const printscanmgr::CupsAddManuallyConfiguredPrinterResponse
      kExpectedResponse = CreateCupsAddManuallyConfiguredPrinterResponse(
          printscanmgr::AddPrinterResult::ADD_PRINTER_RESULT_DBUS_GENERIC);

  base::RunLoop run_loop;
  GetClient()->CupsAddManuallyConfiguredPrinter(
      CreateCupsAddManuallyConfiguredPrinterRequest(),
      base::BindLambdaForTesting(
          [&](std::optional<
              printscanmgr::CupsAddManuallyConfiguredPrinterResponse> result) {
            ASSERT_TRUE(result.has_value());
            EXPECT_THAT(result.value(), EqualsProto(kExpectedResponse));
            run_loop.Quit();
          }));

  run_loop.Run();
}

// Test that the client handles a response to a
// kCupsAddManuallyConfiguredPrinter D-Bus call without a valid proto.
TEST_F(PrintscanmgrClientTest,
       EmptyResponseToCupsAddManuallyConfiguredPrinter) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  SetCupsAddManuallyConfiguredPrinterExpectation(response.get());
  const printscanmgr::CupsAddManuallyConfiguredPrinterResponse
      kExpectedResponse = CreateCupsAddManuallyConfiguredPrinterResponse(
          printscanmgr::AddPrinterResult::ADD_PRINTER_RESULT_DBUS_GENERIC);

  base::RunLoop run_loop;
  GetClient()->CupsAddManuallyConfiguredPrinter(
      CreateCupsAddManuallyConfiguredPrinterRequest(),
      base::BindLambdaForTesting(
          [&](std::optional<
              printscanmgr::CupsAddManuallyConfiguredPrinterResponse> result) {
            ASSERT_TRUE(result.has_value());
            EXPECT_THAT(result.value(), EqualsProto(kExpectedResponse));
            run_loop.Quit();
          }));

  run_loop.Run();
}

// Test that the client can request that cupsd adds an autoconfigured printer.
TEST_F(PrintscanmgrClientTest, CupsAddAutoConfiguredPrinter) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  const printscanmgr::CupsAddAutoConfiguredPrinterResponse kExpectedResponse =
      CreateCupsAddAutoConfiguredPrinterResponse(
          printscanmgr::AddPrinterResult::ADD_PRINTER_RESULT_SUCCESS);
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(kExpectedResponse));
  SetCupsAddAutoConfiguredPrinterExpectation(response.get());

  base::RunLoop run_loop;
  GetClient()->CupsAddAutoConfiguredPrinter(
      CreateCupsAddAutoConfiguredPrinterRequest(),
      base::BindLambdaForTesting(
          [&](std::optional<printscanmgr::CupsAddAutoConfiguredPrinterResponse>
                  result) {
            ASSERT_TRUE(result.has_value());
            EXPECT_THAT(result.value(), EqualsProto(kExpectedResponse));
            run_loop.Quit();
          }));

  run_loop.Run();
}

// Test that the client handles a null response to a
// kCupsAddAutoConfiguredPrinter D-Bus call.
TEST_F(PrintscanmgrClientTest, NullResponseToCupsAddAutoConfiguredPrinter) {
  SetCupsAddAutoConfiguredPrinterExpectation(nullptr);
  const printscanmgr::CupsAddAutoConfiguredPrinterResponse kExpectedResponse =
      CreateCupsAddAutoConfiguredPrinterResponse(
          printscanmgr::AddPrinterResult::ADD_PRINTER_RESULT_DBUS_GENERIC);

  base::RunLoop run_loop;
  GetClient()->CupsAddAutoConfiguredPrinter(
      CreateCupsAddAutoConfiguredPrinterRequest(),
      base::BindLambdaForTesting(
          [&](std::optional<printscanmgr::CupsAddAutoConfiguredPrinterResponse>
                  result) {
            ASSERT_TRUE(result.has_value());
            EXPECT_THAT(result.value(), EqualsProto(kExpectedResponse));
            run_loop.Quit();
          }));

  run_loop.Run();
}

// Test that the client handles a response to a kCupsAddAutoConfiguredPrinter
// D-Bus call without a valid proto.
TEST_F(PrintscanmgrClientTest, EmptyResponseToCupsAddAutoConfiguredPrinter) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  SetCupsAddAutoConfiguredPrinterExpectation(response.get());
  const printscanmgr::CupsAddAutoConfiguredPrinterResponse kExpectedResponse =
      CreateCupsAddAutoConfiguredPrinterResponse(
          printscanmgr::AddPrinterResult::ADD_PRINTER_RESULT_DBUS_GENERIC);

  base::RunLoop run_loop;
  GetClient()->CupsAddAutoConfiguredPrinter(
      CreateCupsAddAutoConfiguredPrinterRequest(),
      base::BindLambdaForTesting(
          [&](std::optional<printscanmgr::CupsAddAutoConfiguredPrinterResponse>
                  result) {
            ASSERT_TRUE(result.has_value());
            EXPECT_THAT(result.value(), EqualsProto(kExpectedResponse));
            run_loop.Quit();
          }));

  run_loop.Run();
}

// Test that the client can request that cupsd removes a printer.
TEST_F(PrintscanmgrClientTest, CupsRemovePrinter) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  const printscanmgr::CupsRemovePrinterResponse kExpectedResponse =
      CreateCupsRemovePrinterResponse();
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(kExpectedResponse));
  SetCupsRemovePrinterExpectation(response.get());

  base::RunLoop run_loop;
  bool error_callback_called = false;
  GetClient()->CupsRemovePrinter(
      CreateCupsRemovePrinterRequest(),
      base::BindLambdaForTesting(
          [&](std::optional<printscanmgr::CupsRemovePrinterResponse> result) {
            ASSERT_TRUE(result.has_value());
            EXPECT_THAT(result.value(), EqualsProto(kExpectedResponse));
            run_loop.Quit();
          }),
      base::BindLambdaForTesting([&]() { error_callback_called = true; }));

  run_loop.Run();

  EXPECT_FALSE(error_callback_called);
}

// Test that the client handles a null response to a kCupsRemovePrinter D-Bus
// call.
TEST_F(PrintscanmgrClientTest, NullResponseToCupsRemovePrinter) {
  SetCupsRemovePrinterExpectation(nullptr);

  base::RunLoop run_loop;
  bool callback_called = false;
  GetClient()->CupsRemovePrinter(
      CreateCupsRemovePrinterRequest(),
      base::BindLambdaForTesting(
          [&](std::optional<printscanmgr::CupsRemovePrinterResponse> result) {
            callback_called = true;
          }),
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

  run_loop.Run();

  EXPECT_FALSE(callback_called);
}

// Test that the client handles a response to a kCupsRemovePrinter D-Bus call
// without a valid proto.
TEST_F(PrintscanmgrClientTest, EmptyResponseToCupsRemovePrinter) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  SetCupsRemovePrinterExpectation(response.get());

  base::RunLoop run_loop;
  bool callback_called = false;
  GetClient()->CupsRemovePrinter(
      CreateCupsRemovePrinterRequest(),
      base::BindLambdaForTesting(
          [&](std::optional<printscanmgr::CupsRemovePrinterResponse> result) {
            callback_called = true;
          }),
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

  run_loop.Run();

  EXPECT_FALSE(callback_called);
}

// Test that the client can request a printer's PPD.
TEST_F(PrintscanmgrClientTest, CupsRetrievePpd) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  const printscanmgr::CupsRetrievePpdResponse kExpectedResponse =
      CreateCupsRetrievePpdResponse();
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(kExpectedResponse));
  SetCupsRetrievePpdExpectation(response.get());

  base::RunLoop run_loop;
  bool error_callback_called = false;
  GetClient()->CupsRetrievePrinterPpd(
      CreateCupsRetrievePpdRequest(),
      base::BindLambdaForTesting(
          [&](std::optional<printscanmgr::CupsRetrievePpdResponse> result) {
            ASSERT_TRUE(result.has_value());
            EXPECT_THAT(result.value(), EqualsProto(kExpectedResponse));
            run_loop.Quit();
          }),
      base::BindLambdaForTesting([&]() { error_callback_called = true; }));

  run_loop.Run();

  EXPECT_FALSE(error_callback_called);
}

// Test that the client handles a null response to a kCupsRetrievePpd D-Bus
// call.
TEST_F(PrintscanmgrClientTest, NullResponseToCupsRetrievePpd) {
  SetCupsRetrievePpdExpectation(nullptr);

  base::RunLoop run_loop;
  bool callback_called = false;
  GetClient()->CupsRetrievePrinterPpd(
      CreateCupsRetrievePpdRequest(),
      base::BindLambdaForTesting(
          [&](std::optional<printscanmgr::CupsRetrievePpdResponse> result) {
            callback_called = true;
          }),
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

  run_loop.Run();

  EXPECT_FALSE(callback_called);
}

// Test that the client handles a response to a kCupsRetrievePpd D-Bus call
// without a valid proto.
TEST_F(PrintscanmgrClientTest, EmptyResponseToCupsRetrievePpd) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  SetCupsRetrievePpdExpectation(response.get());
  const printscanmgr::CupsRetrievePpdResponse kExpectedResponse =
      CreateCupsRetrievePpdResponse();

  base::RunLoop run_loop;
  bool callback_called = false;
  GetClient()->CupsRetrievePrinterPpd(
      CreateCupsRetrievePpdRequest(),
      base::BindLambdaForTesting(
          [&](std::optional<printscanmgr::CupsRetrievePpdResponse> result) {
            callback_called = true;
          }),
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

  run_loop.Run();

  EXPECT_FALSE(callback_called);
}

}  // namespace ash
