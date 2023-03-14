// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/libassistant_loader_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::libassistant {

class LibassistantLoaderImplTest : public ::testing::Test {
 public:
  LibassistantLoaderImplTest() = default;
  LibassistantLoaderImplTest(const LibassistantLoaderImplTest&) = delete;
  LibassistantLoaderImplTest& operator=(const LibassistantLoaderImplTest&) =
      delete;
  ~LibassistantLoaderImplTest() override = default;

 private:
  base::test::TaskEnvironment environment_;
};

TEST_F(LibassistantLoaderImplTest, ShouldCreateInstance) {
  auto* loader = LibassistantLoaderImpl::GetInstance();
  EXPECT_TRUE(loader);
}

TEST_F(LibassistantLoaderImplTest, ShouldRunCallbackWithDlcFeature) {
  auto* loader = LibassistantLoaderImpl::GetInstance();
  EXPECT_TRUE(loader);

  // Should fail without dlcservice client.
  base::RunLoop run_loop1;
  loader->Load(base::BindOnce(
      [](base::RunLoop* run_loop, bool success) {
        EXPECT_FALSE(success);
        run_loop->Quit();
      },
      &run_loop1));
  run_loop1.Run();

  // Should success with dlcservice client.
  base::RunLoop run_loop2;
  DlcserviceClient::InitializeFake();
  loader->Load(base::BindOnce(
      [](base::RunLoop* run_loop, bool success) {
        EXPECT_TRUE(success);
        run_loop->Quit();
      },
      &run_loop2));
  run_loop2.Run();
  DlcserviceClient::Shutdown();
}

}  // namespace ash::libassistant
