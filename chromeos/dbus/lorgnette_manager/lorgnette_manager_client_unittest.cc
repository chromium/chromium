// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/lorgnette_manager/lorgnette_manager_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
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
// Fake scan UUID used in the tests.
constexpr char kScanUuid[] = "uuid";

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

// Convenience method for creating a lorgnette::StartScanRequest.
lorgnette::StartScanRequest CreateStartScanRequest() {
  lorgnette::ScanRegion region;
  region.set_top_left_x(10);
  region.set_top_left_y(12);
  region.set_bottom_right_x(90);
  region.set_bottom_right_y(80);

  lorgnette::ScanSettings settings;
  settings.set_resolution(1080);
  settings.set_color_mode(lorgnette::ColorMode::MODE_GRAYSCALE);
  settings.set_source_name("Source name, round 2");
  *settings.mutable_scan_region() = std::move(region);

  lorgnette::StartScanRequest request;
  request.set_device_name(kScannerDeviceName);
  *request.mutable_settings() = std::move(settings);

  return request;
}

// Convenience method for creating a lorgnette::StartScanResponse with the given
// |state|. If |state| == lorgnette::ScanState::SCAN_STATE_FAILED, this method
// will add an appropriate failure reason.
lorgnette::StartScanResponse CreateStartScanResponse(
    lorgnette::ScanState state) {
  lorgnette::StartScanResponse response;
  response.set_state(state);
  response.set_scan_uuid(kScanUuid);

  if (state == lorgnette::ScanState::SCAN_STATE_FAILED) {
    response.set_failure_reason(
        "The small elf inside the scanner is feeling overworked.");
  }

  return response;
}

