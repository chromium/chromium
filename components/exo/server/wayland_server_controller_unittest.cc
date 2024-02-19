// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/server/wayland_server_controller.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "components/exo/data_exchange_delegate.h"
#include "components/exo/input_method_surface_manager.h"
#include "components/exo/notification_surface_manager.h"
#include "components/exo/security_delegate.h"
#include "components/exo/server/wayland_server_handle.h"
#include "components/exo/test/test_security_delegate.h"
#include "components/exo/toast_surface_manager.h"
#include "components/exo/wayland/test/wayland_server_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace {

class WaylandServerControllerTest : public ash::AshTestBase {
 public:
  void SetUp() override {
    ASSERT_TRUE(root_dir.CreateUniqueTempDir());
    xdg_dir_ = root_dir.GetPath().Append("xdg");
    setenv("XDG_RUNTIME_DIR", xdg_dir_.MaybeAsASCII().c_str(),
           /*overwrite=*/1);
    ash::AshTestBase::SetUp();
  }

 protected:
  base::FilePath xdg_dir_;

 private:
  base::ScopedTempDir root_dir;
};

}  // namespace

TEST_F(WaylandServerControllerTest, RequestServerByFd) {
  WaylandServerController wsc(nullptr, nullptr, nullptr, nullptr, nullptr);
  ASSERT_EQ(WaylandServerController::Get(), &wsc);

  wayland::test::WaylandServerTestBase::ScopedTempSocket sock;

  base::RunLoop loop;
  std::unique_ptr<WaylandServerHandle> handle;
  {
    base::ScopedDisallowBlocking no_blocking;
    WaylandServerController::Get()->ListenOnSocket(
        std::make_unique<test::TestSecurityDelegate>(), sock.TakeFd(),
        base::BindLambdaForTesting(
            [&loop,
             &handle](std::unique_ptr<WaylandServerHandle> result_handle) {
              EXPECT_TRUE(result_handle);
              handle = std::move(result_handle);
              loop.Quit();
            }));
  }
  loop.Run();

  {
    base::ScopedDisallowBlocking no_blocking;
    // Just ensure that closing a socket is nonblocking.
    handle.reset();
  }
  task_environment()->RunUntilIdle();
}

TEST_F(WaylandServerControllerTest, RequestServerBadSocket) {
  WaylandServerController wsc(nullptr, nullptr, nullptr, nullptr, nullptr);
  ASSERT_EQ(WaylandServerController::Get(), &wsc);

  base::RunLoop loop;
  {
    base::ScopedDisallowBlocking no_blocking;
    WaylandServerController::Get()->ListenOnSocket(
        std::make_unique<test::TestSecurityDelegate>(), base::ScopedFD{},
        base::BindLambdaForTesting(
            [&loop](std::unique_ptr<WaylandServerHandle> result_handle) {
              EXPECT_FALSE(result_handle);
              loop.Quit();
            }));
  }
  loop.Run();
}

}  // namespace exo
