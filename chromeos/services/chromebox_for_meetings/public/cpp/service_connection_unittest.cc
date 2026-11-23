// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::StrictMock;
using ::testing::WithArgs;

namespace chromeos {
namespace cfm {
namespace {

class CfmServiceConnectionTest : public testing::Test {
 public:
  CfmServiceConnectionTest() = default;
  CfmServiceConnectionTest(const CfmServiceConnectionTest&) = delete;
  CfmServiceConnectionTest& operator=(const CfmServiceConnectionTest&) = delete;

  void SetUp() override {
    ServiceConnection::UseFakeServiceConnectionForTesting(
        &fake_service_connection_);
  }

  void SetBootstrapCallback(
      FakeServiceConnectionImpl::FakeBootstrapCallback callback) {
    fake_service_connection_.SetCallback(std::move(callback));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  FakeServiceConnectionImpl fake_service_connection_;
};

TEST_F(CfmServiceConnectionTest, FakeBindServiceContext) {
  base::RunLoop run_loop;

  bool test_success = false;
  SetBootstrapCallback(base::BindLambdaForTesting(
      [&](mojo::PendingReceiver<mojom::CfmServiceContext>, bool success) {
        test_success = success;
        run_loop.QuitClosure().Run();
      }));

  mojo::Remote<::chromeos::cfm::mojom::CfmServiceContext> remote;
  ServiceConnection::GetInstance()->BindServiceContext(
      remote.BindNewPipeAndPassReceiver());

  run_loop.Run();

  ASSERT_TRUE(test_success);
  ASSERT_TRUE(remote.is_bound());
}

// Connections that do not accept restricted contexts must reset the receiver
// so that callers observe the service as unavailable.
TEST_F(CfmServiceConnectionTest, RestrictedServiceContextUnavailableByDefault) {
  base::RunLoop run_loop;

  std::optional<uint32_t> disconnect_reason;
  mojo::Remote<::chromeos::cfm::mojom::CfmServiceContext> remote;
  ServiceConnection::GetInstance()->BindRestrictedServiceContext(
      remote.BindNewPipeAndPassReceiver());
  remote.set_disconnect_with_reason_handler(base::BindLambdaForTesting(
      [&](uint32_t custom_reason, const std::string& description) {
        disconnect_reason = custom_reason;
        run_loop.Quit();
      }));

  run_loop.Run();

  ASSERT_TRUE(disconnect_reason.has_value());
  EXPECT_EQ(
      static_cast<uint32_t>(mojom::DisconnectReason::kServiceUnavailableCode),
      disconnect_reason.value());
  EXPECT_FALSE(remote.is_connected());
}

}  // namespace
}  // namespace cfm
}  // namespace chromeos
