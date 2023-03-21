// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/server/wayland_server_controller.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "components/exo/data_exchange_delegate.h"
#include "components/exo/input_method_surface_manager.h"
#include "components/exo/notification_surface_manager.h"
#include "components/exo/security_delegate.h"
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

TEST_F(WaylandServerControllerTest, RequestServer) {
  WaylandServerController wsc(nullptr, nullptr, nullptr, nullptr);
  ASSERT_EQ(WaylandServerController::Get(), &wsc);

  base::RunLoop loop;
  base::FilePath socket_path;
  {
    base::ScopedDisallowBlocking no_blocking;
    WaylandServerController::Get()->CreateServer(
        std::make_unique<test::TestSecurityDelegate>(),
        base::BindLambdaForTesting(
            [&loop, &socket_path](bool success,
                                  const base::FilePath& new_path) {
              socket_path = std::move(new_path);
              loop.Quit();
            }));
  }
  loop.Run();
  EXPECT_FALSE(socket_path.empty());
  EXPECT_TRUE(base::PathExists(socket_path));

  {
    base::ScopedDisallowBlocking no_blocking;
    WaylandServerController::Get()->DeleteServer(socket_path);
  }
  EXPECT_FALSE(base::PathExists(socket_path));
}

TEST_F(WaylandServerControllerTest, RequestServerByFd) {
  WaylandServerController wsc(nullptr, nullptr, nullptr, nullptr);
  ASSERT_EQ(WaylandServerController::Get(), &wsc);

  wayland::test::WaylandServerTestBase::ScopedTempSocket sock;

  base::RunLoop loop;
  WaylandServerController::ServerToken token = -1;
  {
    base::ScopedDisallowBlocking no_blocking;
    WaylandServerController::Get()->ListenOnSocket(
        std::make_unique<test::TestSecurityDelegate>(), sock.TakeFd(),
        base::BindLambdaForTesting(
            [&loop, &token](bool success,
                            WaylandServerController::ServerToken result_token) {
              EXPECT_TRUE(success);
              token = result_token;
              loop.Quit();
            }));
  }
  loop.Run();
  EXPECT_GE(token, 0);

  {
    base::ScopedDisallowBlocking no_blocking;
    // Just ensure that closing a socket is nonblocking.
    WaylandServerController::Get()->CloseSocket(token);
  }
  task_environment()->RunUntilIdle();
}

}  // namespace exo
