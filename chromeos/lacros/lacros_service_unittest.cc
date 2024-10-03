// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/test/task_environment.h"
#include "base/token.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_init_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using LacrosServiceTest = testing::Test;

// Verifies that `mojo::Remote` bound through `LacrosService` has the same
// version number as the associated interface.
TEST_F(LacrosServiceTest, CheckCrosapiRemoteVersion) {
  // Set the version of `crosapi::mojom::DownloadStatusUpdater` to be one.
  crosapi::mojom::BrowserInitParamsPtr init_params =
      ::crosapi::mojom::BrowserInitParams::New();
  init_params->interface_versions = base::flat_map<base::Token, unsigned int>(
      std::vector<std::pair<base::Token, unsigned int>>(
          {{crosapi::mojom::DownloadStatusUpdater::Uuid_, 1}}));
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));

  // Bind a download status updater remote and then check the version of this
  // remote to be one.
  base::test::TaskEnvironment environment;
  mojo::Remote<crosapi::mojom::DownloadStatusUpdater>
      download_status_updater_remote;
  bool success = LacrosService()
                     .MaybeInitializeAndBindRemote<
                         crosapi::mojom::DownloadStatusUpdater,
                         &crosapi::mojom::Crosapi::BindDownloadStatusUpdater>(
                         &download_status_updater_remote);
  EXPECT_TRUE(success);
  EXPECT_EQ(download_status_updater_remote.version(), 1u);
}

}  // namespace chromeos
