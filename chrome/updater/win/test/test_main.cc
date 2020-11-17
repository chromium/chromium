// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/updater/win/task_scheduler.h"
#include "chrome/updater/win/util.h"

int main(int argc, char** argv) {
  auto scoped_com_initializer =
      std::make_unique<base::win::ScopedCOMInitializer>(
          base::win::ScopedCOMInitializer::kMTA);
  bool success = updater::InitializeCOMSecurity();
  CHECK(success) << "InitializeCOMSecurity() failed.";

  // Some tests will fail if two tests try to launch test_process.exe
  // simultaneously, so run the tests serially. This will still shard them and
  // distribute the shards to different swarming bots, but tests will run
  // serially on each bot.
  base::TestSuite test_suite(argc, argv);
  const int result = base::LaunchUnitTestsWithOptions(
      argc, argv,
      /*parallel_jobs=*/1U,        // Like LaunchUnitTestsSerially
      /*default_batch_limit=*/10,  // Like LaunchUnitTestsSerially
      false,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));

  return result;
}
