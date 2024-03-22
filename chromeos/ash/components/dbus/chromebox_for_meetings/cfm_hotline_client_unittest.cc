// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_hotline_client.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/cfm/dbus-constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace ash {

class FakeCfmObserver : public cfm::CfmObserver {
 public:
  FakeCfmObserver() { CfmHotlineClient::Get()->AddObserver(this); }
  ~FakeCfmObserver() override { CfmHotlineClient::Get()->RemoveObserver(this); }

  bool ServiceRequestReceived(const std::string& service_id) override {
    request_service_id_ = std::move(service_id);
    return true;
  }

  std::string request_service_id_;
};

class CfmHotlineClientTest : public testing::Test {
 public:
  CfmHotlineClientTest() = default;
  ~CfmHotlineClientTest() override = default;

  void SetUp() override {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;

    // Filter uninteresting calls to the bus
    mock_bus_ = base::MakeRefCounted<::testing::NiceMock<dbus::MockBus>>(
        dbus::Bus::Options());

    mock_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), ::cfm::broker::kServiceName,
        dbus::ObjectPath(::cfm::broker::kServicePath));

    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(::cfm::broker::kServiceName,
                               dbus::ObjectPath(::cfm::broker::kServicePath)))
        .WillOnce(Return(mock_proxy_.get()));

    EXPECT_CALL(
        *mock_proxy_.get(),
        DoConnectToSignal(::cfm::broker::kServiceInterfaceName, _, _, _))
        .WillRepeatedly(Invoke(this, &CfmHotlineClientTest::ConnectToSignal));

    CfmHotlineClient::Initialize(mock_bus_.get());

    // The easiest source of fds is opening /dev/null.
    test_file_ = base::File(base::FilePath("/dev/null"),
                            base::File::FLAG_OPEN | base::File::FLAG_WRITE);
    ASSERT_TRUE(test_file_.IsValid());

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { CfmHotlineClient::Shutdown(); }

  void CallMethod(dbus::MethodCall* method_call,
                  int timeout_ms,
                  dbus::ObjectProxy::ResponseCallback* callback) {
    dbus::Response* response = nullptr;

    if (!responses_.empty()) {
      used_responses_.push_back(std::move(responses_.front()));
      responses_.pop_front();
      response = used_responses_.back().get();
    }

    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*callback), response));
  }

  void EmitServiceRequestedSignal(const std::string& interface_name) {
    dbus::Signal signal(::cfm::broker::kServiceInterfaceName,
                        ::cfm::broker::kMojoServiceRequestedSignal);
    dbus::MessageWriter writer(&signal);
    writer.AppendString(interface_name);

    // simulate signal
    const std::string signal_name = signal.GetMember();
    const auto it = signal_callbacks_.find(signal_name);
    ASSERT_TRUE(it != signal_callbacks_.end())
        << "Client didn't register for signal " << signal_name;
    it->second.Run(&signal);
  }

 protected:
  // Handles calls to |proxy_|'s ConnectToSignal() method.
  void ConnectToSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      dbus::ObjectProxy::SignalCallback signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    EXPECT_EQ(interface_name, ::cfm::broker::kServiceInterfaceName);
    signal_callbacks_[signal_name] = signal_callback;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(*on_connected_callback), interface_name,
                       signal_name, true /* success */));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
  std::deque<std::unique_ptr<dbus::Response>> responses_;
  base::File test_file_;

 private:
  std::deque<std::unique_ptr<dbus::Response>> used_responses_;
  // Maps from biod signal name to the corresponding callback provided by the
  // CfmHotlineClient.
  std::map<std::string, dbus::ObjectProxy::SignalCallback> signal_callbacks_;
};

TEST_F(CfmHotlineClientTest, BootstrapMojoSuccessTest) {
  responses_.push_back(dbus::Response::CreateEmpty());

  EXPECT_CALL(*mock_proxy_.get(), DoCallMethod(_, _, _))
      .WillOnce(Invoke(this, &CfmHotlineClientTest::CallMethod));

  base::MockCallback<CfmHotlineClient::BootstrapMojoConnectionCallback>
      callback;
  EXPECT_CALL(callback, Run(true)).Times(1);

  CfmHotlineClient::Get()->BootstrapMojoConnection(
      base::ScopedFD(test_file_.TakePlatformFile()), callback.Get());

  base::RunLoop().RunUntilIdle();
}

TEST_F(CfmHotlineClientTest, BootstrapMojoFailureTest) {
  EXPECT_CALL(*mock_proxy_.get(), DoCallMethod(_, _, _))
      .WillOnce(Invoke(this, &CfmHotlineClientTest::CallMethod));

  base::MockCallback<CfmHotlineClient::BootstrapMojoConnectionCallback>
      callback;
  EXPECT_CALL(callback, Run(false)).Times(1);

  // Fail with no normal or error response
  CfmHotlineClient::Get()->BootstrapMojoConnection(
      base::ScopedFD(test_file_.TakePlatformFile()), callback.Get());

  base::RunLoop().RunUntilIdle();
}

TEST_F(CfmHotlineClientTest, EmitMojoServiceRequestedSignal) {
  base::RunLoop run_loop;
  const std::string interface_name = "Foo";

  FakeCfmObserver observer;
  EmitServiceRequestedSignal(interface_name);

  run_loop.RunUntilIdle();
  EXPECT_EQ(observer.request_service_id_, interface_name);
}

}  // namespace ash
