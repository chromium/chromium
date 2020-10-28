// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/lorgnette_manager/lorgnette_manager_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

namespace chromeos {

namespace {

// Fake scanner name used in the tests.
constexpr char kScannerDeviceName[] = "test:MX3100_192.168.0.3";

// Convenience method for creating a lorgnette::ListScannersResponse.
lorgnette::ListScannersResponse CreateListScannersResponse() {
  lorgnette::ScannerInfo scanner;
  scanner.set_name("Name");
  scanner.set_manufacturer("Manufacturer");
  scanner.set_model("Model");
  scanner.set_type("Type");

  lorgnette::ListScannersResponse response;
  *response.add_scanners() = std::move(scanner);

  return response;
}

// Convenience method for creating a lorgnette::ScannerCapabilities.
lorgnette::ScannerCapabilities CreateScannerCapabilities() {
  lorgnette::ScannableArea area;
  area.set_width(8.5);
  area.set_height(11.0);

  lorgnette::DocumentSource source;
  source.set_type(lorgnette::SourceType::SOURCE_ADF_SIMPLEX);
  source.set_name("Source name");
  *source.mutable_area() = std::move(area);

  lorgnette::ScannerCapabilities capabilities;
  capabilities.add_resolutions(100);
  *capabilities.add_sources() = std::move(source);
  capabilities.add_color_modes(lorgnette::ColorMode::MODE_COLOR);

  return capabilities;
}

// Matcher that verifies that a dbus::Message has member |name|.
MATCHER_P(HasMember, name, "") {
  if (arg->GetMember() != name) {
    *result_listener << "has member " << arg->GetMember();
    return false;
  }
  return true;
}

// Matcher that veries two protobufs contain the same data.
MATCHER_P(ProtobufEquals, expected_message, "") {
  std::string arg_dumped;
  arg.SerializeToString(&arg_dumped);
  std::string expected_message_dumped;
  expected_message.SerializeToString(&expected_message_dumped);
  return arg_dumped == expected_message_dumped;
}

}  // namespace

class LorgnetteManagerClientTest : public testing::Test {
 public:
  LorgnetteManagerClientTest() = default;
  LorgnetteManagerClientTest(const LorgnetteManagerClientTest&) = delete;
  LorgnetteManagerClientTest& operator=(const LorgnetteManagerClientTest&) =
      delete;

  void SetUp() override {
    // Create a mock bus.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options);

    // Create a mock lorgnette daemon proxy.
    mock_proxy_ = new dbus::MockObjectProxy(
        mock_bus_.get(), lorgnette::kManagerServiceName,
        dbus::ObjectPath(lorgnette::kManagerServicePath));

    // |client_|'s Init() method should request a proxy for communicating with
    // the lorgnette daemon.
    EXPECT_CALL(
        *mock_bus_.get(),
        GetObjectProxy(lorgnette::kManagerServiceName,
                       dbus::ObjectPath(lorgnette::kManagerServicePath)))
        .WillOnce(Return(mock_proxy_.get()));

    // Save |client_|'s scan status changed signal callback.
    EXPECT_CALL(*mock_proxy_.get(),
                DoConnectToSignal(lorgnette::kManagerServiceInterface,
                                  lorgnette::kScanStatusChangedSignal, _, _))
        .WillOnce(WithArgs<2, 3>(Invoke(
            [&](dbus::ObjectProxy::SignalCallback signal_callback,
                dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
              scan_status_changed_signal_callback_ = std::move(signal_callback);

              task_environment_.GetMainThreadTaskRunner()->PostTask(
                  FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                            lorgnette::kManagerServiceInterface,
                                            lorgnette::kScanStatusChangedSignal,
                                            /*success=*/true));
            })));

    // ShutdownAndBlock() will be called in TearDown().
    EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock()).WillOnce(Return());

    // Create and initialize a client with the mock bus.
    client_ = LorgnetteManagerClient::Create();
    client_->Init(mock_bus_.get());

