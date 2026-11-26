// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/api_bindings/manager.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/cast/api_bindings/scoped_api_binding.h"
#include "components/cast/message_port/message_port.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast/message_port/test_message_port_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_api_bindings {
namespace {

constexpr std::string_view kTestPortName = "cast.__platform__.cryptokeys";
constexpr std::string_view kTestBindingId = "cryptokeys";
constexpr std::string_view kTestBindingScript = "// test script";

class FakeManager : public Manager {
 public:
  FakeManager() = default;
  ~FakeManager() override = default;

  void AddBinding(std::string_view binding_name,
                  std::string_view binding_script) override {
    bindings_.emplace_back(binding_name, binding_script);
  }

  bool ConnectPort(std::string_view port_name,
                   std::unique_ptr<MessagePort> port) {
    return OnPortConnected(port_name, std::move(port));
  }

  void UpdateOrigin(std::string origin) { SetOrigin(std::move(origin)); }

 private:
  std::vector<std::pair<std::string, std::string>> bindings_;
};

class FakeBindingDelegate : public ScopedApiBinding::Delegate {
 public:
  explicit FakeBindingDelegate(std::string_view port_name)
      : port_name_(port_name) {}
  ~FakeBindingDelegate() override = default;

  std::string_view GetPortName() const override { return port_name_; }

  void OnConnected() override { ++connected_count_; }

  bool OnMessage(std::string_view message) override {
    received_messages_.emplace_back(message);
    if (quit_on_message_) {
      std::move(quit_on_message_).Run();
    }
    return true;
  }

  void OnDisconnected() override {
    ++disconnected_count_;
    if (on_disconnected_callback_) {
      std::move(on_disconnected_callback_).Run();
    }
  }

  void SetOnDisconnectedCallback(base::OnceClosure callback) {
    on_disconnected_callback_ = std::move(callback);
  }

  void WaitForMessage() {
    base::RunLoop run_loop;
    quit_on_message_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  int connected_count() const { return connected_count_; }
  int disconnected_count() const { return disconnected_count_; }
  const std::vector<std::string>& received_messages() const {
    return received_messages_;
  }

 private:
  std::string port_name_;
  int connected_count_ = 0;
  int disconnected_count_ = 0;
  std::vector<std::string> received_messages_;
  base::OnceClosure quit_on_message_;
  base::OnceClosure on_disconnected_callback_;
};

class ChildErrorReceiver : public MessagePort::Receiver {
 public:
  ChildErrorReceiver() = default;
  ~ChildErrorReceiver() override = default;

  bool OnMessage(std::string_view message,
                 std::vector<std::unique_ptr<MessagePort>> ports) override {
    return true;
  }

  void OnPipeError() override { ++pipe_errors_; }

  int pipe_errors() const { return pipe_errors_; }

 private:
  int pipe_errors_ = 0;
};

class MultiPortReceiver : public MessagePort::Receiver {
 public:
  MultiPortReceiver() = default;
  ~MultiPortReceiver() override = default;

  bool OnMessage(std::string_view message,
                 std::vector<std::unique_ptr<MessagePort>> ports) override {
    if (!ports.empty()) {
      child_port_ = std::move(ports[0]);
      child_port_->SetReceiver(&child_receiver_);
    }
    if (quit_on_message_) {
      std::move(quit_on_message_).Run();
    }
    return true;
  }

  void OnPipeError() override { ++parent_pipe_errors_; }

