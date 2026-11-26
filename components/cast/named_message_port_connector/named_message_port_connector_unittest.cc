// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/named_message_port_connector/named_message_port_connector.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/cast/message_port/message_port.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast/message_port/test_message_port_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_api_bindings {
namespace {

std::string ExtractTokenFromConnectMessage(std::string_view connect_message) {
  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(connect_message, base::JSON_PARSE_RFC);
  CHECK(dict.has_value());
  const std::string* token = dict->FindString("token");
  CHECK(token);
  return *token;
}

std::string BuildConnectorPayload(std::string_view port_id,
                                  std::string_view token) {
  base::DictValue payload;
  payload.Set("portId", port_id);
  payload.Set("token", token);
  std::string serialized;
  base::JSONWriter::Write(payload, &serialized);
  return serialized;
}

std::vector<std::unique_ptr<MessagePort>> CreateSingleTransferredPort(
    std::unique_ptr<MessagePort>* out_page_port) {
  std::unique_ptr<MessagePort> host_port;
  CreatePlatformMessagePortPair(&host_port, out_page_port);
  std::vector<std::unique_ptr<MessagePort>> transferred_ports;
  transferred_ports.push_back(std::move(host_port));
  return transferred_ports;
}

class NamedMessagePortConnectorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    connector_.RegisterPortHandler(
        base::BindRepeating(&NamedMessagePortConnectorTest::OnPortConnected,
                            base::Unretained(this)));
  }

  bool OnPortConnected(std::string_view service_name,
                       std::unique_ptr<MessagePort> port) {
    connected_service_names_.emplace_back(service_name);
    connected_service_port_ = std::move(port);
    if (quit_on_port_) {
      std::move(quit_on_port_).Run();
    }
    return true;
  }

  void WaitForPortConnected() {
    base::RunLoop run_loop;
    quit_on_port_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  NamedMessagePortConnector connector_;
  std::vector<std::string> connected_service_names_;
  std::unique_ptr<MessagePort> connected_service_port_;
  base::OnceClosure quit_on_port_;
};

TEST_F(NamedMessagePortConnectorTest,
       InvokesOnConnectedCallbackOnMasterConnectedAck) {
  TestMessagePortReceiver connect_receiver;
  std::string connect_message;
  std::unique_ptr<MessagePort> connect_port;
  connector_.GetConnectMessage(&connect_message, &connect_port);
  connect_port->SetReceiver(&connect_receiver);
  const std::string token = ExtractTokenFromConnectMessage(connect_message);

  int on_connected_count = 0;
  base::RunLoop ack_loop;
  connector_.RegisterOnConnectedCallback(base::BindOnce(
      [](int* count, base::OnceClosure quit) {
        ++(*count);
        std::move(quit).Run();
      },
      &on_connected_count, ack_loop.QuitClosure()));

  ASSERT_TRUE(connect_port->PostMessage(
      BuildConnectorPayload("cast.master.connected", token)));
  ack_loop.Run();
  EXPECT_EQ(on_connected_count, 1);
  EXPECT_TRUE(connected_service_names_.empty());

  std::unique_ptr<MessagePort> page_service_port;
  ASSERT_TRUE(connect_port->PostMessageWithTransferables(
      BuildConnectorPayload("cast.__platform__.cryptokeys", token),
      CreateSingleTransferredPort(&page_service_port)));
  WaitForPortConnected();

  EXPECT_EQ(on_connected_count, 1);
  ASSERT_EQ(connected_service_names_.size(), 1u);
  EXPECT_EQ(connected_service_names_[0], "cast.__platform__.cryptokeys");
  EXPECT_TRUE(connected_service_port_);
}

