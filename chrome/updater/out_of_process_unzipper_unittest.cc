// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/out_of_process_unzipper.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/updater/ipc/ipc_support.h"
#include "chrome/updater/test/test_scope.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

class OutOfProcessUnzipperTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  ScopedIPCSupportWrapper ipc_support_;
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(OutOfProcessUnzipperTest, DecodeXz_InputFileNotFound) {
  OutOfProcessUnzipper unzipper(GetUpdaterScopeForTesting());
  base::RunLoop run_loop;
  unzipper.DecodeXz(temp_dir_.GetPath().AppendASCII("non_existent_file.xz"),
                    temp_dir_.GetPath().AppendASCII("output.dat"),
                    base::BindOnce([](bool result) {
                      EXPECT_FALSE(result);
                    }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(OutOfProcessUnzipperTest, DecodeXz_OutputCannotBeCreated) {
  OutOfProcessUnzipper unzipper(GetUpdaterScopeForTesting());

  // Create an input file.
  const base::FilePath input_path = temp_dir_.GetPath().AppendASCII("input.xz");
  ASSERT_TRUE(base::WriteFile(input_path, "test data"));

  // Create a directory where the output file should be, to cause a failure.
  const base::FilePath output_path =
      temp_dir_.GetPath().AppendASCII("output.dat");
  ASSERT_TRUE(base::CreateDirectory(output_path));

  base::RunLoop run_loop;
  unzipper.DecodeXz(input_path, output_path, base::BindOnce([](bool result) {
                                               EXPECT_FALSE(result);
                                             }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace updater