  void WaitForChildPort() {
    if (child_port_) {
      return;
    }
    base::RunLoop run_loop;
    quit_on_message_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  MessagePort* child_port() { return child_port_.get(); }
  int child_pipe_errors() const { return child_receiver_.pipe_errors(); }
  int parent_pipe_errors() const { return parent_pipe_errors_; }

 private:
  ChildErrorReceiver child_receiver_;
  std::unique_ptr<MessagePort> child_port_;
  int parent_pipe_errors_ = 0;
  base::OnceClosure quit_on_message_;
};

class ManagerTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  FakeManager manager_;
};

TEST_F(ManagerTest, DisconnectsConnectedPortsWhenOriginChanges) {
  FakeBindingDelegate delegate(kTestPortName);
  ScopedApiBinding binding(&manager_, &delegate, kTestBindingId,
                           kTestBindingScript);

  manager_.UpdateOrigin("https://b.test");
  EXPECT_EQ(manager_.origin(), "https://b.test");

  TestMessagePortReceiver client_receiver;
  std::unique_ptr<MessagePort> client_port;
  std::unique_ptr<MessagePort> server_port;
  CreatePlatformMessagePortPair(&client_port, &server_port);
  client_port->SetReceiver(&client_receiver);

  ASSERT_TRUE(manager_.ConnectPort(kTestPortName, std::move(server_port)));
  EXPECT_EQ(delegate.connected_count(), 1);
  EXPECT_EQ(delegate.disconnected_count(), 0);

  ASSERT_TRUE(client_port->PostMessage("ping_from_b"));
  delegate.WaitForMessage();
  ASSERT_EQ(delegate.received_messages().size(), 1u);
  EXPECT_EQ(delegate.received_messages()[0], "ping_from_b");

  ASSERT_TRUE(binding.SendMessage("pong_to_b"));
  ASSERT_TRUE(client_receiver.RunUntilMessageCountEqual(1));
  EXPECT_EQ(client_receiver.buffer()[0].first, "pong_to_b");

  // Navigating away clears or changes the origin and disconnects active ports.
  manager_.UpdateOrigin("");
  EXPECT_EQ(manager_.origin(), "");
  EXPECT_EQ(delegate.disconnected_count(), 1);
  EXPECT_FALSE(binding.SendMessage("dropped_after_origin_reset"));
  client_receiver.RunUntilDisconnected();
}

TEST_F(ManagerTest, AllowsReconnectingNewPortAfterOriginChange) {
  FakeBindingDelegate delegate(kTestPortName);
  ScopedApiBinding binding(&manager_, &delegate, kTestBindingId,
                           kTestBindingScript);

  manager_.UpdateOrigin("https://b.test");
  TestMessagePortReceiver old_client_receiver;
  std::unique_ptr<MessagePort> old_client_port;
  std::unique_ptr<MessagePort> old_server_port;
  CreatePlatformMessagePortPair(&old_client_port, &old_server_port);
  old_client_port->SetReceiver(&old_client_receiver);
  ASSERT_TRUE(manager_.ConnectPort(kTestPortName, std::move(old_server_port)));

  manager_.UpdateOrigin("https://a.test");
  old_client_receiver.RunUntilDisconnected();
  EXPECT_EQ(delegate.disconnected_count(), 1);

  TestMessagePortReceiver new_client_receiver;
  std::unique_ptr<MessagePort> new_client_port;
  std::unique_ptr<MessagePort> new_server_port;
  CreatePlatformMessagePortPair(&new_client_port, &new_server_port);
  new_client_port->SetReceiver(&new_client_receiver);
  ASSERT_TRUE(manager_.ConnectPort(kTestPortName, std::move(new_server_port)));
  EXPECT_EQ(delegate.connected_count(), 2);

  ASSERT_TRUE(new_client_port->PostMessage("ping_from_a"));
  delegate.WaitForMessage();
  ASSERT_EQ(delegate.received_messages().size(), 1u);
  EXPECT_EQ(delegate.received_messages()[0], "ping_from_a");
}

TEST_F(ManagerTest, KeepsConnectedPortsWhenOriginIsUnchanged) {
  FakeBindingDelegate delegate(kTestPortName);
  ScopedApiBinding binding(&manager_, &delegate, kTestBindingId,
                           kTestBindingScript);

  manager_.UpdateOrigin("https://a.test");

  TestMessagePortReceiver client_receiver;
  std::unique_ptr<MessagePort> client_port;
  std::unique_ptr<MessagePort> server_port;
  CreatePlatformMessagePortPair(&client_port, &server_port);
  client_port->SetReceiver(&client_receiver);

  ASSERT_TRUE(manager_.ConnectPort(kTestPortName, std::move(server_port)));
  EXPECT_EQ(delegate.connected_count(), 1);

  manager_.UpdateOrigin("https://a.test");
  EXPECT_EQ(delegate.disconnected_count(), 0);

  ASSERT_TRUE(binding.SendMessage("still_connected"));
  ASSERT_TRUE(client_receiver.RunUntilMessageCountEqual(1));
  EXPECT_EQ(client_receiver.buffer()[0].first, "still_connected");
}

TEST_F(ManagerTest, DisconnectsTransferredChildSubPortsWhenOriginChanges) {
  MultiPortReceiver multi_receiver;
  std::unique_ptr<MessagePort> server_parent_holder;
  manager_.RegisterPortHandler(
      kTestPortName,
      base::BindRepeating(
          [](std::unique_ptr<MessagePort>* out, MultiPortReceiver* receiver,
             std::unique_ptr<MessagePort> port) {
            *out = std::move(port);
            (*out)->SetReceiver(receiver);
          },
          &server_parent_holder, &multi_receiver));

  manager_.UpdateOrigin("https://b.test");
  TestMessagePortReceiver client_parent_receiver;
  std::unique_ptr<MessagePort> client_parent;
  std::unique_ptr<MessagePort> server_parent;
  CreatePlatformMessagePortPair(&client_parent, &server_parent);
  client_parent->SetReceiver(&client_parent_receiver);
  ASSERT_TRUE(manager_.ConnectPort(kTestPortName, std::move(server_parent)));

  TestMessagePortReceiver client_child_receiver;
  std::unique_ptr<MessagePort> client_child;
  std::unique_ptr<MessagePort> server_child;
  CreatePlatformMessagePortPair(&client_child, &server_child);
  client_child->SetReceiver(&client_child_receiver);

  std::vector<std::unique_ptr<MessagePort>> transferred;
  transferred.push_back(std::move(server_child));
  ASSERT_TRUE(client_parent->PostMessageWithTransferables(
      "bind_child", std::move(transferred)));
  multi_receiver.WaitForChildPort();
  ASSERT_TRUE(multi_receiver.child_port());

  manager_.UpdateOrigin("https://a.test");
  EXPECT_EQ(multi_receiver.parent_pipe_errors(), 1);
  EXPECT_EQ(multi_receiver.child_pipe_errors(), 1);
  client_parent_receiver.RunUntilDisconnected();
  client_child_receiver.RunUntilDisconnected();
  EXPECT_FALSE(multi_receiver.child_port()->CanPostMessage());

  manager_.UnregisterPortHandler(kTestPortName);
}

TEST_F(ManagerTest, DisconnectsPortsWhenSetOriginEmptyFromInitialEmptyOrigin) {
  FakeBindingDelegate delegate(kTestPortName);
  ScopedApiBinding binding(&manager_, &delegate, kTestBindingId,
                           kTestBindingScript);

  EXPECT_EQ(manager_.origin(), "");
  TestMessagePortReceiver client_receiver;
  std::unique_ptr<MessagePort> client_port;
  std::unique_ptr<MessagePort> server_port;
  CreatePlatformMessagePortPair(&client_port, &server_port);
  client_port->SetReceiver(&client_receiver);

  ASSERT_TRUE(manager_.ConnectPort(kTestPortName, std::move(server_port)));
  EXPECT_EQ(delegate.connected_count(), 1);

  manager_.UpdateOrigin("");
  EXPECT_EQ(delegate.disconnected_count(), 1);
  client_receiver.RunUntilDisconnected();
}

TEST_F(ManagerTest, HandlesPreConnectSendMessageAndPeerPipeError) {
  FakeBindingDelegate delegate(kTestPortName);
  ScopedApiBinding binding(&manager_, &delegate, kTestBindingId,
                           kTestBindingScript);

  EXPECT_FALSE(binding.SendMessage("before_connect"));

  std::unique_ptr<MessagePort> client_port;
  std::unique_ptr<MessagePort> server_port;
  CreatePlatformMessagePortPair(&client_port, &server_port);
  ASSERT_TRUE(manager_.ConnectPort(kTestPortName, std::move(server_port)));

  base::RunLoop disconnect_loop;
  delegate.SetOnDisconnectedCallback(disconnect_loop.QuitClosure());
  client_port->Close();
  client_port.reset();
  disconnect_loop.Run();

  EXPECT_EQ(delegate.disconnected_count(), 1);
  EXPECT_FALSE(binding.SendMessage("after_peer_close"));
}

TEST_F(ManagerTest,
       HandlesReentrantScopedApiBindingDestructionDuringSetOrigin) {
  FakeBindingDelegate delegate(kTestPortName);
  auto binding = std::make_unique<ScopedApiBinding>(
      &manager_, &delegate, kTestBindingId, kTestBindingScript);

  manager_.UpdateOrigin("https://b.test");
  std::unique_ptr<MessagePort> client_port;
  std::unique_ptr<MessagePort> server_port;
  CreatePlatformMessagePortPair(&client_port, &server_port);
  ASSERT_TRUE(manager_.ConnectPort(kTestPortName, std::move(server_port)));

  delegate.SetOnDisconnectedCallback(base::BindOnce(
      [](std::unique_ptr<ScopedApiBinding>* b) { b->reset(); }, &binding));

  manager_.UpdateOrigin("https://a.test");
  EXPECT_FALSE(binding);
  EXPECT_EQ(delegate.disconnected_count(), 1);
}

TEST_F(ManagerTest, UnwrapsManagerProvidedSubPortWhenForwardedViaPostMessage) {
  TestMessagePortReceiver server_port_receiver;
  TestMessagePortReceiver forwarder_server_receiver;
  std::unique_ptr<MessagePort> receiver_server_port;
  std::unique_ptr<MessagePort> forwarder_server_port;
  manager_.RegisterPortHandler(
      kTestPortName, base::BindRepeating(
                         [](std::unique_ptr<MessagePort>* out,
                            TestMessagePortReceiver* receiver,
                            std::unique_ptr<MessagePort> port) {
                           *out = std::move(port);
                           (*out)->SetReceiver(receiver);
                         },
                         &receiver_server_port, &server_port_receiver));
  manager_.RegisterPortHandler(
      "cast.__platform__.forwarder",
      base::BindRepeating(
          [](std::unique_ptr<MessagePort>* out,
             TestMessagePortReceiver* receiver,
             std::unique_ptr<MessagePort> port) {
            *out = std::move(port);
            (*out)->SetReceiver(receiver);
          },
          &forwarder_server_port, &forwarder_server_receiver));

  manager_.UpdateOrigin("https://a.test");

  TestMessagePortReceiver sender_client_receiver;
  std::unique_ptr<MessagePort> sender_client;
  std::unique_ptr<MessagePort> sender_server;
  CreatePlatformMessagePortPair(&sender_client, &sender_server);
  sender_client->SetReceiver(&sender_client_receiver);
  ASSERT_TRUE(manager_.ConnectPort(kTestPortName, std::move(sender_server)));

  TestMessagePortReceiver target_client_receiver;
  std::unique_ptr<MessagePort> target_client;
  std::unique_ptr<MessagePort> target_server;
  CreatePlatformMessagePortPair(&target_client, &target_server);
  target_client->SetReceiver(&target_client_receiver);
  ASSERT_TRUE(manager_.ConnectPort("cast.__platform__.forwarder",
                                   std::move(target_server)));

  // Transfer a child sub-port into the Manager (which wraps it), then forward
  // that wrapped sub-port via PostMessageWithTransferables without calling
  // SetReceiver() on it.
  TestMessagePortReceiver sub_client_receiver;
  std::unique_ptr<MessagePort> sub_client;
  std::unique_ptr<MessagePort> sub_server;
  CreatePlatformMessagePortPair(&sub_client, &sub_server);
  sub_client->SetReceiver(&sub_client_receiver);

  std::vector<std::unique_ptr<MessagePort>> incoming;
  incoming.push_back(std::move(sub_server));
  ASSERT_TRUE(sender_client->PostMessageWithTransferables("incoming_sub",
                                                          std::move(incoming)));
  ASSERT_TRUE(server_port_receiver.RunUntilMessageCountEqual(1));
  ASSERT_EQ(server_port_receiver.buffer()[0].second.size(), 1u);

  std::vector<std::unique_ptr<MessagePort>> forwarded;
  forwarded.push_back(std::move(server_port_receiver.buffer()[0].second[0]));
  ASSERT_TRUE(forwarder_server_port->PostMessageWithTransferables(
      "forwarded_sub", std::move(forwarded)));
  ASSERT_TRUE(target_client_receiver.RunUntilMessageCountEqual(1));
  ASSERT_EQ(target_client_receiver.buffer()[0].second.size(), 1u);

  // Verify the forwarded sub-port works end-to-end as a real platform port.
  TestMessagePortReceiver received_sub_receiver;
  std::unique_ptr<MessagePort> received_sub =
      std::move(target_client_receiver.buffer()[0].second[0]);
  received_sub->SetReceiver(&received_sub_receiver);
  ASSERT_TRUE(received_sub->PostMessage("hello_over_forwarded_sub"));
  ASSERT_TRUE(sub_client_receiver.RunUntilMessageCountEqual(1));
  EXPECT_EQ(sub_client_receiver.buffer()[0].first, "hello_over_forwarded_sub");

  manager_.UnregisterPortHandler("cast.__platform__.forwarder");
  manager_.UnregisterPortHandler(kTestPortName);
}

TEST_F(ManagerTest, RejectsForwardingDisconnectedManagerPort) {
  TestMessagePortReceiver held_port_receiver;
  std::unique_ptr<MessagePort> held_port;
  manager_.RegisterPortHandler(
      kTestPortName,
      base::BindRepeating(
          [](std::unique_ptr<MessagePort>* out,
             std::unique_ptr<MessagePort> port) { *out = std::move(port); },
          &held_port));

  manager_.UpdateOrigin("https://b.test");
  std::unique_ptr<MessagePort> old_client;
  std::unique_ptr<MessagePort> old_server;
  CreatePlatformMessagePortPair(&old_client, &old_server);
  ASSERT_TRUE(manager_.ConnectPort(kTestPortName, std::move(old_server)));
  std::unique_ptr<MessagePort> stale_wrapper =
      std::exchange(held_port, nullptr);

  // Disconnecting stale_wrapper leaves it wrapped with a null inner port.
  manager_.UpdateOrigin("https://a.test");

  TestMessagePortReceiver new_client_receiver;
  std::unique_ptr<MessagePort> new_client;
  std::unique_ptr<MessagePort> new_server;
  CreatePlatformMessagePortPair(&new_client, &new_server);
  new_client->SetReceiver(&new_client_receiver);
  ASSERT_TRUE(manager_.ConnectPort(kTestPortName, std::move(new_server)));
  ASSERT_TRUE(held_port);
  held_port->SetReceiver(&held_port_receiver);
  ASSERT_TRUE(held_port->CanPostMessage());

  std::vector<std::unique_ptr<MessagePort>> ports;
  ports.push_back(std::move(stale_wrapper));
  EXPECT_FALSE(
      held_port->PostMessageWithTransferables("stale_port", std::move(ports)));

  // Verify the healthy port can still post normal messages afterwards.
  EXPECT_TRUE(held_port->PostMessage("still_working"));
  ASSERT_TRUE(new_client_receiver.RunUntilMessageCountEqual(1));
  EXPECT_EQ(new_client_receiver.buffer()[0].first, "still_working");

  manager_.UnregisterPortHandler(kTestPortName);
}

}  // namespace
}  // namespace cast_api_bindings
