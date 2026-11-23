// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/chromebox_for_meetings/features.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_hotline_client.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/fake_cfm_hotline_client.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "dbus/object_proxy.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::cfm {
namespace {

namespace mojom = ::chromeos::cfm::mojom;

class FakeCfmServiceAdaptor : public mojom::CfmServiceAdaptor {
 public:
  void OnBindService(mojo::ScopedMessagePipeHandle receiver_pipe) override {}
};

// A |CfmHotlineClient| that never reports the hotline daemon as available so
// that requests forwarded to the daemon stay queued for the duration of the
// test.
class StalledCfmHotlineClient : public FakeCfmHotlineClient {
 public:
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    pending_callbacks_.push_back(std::move(callback));
  }

 private:
  std::vector<dbus::ObjectProxy::WaitForServiceToBeAvailableCallback>
      pending_callbacks_;
};

class ServiceConnectionCfmAshTest : public testing::Test {
 public:
  ServiceConnectionCfmAshTest() = default;
  ServiceConnectionCfmAshTest(const ServiceConnectionCfmAshTest&) = delete;
  ServiceConnectionCfmAshTest& operator=(const ServiceConnectionCfmAshTest&) =
      delete;

  void SetUp() override {
    // Registers itself as the global instance; deleted by
    // |CfmHotlineClient::Shutdown()|.
    new StalledCfmHotlineClient();
    ::chromeos::cfm::ServiceConnection::UseFakeServiceConnectionForTesting(
        nullptr);
  }

  void TearDown() override { CfmHotlineClient::Shutdown(); }

 protected:
  base::test::ScopedFeatureList feature_list_{features::kMojoServices};
  base::test::TaskEnvironment task_environment_;
};

// Adaptor registration is only forwarded to the daemon for contexts bound via
// |BindServiceContext|; contexts bound via |BindRestrictedServiceContext| may
// still request services, while calls to |ProvideAdaptor| are reported as bad
// Mojo messages.
TEST_F(ServiceConnectionCfmAshTest, ProvideAdaptorRequiresFullServiceContext) {
  mojo::test::BadMessageObserver bad_message_observer;
  auto* service_connection = ::chromeos::cfm::ServiceConnection::GetInstance();

  mojo::Remote<mojom::CfmServiceContext> restricted_context;
  service_connection->BindRestrictedServiceContext(
      restricted_context.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::CfmServiceContext> full_context;
  service_connection->BindServiceContext(
      full_context.BindNewPipeAndPassReceiver());

  // Service bind requests on a restricted context are still forwarded to the
  // daemon, so the response stays pending while the daemon is unavailable.
  mojo::MessagePipe pipe;
  auto bind_result = std::make_shared<std::optional<bool>>();
  restricted_context->RequestBindService(
      "foo.mojom.Foo", std::move(pipe.handle0),
      base::BindLambdaForTesting(
          [bind_result](bool success) { *bind_result = success; }));

  // Adaptor registration on a full context is forwarded to the daemon, so the
  // response stays pending while the daemon is unavailable.
  FakeCfmServiceAdaptor full_adaptor;
  mojo::Receiver<mojom::CfmServiceAdaptor> full_adaptor_receiver(&full_adaptor);
  auto full_result = std::make_shared<std::optional<bool>>();
  full_context->ProvideAdaptor(
      "foo.mojom.Foo", full_adaptor_receiver.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting(
          [full_result](bool success) { *full_result = success; }));

  // Flush both remotes to verify legitimate requests are dispatched without
  // reporting a bad message or completing locally.
  restricted_context.FlushForTesting();
  full_context.FlushForTesting();

  EXPECT_FALSE(bad_message_observer.got_bad_message());
  EXPECT_FALSE(bind_result->has_value());
  EXPECT_FALSE(full_result->has_value());
  EXPECT_TRUE(restricted_context.is_connected());
  EXPECT_TRUE(full_context.is_connected());

  // Adaptor registration on a restricted context reports a bad Mojo message
  // and disconnects that receiver instead of forwarding to the daemon.
  base::RunLoop disconnect_loop;
  restricted_context.set_disconnect_handler(disconnect_loop.QuitClosure());

  FakeCfmServiceAdaptor restricted_adaptor;
  mojo::Receiver<mojom::CfmServiceAdaptor> restricted_adaptor_receiver(
      &restricted_adaptor);
  auto restricted_result = std::make_shared<std::optional<bool>>();
  restricted_context->ProvideAdaptor(
      "foo.mojom.Foo", restricted_adaptor_receiver.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting(
          [restricted_result](bool success) { *restricted_result = success; }));

  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Restricted CfmServiceContext cannot register service adaptors.");
  disconnect_loop.Run();

  EXPECT_FALSE(restricted_result->has_value());
  EXPECT_FALSE(restricted_context.is_connected());

  // The rejected receiver leaves other connected contexts unaffected.
  full_context.FlushForTesting();
  EXPECT_TRUE(full_context.is_connected());
  EXPECT_FALSE(full_result->has_value());
}

}  // namespace
}  // namespace ash::cfm