    // Execute callbacks posted by Init().
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { mock_bus_->ShutdownAndBlock(); }

  LorgnetteManagerClient* client() { return client_.get(); }

  // Adds an expectation to |mock_proxy_| that kListScannersMethod will be
  // called. When called, |mock_proxy_| will respond with |response|.
  void SetListScannersExpectation(dbus::Response* response) {
    response_ = response;
    EXPECT_CALL(*mock_proxy_.get(),
                DoCallMethod(HasMember(lorgnette::kListScannersMethod),
                             dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
        .WillOnce(
            Invoke(this, &LorgnetteManagerClientTest::OnCallListScanners));
  }

  // Adds an expectation to |mock_proxy_| that kGetScannerCapabilitiesMethod
  // will be called. When called, |mock_proxy_| will respond with |response|.
  void SetGetScannerCapabilitiesExpectation(dbus::Response* response) {
    response_ = response;
    EXPECT_CALL(
        *mock_proxy_.get(),
        DoCallMethod(HasMember(lorgnette::kGetScannerCapabilitiesMethod),
                     dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
        .WillOnce(Invoke(
            this, &LorgnetteManagerClientTest::OnCallGetScannerCapabilities));
  }

  // Tells |mock_proxy_| to emit a kScanStatusChangedSignal with the given
  // |uuid|, |state|, |page|, |progress| and |more_pages|.
  void EmitScanStatusChangedSignal(const std::string& uuid,
                                   const lorgnette::ScanState state,
                                   const int page,
                                   const int progress,
                                   const bool more_pages) {
    lorgnette::ScanStatusChangedSignal proto;
    proto.set_scan_uuid(uuid);
    proto.set_state(state);
    proto.set_page(page);
    proto.set_progress(progress);
    proto.set_more_pages(more_pages);

    dbus::Signal signal(lorgnette::kManagerServiceInterface,
                        lorgnette::kScanStatusChangedSignal);
    dbus::MessageWriter(&signal).AppendProtoAsArrayOfBytes(proto);

    scan_status_changed_signal_callback_.Run(&signal);
  }

  // Tells |mock_proxy_| to emit a kScanStatusChangedSignal with no data. This
  // is useful for testing failure cases.
  void EmitEmptyScanStatusChangedSignal() {
    dbus::Signal signal(lorgnette::kManagerServiceInterface,
                        lorgnette::kScanStatusChangedSignal);
    scan_status_changed_signal_callback_.Run(&signal);
  }

 private:
  // Responsible for responding to a kListScannersMethod call.
  void OnCallListScanners(dbus::MethodCall* method_call,
                          int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*callback), response_));
  }

  // Responsible for responding to a kGetScannerCapabilitiesMethod call, and
  // verifying that |method_call| was formatted correctly.
  void OnCallGetScannerCapabilities(
      dbus::MethodCall* method_call,
      int timeout_ms,
      dbus::ObjectProxy::ResponseCallback* callback) {
    // Verify that the scanner name was sent correctly.
    std::string device_name;
    ASSERT_TRUE(dbus::MessageReader(method_call).PopString(&device_name));
    EXPECT_EQ(device_name, kScannerDeviceName);
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*callback), response_));
  }

  // A message loop to emulate asynchronous behavior.
  base::test::SingleThreadTaskEnvironment task_environment_;
  // Mock D-Bus objects for |client_| to interact with.
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
  // The client to be tested.
  std::unique_ptr<LorgnetteManagerClient> client_;
  // Holds |client_|'s kScanStatusChangedSignal callback.
  dbus::ObjectProxy::SignalCallback scan_status_changed_signal_callback_;
  // Used to respond to D-Bus method calls.
  dbus::Response* response_ = nullptr;
};

