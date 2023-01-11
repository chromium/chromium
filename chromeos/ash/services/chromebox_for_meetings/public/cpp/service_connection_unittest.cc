// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/chromebox_for_meetings/public/cpp/service_connection.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_hotline_client.h"
#include "chromeos/ash/services/chromebox_for_meetings/public/cpp/fake_service_connection.h"
#include "chromeos/ash/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::StrictMock;
using ::testing::WithArgs;

namespace ash {
namespace cfm {
namespace {

// TODO(https://crbug.com/1403174): Remove when namespace of mojoms for CfM are
// migarted to ash.
namespace mojom = ::chromeos::cfm::mojom;

class CfmServiceConnectionTest : public testing::Test {
 public:
  CfmServiceConnectionTest() = default;
  CfmServiceConnectionTest(const CfmServiceConnectionTest&) = delete;
  CfmServiceConnectionTest& operator=(const CfmServiceConnectionTest&) = delete;

  void SetUp() override {
    CfmHotlineClient::InitializeFake();
    ServiceConnection::UseFakeServiceConnectionForTesting(
        &fake_service_connection_);
  }

  void TearDown() override { CfmHotlineClient::Shutdown(); }

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

}  // namespace
}  // namespace cfm
}  // namespace ash