// Convenience method for creating a lorgnette::GetNextImageRequest.
lorgnette::GetNextImageRequest CreateGetNextImageRequest() {
  lorgnette::GetNextImageRequest request;
  request.set_scan_uuid(kScanUuid);
  return request;
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
    list_scanners_response_ = response;
    EXPECT_CALL(*mock_proxy_.get(),
                DoCallMethod(HasMember(lorgnette::kListScannersMethod),
                             dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
        .WillOnce(
            Invoke(this, &LorgnetteManagerClientTest::OnCallListScanners));
  }

  // Adds an expectation to |mock_proxy_| that kGetScannerCapabilitiesMethod
  // will be called. When called, |mock_proxy_| will respond with |response|.
  void SetGetScannerCapabilitiesExpectation(dbus::Response* response) {
    get_scanner_capabilities_response_ = response;
    EXPECT_CALL(
        *mock_proxy_.get(),
        DoCallMethod(HasMember(lorgnette::kGetScannerCapabilitiesMethod),
                     dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
        .WillOnce(Invoke(
            this, &LorgnetteManagerClientTest::OnCallGetScannerCapabilities));
  }

  // Adds an expectation to |mock_proxy_| that kStartScanMethod will be called.
  // When called, |mock_proxy_| will respond with |response|.
  void SetStartScanExpectation(dbus::Response* response) {
    start_scan_response_ = response;
    EXPECT_CALL(*mock_proxy_.get(),
                DoCallMethod(HasMember(lorgnette::kStartScanMethod),
                             dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
        .WillOnce(Invoke(this, &LorgnetteManagerClientTest::OnCallStartScan));
  }

  // Adds an expectation to |mock_proxy_| that kGetNextImageMethod will be
  // called. When called, |mock_proxy_| will respond with |response|.
  void SetGetNextImageExpectation(dbus::Response* response) {
    get_next_image_response_ = response;
    EXPECT_CALL(*mock_proxy_.get(),
                DoCallMethod(HasMember(lorgnette::kGetNextImageMethod),
                             dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
        .WillOnce(
            Invoke(this, &LorgnetteManagerClientTest::OnCallGetNextImage));
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
        FROM_HERE,
        base::BindOnce(std::move(*callback), list_scanners_response_));
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
        FROM_HERE, base::BindOnce(std::move(*callback),
                                  get_scanner_capabilities_response_));
  }

  // Responsible for responding to a kStartScanMethod call and verifying that
  // |method_call| was formatted correctly.
  void OnCallStartScan(dbus::MethodCall* method_call,
                       int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    // Verify that the start scan request was created and sent correctly.
    lorgnette::StartScanRequest request;
    ASSERT_TRUE(
        dbus::MessageReader(method_call).PopArrayOfBytesAsProto(&request));
    EXPECT_THAT(request, ProtobufEquals(CreateStartScanRequest()));
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*callback), start_scan_response_));
  }

  // Responsible for responding to a kGetNextImageMethod call and verifying that
  // |method_call| was formatted correctly.
  void OnCallGetNextImage(dbus::MethodCall* method_call,
                          int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
    // Verify that the get next image request was sent correctly.
    lorgnette::GetNextImageRequest request;
    ASSERT_TRUE(
        dbus::MessageReader(method_call).PopArrayOfBytesAsProto(&request));
    EXPECT_THAT(request, ProtobufEquals(CreateGetNextImageRequest()));
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(*callback), get_next_image_response_));
  }

  // A message loop to emulate asynchronous behavior.
  base::test::TaskEnvironment task_environment_;
  // Mock D-Bus objects for |client_| to interact with.
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
  // The client to be tested.
  std::unique_ptr<LorgnetteManagerClient> client_;
  // Holds |client_|'s kScanStatusChangedSignal callback.
  dbus::ObjectProxy::SignalCallback scan_status_changed_signal_callback_;
  // Used to respond to kListScannersMethod D-Bus calls.
  dbus::Response* list_scanners_response_ = nullptr;
  // Used to respond to kGetScannerCapabilitiesMethod D-Bus calls.
  dbus::Response* get_scanner_capabilities_response_ = nullptr;
  // Used to respond to kStartScanMethod D-Bus calls.
  dbus::Response* start_scan_response_ = nullptr;
  // Used to respond to kGetNextImageMethod D-Bus calls.
  dbus::Response* get_next_image_response_ = nullptr;
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

// Test that the client handles a null response to a kStartScanMethod D-Bus
// call.
TEST_F(LorgnetteManagerClientTest, NullResponseToStartScan) {
  SetStartScanExpectation(nullptr);

  base::RunLoop run_loop;
  lorgnette::StartScanRequest request = CreateStartScanRequest();
  client()->StartScan(request.device_name(), request.settings(),
                      base::BindLambdaForTesting([&](bool completed) {
                        EXPECT_FALSE(completed);
                        run_loop.Quit();
                      }),
                      base::NullCallback(), base::NullCallback());

  run_loop.Run();
}

// Test that the client handles a response to a kStartScanMethod D-Bus call
// without a valid proto.
TEST_F(LorgnetteManagerClientTest, EmptyResponseToStartScan) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  SetStartScanExpectation(response.get());

  base::RunLoop run_loop;
  lorgnette::StartScanRequest request = CreateStartScanRequest();
  client()->StartScan(request.device_name(), request.settings(),
                      base::BindLambdaForTesting([&](bool completed) {
                        EXPECT_FALSE(completed);
                        run_loop.Quit();
                      }),
                      base::NullCallback(), base::NullCallback());

  run_loop.Run();
}

// Test that the client handles a response to a kStartScanMethod D-Bus call
// with state lorgnette::SCAN_STATE_FAILED.
TEST_F(LorgnetteManagerClientTest, StartScanScanStateFailed) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(CreateStartScanResponse(
                      lorgnette::ScanState::SCAN_STATE_FAILED)));
  SetStartScanExpectation(response.get());

  base::RunLoop run_loop;
  lorgnette::StartScanRequest request = CreateStartScanRequest();
  client()->StartScan(request.device_name(), request.settings(),
                      base::BindLambdaForTesting([&](bool completed) {
                        EXPECT_FALSE(completed);
                        run_loop.Quit();
                      }),
                      base::NullCallback(), base::NullCallback());

  run_loop.Run();
}

