// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/server/wayland_server_controller.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "components/exo/capabilities.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace {

class TestCapabilities : public Capabilities {
 public:
  std::string GetSecurityContext() const override { return "test"; }
};

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

TEST_F(WaylandServerControllerTest, DefaultPath) {
  std::unique_ptr<Capabilities> capabilities =
      Capabilities::GetDefaultCapabilities();
  // You are not allowed to create a custom server with the default security
  // context.
  EXPECT_EQ(WaylandServerController::PathHelper::Create(*capabilities),
            nullptr);
}

TEST_F(WaylandServerControllerTest, CustomPath) {
  TestCapabilities capabilities;
  std::unique_ptr<WaylandServerController::PathHelper> helper =
      WaylandServerController::PathHelper::Create(capabilities);

  base::FilePath server_dir = helper->GetPath().DirName();

  // Should create a directory for the server.
  EXPECT_TRUE(base::DirectoryExists(server_dir));

  // Must not be a child of the XDG dir.
  EXPECT_TRUE(base::IsDirectoryEmpty(xdg_dir_));

  // Must be distinct from other servers in the same context.
  std::unique_ptr<WaylandServerController::PathHelper> helper2 =
      WaylandServerController::PathHelper::Create(capabilities);
  EXPECT_NE(helper->GetPath(), helper2->GetPath());

  // Must be deleted when the helper is removed.
  helper.reset();
  EXPECT_FALSE(base::DirectoryExists(server_dir));
}

}  // namespace exo
