// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/cast_socket_service.h"

#include "base/memory/ptr_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_simple_task_runner.h"
#include "components/media_router/common/providers/cast/channel/cast_test_util.h"
#include "content/public/test/browser_task_environment.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArgs;

namespace cast_channel {

class CastSocketServiceTest : public testing::Test {
 public:
  CastSocketServiceTest() : cast_socket_service_(new CastSocketServiceImpl()) {
    cast_socket_service_->SetTaskRunnerForTest(
        base::MakeRefCounted<base::TestSimpleTaskRunner>());
  }

  CastSocket* AddSocket(std::unique_ptr<CastSocket> socket) {
    return cast_socket_service_->AddSocket(std::move(socket));
  }

  void TearDown() override { cast_socket_service_ = nullptr; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<CastSocketServiceImpl> cast_socket_service_;
  base::MockCallback<CastSocket::OnOpenCallback> mock_on_open_callback_;
  MockCastSocketObserver mock_observer_;
};

TEST_F(CastSocketServiceTest, TestAddSocket) {
  auto socket1 = std::make_unique<MockCastSocket>();
  auto* socket_ptr1 = AddSocket(std::move(socket1));
  EXPECT_NE(0, socket_ptr1->id());

  auto socket2 = std::make_unique<MockCastSocket>();
  auto* socket_ptr2 = AddSocket(std::move(socket2));
  EXPECT_NE(socket_ptr1->id(), socket_ptr2->id());

  auto removed_socket = cast_socket_service_->RemoveSocket(socket_ptr2->id());
  EXPECT_EQ(socket_ptr2, removed_socket.get());

  auto socket3 = std::make_unique<MockCastSocket>();
  auto* socket_ptr3 = AddSocket(std::move(socket3));
  EXPECT_NE(socket_ptr1->id(), socket_ptr3->id());
  EXPECT_NE(socket_ptr2->id(), socket_ptr3->id());
}

TEST_F(CastSocketServiceTest, TestRemoveAndGetSocket) {
  int channel_id = 1;
  auto* socket_ptr = cast_socket_service_->GetSocket(channel_id);
  EXPECT_FALSE(socket_ptr);
  auto socket = cast_socket_service_->RemoveSocket(channel_id);
  EXPECT_FALSE(socket);

  auto mock_socket = std::make_unique<MockCastSocket>();

  auto* mock_socket_ptr = AddSocket(std::move(mock_socket));
  channel_id = mock_socket_ptr->id();
  EXPECT_EQ(mock_socket_ptr, cast_socket_service_->GetSocket(channel_id));

  socket = cast_socket_service_->RemoveSocket(channel_id);
  EXPECT_TRUE(socket);
}

TEST_F(CastSocketServiceTest, TestOpenChannel) {
  auto* mock_socket = new MockCastSocket();
  auto ip_endpoint = CreateIPEndPointForTest();
  mock_socket->SetIPEndpoint(ip_endpoint);
  cast_socket_service_->SetSocketForTest(base::WrapUnique(mock_socket));

  EXPECT_CALL(*mock_socket, Connect_(_))
      .WillOnce(base::test::RunOnceCallback<0>(mock_socket));
  EXPECT_CALL(mock_on_open_callback_, Run(mock_socket));
  EXPECT_CALL(*mock_socket, AddObserver(_));

  cast_socket_service_->AddObserver(&mock_observer_);
  CastSocketOpenParams open_param(ip_endpoint, base::Seconds(20));
  cast_socket_service_->OpenSocket(network::NetworkContextGetter(), open_param,
                                   mock_on_open_callback_.Get());
}

}  // namespace cast_channel