// Test that the client can retrieve a list of scanners.
TEST_F(LorgnetteManagerClientTest, ListScanners) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  const lorgnette::ListScannersResponse kExpectedResponse =
      CreateListScannersResponse();
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(kExpectedResponse));
  SetListScannersExpectation(response.get());

  base::RunLoop run_loop;
  client()->ListScanners(base::BindLambdaForTesting(
      [&](base::Optional<lorgnette::ListScannersResponse> result) {
        ASSERT_TRUE(result.has_value());
        EXPECT_THAT(result.value(), ProtobufEquals(kExpectedResponse));
        run_loop.Quit();
      }));

  run_loop.Run();
}

// Test that the client handles a null response to a kListScannersMethod D-Bus
// call.
TEST_F(LorgnetteManagerClientTest, NullResponseToListScanners) {
  SetListScannersExpectation(nullptr);

  base::RunLoop run_loop;
  client()->ListScanners(base::BindLambdaForTesting(
      [&](base::Optional<lorgnette::ListScannersResponse> result) {
        EXPECT_EQ(result, base::nullopt);
        run_loop.Quit();
      }));

  run_loop.Run();
}

// Test that the client handles a response to a kListScannersMethod D-Bus call
// without a valid proto.
TEST_F(LorgnetteManagerClientTest, EmptyResponseToListScanners) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  SetListScannersExpectation(response.get());

  base::RunLoop run_loop;
  client()->ListScanners(base::BindLambdaForTesting(
      [&](base::Optional<lorgnette::ListScannersResponse> result) {
        EXPECT_EQ(result, base::nullopt);
        run_loop.Quit();
      }));

  run_loop.Run();
}

// Test that the client can retrieve capabilities for a scanner.
TEST_F(LorgnetteManagerClientTest, GetScannerCapabilities) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  const lorgnette::ScannerCapabilities kExpectedResponse =
      CreateScannerCapabilities();
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(kExpectedResponse));
  SetGetScannerCapabilitiesExpectation(response.get());

  base::RunLoop run_loop;
  client()->GetScannerCapabilities(
      kScannerDeviceName,
      base::BindLambdaForTesting(
          [&](base::Optional<lorgnette::ScannerCapabilities> result) {
            ASSERT_TRUE(result.has_value());
            EXPECT_THAT(result.value(), ProtobufEquals(kExpectedResponse));
            run_loop.Quit();
          }));

  run_loop.Run();
}

// Test that the client handles a null response to a
// kGetScannerCapabilitiesMethod D-Bus call.
TEST_F(LorgnetteManagerClientTest, NullResponseToGetScannerCapabilities) {
  SetGetScannerCapabilitiesExpectation(nullptr);

  base::RunLoop run_loop;
  client()->GetScannerCapabilities(
      kScannerDeviceName,
      base::BindLambdaForTesting(
          [&](base::Optional<lorgnette::ScannerCapabilities> result) {
            EXPECT_EQ(result, base::nullopt);
            run_loop.Quit();
          }));

  run_loop.Run();
}

// Test that the client handles a response to a kGetScannerCapabilitiesMethod
// D-Bus call without a valid proto.
TEST_F(LorgnetteManagerClientTest, EmptyResponseToGetScannerCapabilities) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  SetGetScannerCapabilitiesExpectation(response.get());

  base::RunLoop run_loop;
  client()->GetScannerCapabilities(
      kScannerDeviceName,
      base::BindLambdaForTesting(
          [&](base::Optional<lorgnette::ScannerCapabilities> result) {
            EXPECT_EQ(result, base::nullopt);
            run_loop.Quit();
          }));

  run_loop.Run();
}

// Test that the client handles an unexpected kScanStatusChangedSignal.
TEST_F(LorgnetteManagerClientTest, UnexpectedScanStatusChangedSignal) {
  EmitScanStatusChangedSignal(
      "uuid", lorgnette::ScanState::SCAN_STATE_PAGE_COMPLETED, /*page=*/1,
      /*progress=*/100,
      /*more_pages=*/true);
}

// Test that the client handles a kScanStatusChangedSignal without a valid
// proto.
TEST_F(LorgnetteManagerClientTest, EmptyScanStatusChangedSignal) {
  EmitEmptyScanStatusChangedSignal();
}

}  // namespace chromeos