// Test that the client handles a null response to a kGetNextImageMethod D-Bus
// call.
TEST_F(LorgnetteManagerClientTest, NullResponseToGetNextImage) {
  std::unique_ptr<dbus::Response> start_scan_response =
      dbus::Response::CreateEmpty();
  ASSERT_TRUE(dbus::MessageWriter(start_scan_response.get())
                  .AppendProtoAsArrayOfBytes(CreateStartScanResponse(
                      lorgnette::ScanState::SCAN_STATE_IN_PROGRESS)));
  SetStartScanExpectation(start_scan_response.get());
  SetGetNextImageExpectation(nullptr);

  base::RunLoop run_loop;
  lorgnette::StartScanRequest request = CreateStartScanRequest();
  client()->StartScan(request.device_name(), request.settings(),
                      base::BindLambdaForTesting([&](bool completed) {
                        EXPECT_FALSE(completed);
                        run_loop.Quit();
                      }),
                      base::NullCallback(), base::NullCallback());

  run_loop.Run();
}

// Test that the client handles a response to a kGetNextImageMethod D-Bus call
// without a valid proto.
TEST_F(LorgnetteManagerClientTest, EmptyResponseToGetNextImage) {
  std::unique_ptr<dbus::Response> start_scan_response =
      dbus::Response::CreateEmpty();
  ASSERT_TRUE(dbus::MessageWriter(start_scan_response.get())
                  .AppendProtoAsArrayOfBytes(CreateStartScanResponse(
                      lorgnette::ScanState::SCAN_STATE_IN_PROGRESS)));
  SetStartScanExpectation(start_scan_response.get());
  std::unique_ptr<dbus::Response> get_next_image_response =
      dbus::Response::CreateEmpty();
  SetGetNextImageExpectation(get_next_image_response.get());

  base::RunLoop run_loop;
  lorgnette::StartScanRequest request = CreateStartScanRequest();
  client()->StartScan(request.device_name(), request.settings(),
                      base::BindLambdaForTesting([&](bool completed) {
                        EXPECT_FALSE(completed);
                        run_loop.Quit();
                      }),
                      base::NullCallback(), base::NullCallback());

  run_loop.Run();
}

// Test that the client handles a response to a kGetNextImageMethod D-Bus call
// with state lorgnette::SCAN_STATE_FAILED.
TEST_F(LorgnetteManagerClientTest, GetNextImageScanStateFailed) {
  std::unique_ptr<dbus::Response> start_scan_response =
      dbus::Response::CreateEmpty();
  ASSERT_TRUE(dbus::MessageWriter(start_scan_response.get())
                  .AppendProtoAsArrayOfBytes(CreateStartScanResponse(
                      lorgnette::ScanState::SCAN_STATE_IN_PROGRESS)));
  SetStartScanExpectation(start_scan_response.get());
  std::unique_ptr<dbus::Response> get_next_image_response =
      dbus::Response::CreateEmpty();
  ASSERT_TRUE(dbus::MessageWriter(get_next_image_response.get())
                  .AppendProtoAsArrayOfBytes(CreateStartScanResponse(
                      lorgnette::ScanState::SCAN_STATE_FAILED)));
  SetGetNextImageExpectation(get_next_image_response.get());

  base::RunLoop run_loop;
  lorgnette::StartScanRequest request = CreateStartScanRequest();
  client()->StartScan(request.device_name(), request.settings(),
                      base::BindLambdaForTesting([&](bool completed) {
                        EXPECT_FALSE(completed);
                        run_loop.Quit();
                      }),
                      base::NullCallback(), base::NullCallback());

  run_loop.Run();
}

// Test that the client handles an unexpected kScanStatusChangedSignal.
TEST_F(LorgnetteManagerClientTest, UnexpectedScanStatusChangedSignal) {
  EmitScanStatusChangedSignal(
      kScanUuid, lorgnette::ScanState::SCAN_STATE_PAGE_COMPLETED, /*page=*/1,
      /*progress=*/100,
      /*more_pages=*/true);
}

// Test that the client handles a kScanStatusChangedSignal without a valid
// proto.
TEST_F(LorgnetteManagerClientTest, EmptyScanStatusChangedSignal) {
  EmitEmptyScanStatusChangedSignal();
}

}  // namespace chromeos
