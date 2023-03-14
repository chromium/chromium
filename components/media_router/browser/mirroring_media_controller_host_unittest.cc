// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/mirroring_media_controller_host.h"

#include "base/test/task_environment.h"
#include "components/media_router/common/media_route.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

TEST(MirroringMediaControllerHostTest, GetMediaStatusObserverPendingRemote) {
  base::test::SingleThreadTaskEnvironment task_environment;
  mojo::Remote<mojom::MediaController> controller_remote;
  MirroringMediaControllerHost host(std::move(controller_remote));
  auto observer_remote = host.GetMediaStatusObserverPendingRemote();
  EXPECT_TRUE(observer_remote.is_valid());
}

}  // namespace media_router