TEST_F(NamedMessagePortConnectorTest,
       InvokesOnConnectedCallbackOnDirectPortConnection) {
  TestMessagePortReceiver connect_receiver;
  std::string connect_message;
  std::unique_ptr<MessagePort> connect_port;
  connector_.GetConnectMessage(&connect_message, &connect_port);
  connect_port->SetReceiver(&connect_receiver);
  const std::string token = ExtractTokenFromConnectMessage(connect_message);

  int on_connected_count = 0;
  connector_.RegisterOnConnectedCallback(
      base::BindOnce([](int* count) { ++(*count); }, &on_connected_count));

  std::unique_ptr<MessagePort> page_service_port;
  ASSERT_TRUE(connect_port->PostMessageWithTransferables(
      BuildConnectorPayload("cast.__platform__.cryptokeys", token),
      CreateSingleTransferredPort(&page_service_port)));
  WaitForPortConnected();

  EXPECT_EQ(on_connected_count, 1);
  ASSERT_EQ(connected_service_names_.size(), 1u);
  EXPECT_EQ(connected_service_names_[0], "cast.__platform__.cryptokeys");
}

TEST_F(NamedMessagePortConnectorTest, RejectsPortWithStaleGenerationToken) {
  std::string connect_message_1;
  std::unique_ptr<MessagePort> connect_port_1;
  connector_.GetConnectMessage(&connect_message_1, &connect_port_1);
  const std::string stale_token =
      ExtractTokenFromConnectMessage(connect_message_1);

  TestMessagePortReceiver connect_receiver_2;
  std::string connect_message_2;
  std::unique_ptr<MessagePort> connect_port_2;
  connector_.GetConnectMessage(&connect_message_2, &connect_port_2);
  connect_port_2->SetReceiver(&connect_receiver_2);

  bool on_connected_called = false;
  connector_.RegisterOnConnectedCallback(base::BindOnce(
      [](bool* called) { *called = true; }, &on_connected_called));

  std::unique_ptr<MessagePort> stale_page_port;
  ASSERT_TRUE(connect_port_2->PostMessageWithTransferables(
      BuildConnectorPayload("stale_service", stale_token),
      CreateSingleTransferredPort(&stale_page_port)));
  connect_receiver_2.RunUntilDisconnected();

  EXPECT_FALSE(on_connected_called);
  EXPECT_TRUE(connected_service_names_.empty());
}

TEST_F(NamedMessagePortConnectorTest,
       ResetClosesControlPortAndClearsPendingCallback) {
  TestMessagePortReceiver connect_receiver;
  std::string connect_message;
  std::unique_ptr<MessagePort> connect_port;
  connector_.GetConnectMessage(&connect_message, &connect_port);
  connect_port->SetReceiver(&connect_receiver);

  bool callback_called = false;
  connector_.RegisterOnConnectedCallback(
      base::BindOnce([](bool* called) { *called = true; }, &callback_called));

  connector_.Reset();
  connect_receiver.RunUntilDisconnected();
  EXPECT_FALSE(callback_called);
}

TEST_F(NamedMessagePortConnectorTest,
       ResetsPendingOnConnectedCallbackOnNewConnectMessage) {
  std::string connect_message_1;
  std::unique_ptr<MessagePort> connect_port_1;
  connector_.GetConnectMessage(&connect_message_1, &connect_port_1);

  bool first_callback_called = false;
  connector_.RegisterOnConnectedCallback(base::BindOnce(
      [](bool* called) { *called = true; }, &first_callback_called));

  TestMessagePortReceiver connect_receiver_2;
  std::string connect_message_2;
  std::unique_ptr<MessagePort> connect_port_2;
  connector_.GetConnectMessage(&connect_message_2, &connect_port_2);
  connect_port_2->SetReceiver(&connect_receiver_2);
  const std::string second_token =
      ExtractTokenFromConnectMessage(connect_message_2);

  std::unique_ptr<MessagePort> page_service_port;
  ASSERT_TRUE(connect_port_2->PostMessageWithTransferables(
      BuildConnectorPayload("cast.__platform__.cryptokeys", second_token),
      CreateSingleTransferredPort(&page_service_port)));
  WaitForPortConnected();

  EXPECT_FALSE(first_callback_called);
  ASSERT_EQ(connected_service_names_.size(), 1u);
}

}  // namespace
}  // namespace cast_api_bindings
