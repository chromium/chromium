// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/mirroring_media_controller_host.h"

#include "base/test/task_environment.h"
#include "components/media_router/common/media_route.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class MockMirroringMediaControllerHostObserver
    : public MirroringMediaControllerHost::Observer {
 public:
  MockMirroringMediaControllerHostObserver() = default;
  explicit MockMirroringMediaControllerHostObserver(
      MirroringMediaControllerHost* host)
      : host_(host) {
    host_->AddObserver(this);
  }

  ~MockMirroringMediaControllerHostObserver() override {
    if (host_) {
      host_->RemoveObserver(this);
    }
  }

  MOCK_METHOD(void, OnFreezeInfoChanged, (), (override));

 private:
  raw_ptr<MirroringMediaControllerHost> host_ = nullptr;
};

class MirroringMediaControllerHostTest : public ::testing::Test {
 public:
  MirroringMediaControllerHostTest() = default;
  ~MirroringMediaControllerHostTest() override = default;

  void SetUp() override {
    host_ = std::make_unique<MirroringMediaControllerHost>(
        std::move(controller_remote_));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  mojo::Remote<mojom::MediaController> controller_remote_;
  std::unique_ptr<MirroringMediaControllerHost> host_;
};

TEST_F(MirroringMediaControllerHostTest, GetMediaStatusObserverPendingRemote) {
  auto observer_remote = host_->GetMediaStatusObserverPendingRemote();
  EXPECT_TRUE(observer_remote.is_valid());
}

TEST_F(MirroringMediaControllerHostTest, OnMediaStatusUpdated) {
  // Constructing will add itself as an observer of |host|.
  MockMirroringMediaControllerHostObserver observer(host_.get());

  EXPECT_CALL(observer, OnFreezeInfoChanged);
  host_->OnMediaStatusUpdated({});
}

}  // namespace media_router
