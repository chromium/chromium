// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/chromebox_for_meetings/public/cpp//service_adaptor.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_context.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::cfm {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArgs;

class FakeDelegate : public ServiceAdaptor::Delegate {
 public:
  void OnAdaptorConnect(bool success) override {
    connect_count++;
    connect_callback_.Run();
  }

  void OnAdaptorDisconnect() override {
    disconnect_count++;
    disconnect_callback_.Run();
  }

  void OnBindService(mojo::ScopedMessagePipeHandle) override {
    bind_request_count++;
    bind_service_callback_.Run();
  }

  int connect_count = 0;
  int disconnect_count = 0;
  int bind_request_count = 0;
  base::RepeatingClosure connect_callback_;
  base::RepeatingClosure disconnect_callback_;
  base::RepeatingClosure bind_service_callback_;
};

class CfmServiceAdaptorTest : public testing::Test {
 public:
  CfmServiceAdaptorTest() = default;
  CfmServiceAdaptorTest(const CfmServiceAdaptorTest&) = delete;
  CfmServiceAdaptorTest& operator=(const CfmServiceAdaptorTest&) = delete;

  void SetUp() override {
    chromeos::cfm::ServiceConnection::UseFakeServiceConnectionForTesting(
        &fake_service_connection);
  }

  void SetCallback(
      chromeos::cfm::FakeServiceConnectionImpl::FakeBootstrapCallback
          callback) {
    fake_service_connection.SetCallback(std::move(callback));
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  chromeos::cfm::FakeServiceConnectionImpl fake_service_connection;
};

TEST_F(CfmServiceAdaptorTest, BindServiceAdaptor) {
  base::RunLoop run_loop;

  // Intercept Connection to |CfmServiceContext|
  chromeos::cfm::FakeCfmServiceContext fake_context;
  mojo::Receiver<chromeos::cfm::mojom::CfmServiceContext> context_receiver(
      &fake_context);

  SetCallback(base::BindLambdaForTesting(
      [&context_receiver](
          mojo::PendingReceiver<chromeos::cfm::mojom::CfmServiceContext>
              pending_receiver,
          bool success) {
        // Bind the Context Receiver to the mock
        context_receiver.Bind(std::move(pending_receiver));
      }));

  // Fake out the next call to provide adaptor
  mojo::Remote<chromeos::cfm::mojom::CfmServiceAdaptor> remote;
  fake_context.SetFakeProvideAdaptorCallback(base::BindLambdaForTesting(
      [&](const std::string& interface_name,
          mojo::PendingRemote<chromeos::cfm::mojom::CfmServiceAdaptor>
              pending_adaptor,
          chromeos::cfm::FakeCfmServiceContext::ProvideAdaptorCallback
              callback) {
        // Bind the |chromeos::cfm::mojom::CfmServiceAdaptor| to a Remote
        remote.Bind(std::move(pending_adaptor));
        std::move(callback).Run(true);
      }));

  FakeDelegate fake_delegate;
  fake_delegate.connect_callback_ = base::BindLambdaForTesting([&] {
    // Force call to disconnect handler
    remote.reset();
  });
  fake_delegate.disconnect_callback_ = run_loop.QuitClosure();

  // Initiate Test
  ServiceAdaptor adaptor("foo", &fake_delegate);
  adaptor.BindServiceAdaptor();

  run_loop.Run();
  EXPECT_EQ(fake_delegate.connect_count, 1);
  EXPECT_EQ(fake_delegate.disconnect_count, 1);
}

}  // namespace
}  // namespace chromeos::cfm
