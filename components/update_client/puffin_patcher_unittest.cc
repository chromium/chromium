// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/puffin_patcher.h"

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/services/patch/in_process_file_patcher.h"
#include "components/update_client/patch/patch_impl.h"
#include "components/update_client/test_installer.h"
#include "components/update_client/update_client_errors.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff/bsdiff.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

namespace {

base::FilePath OutTestFile(const char* file) {
  base::FilePath path;
  base::PathService::Get(base::DIR_GEN_TEST_DATA_ROOT, &path);
  return path.AppendASCII(file);
}

}  // namespace

class PuffinPatcherTest : public testing::Test {
 public:
  PuffinPatcherTest() = default;
  ~PuffinPatcherTest() override = default;

 protected:
  base::test::TaskEnvironment env_;
};

TEST_F(PuffinPatcherTest, CheckPuffPatch) {
  // The operation needs a Patcher to access the PatchService.
  scoped_refptr<Patcher> patcher =
      base::MakeRefCounted<PatchChromiumFactory>(
          base::BindRepeating(&patch::LaunchInProcessFilePatcher))
          ->Create();

  base::FilePath out_file = OutTestFile("puffin_app_v1_to_v2.crx3");
  EXPECT_TRUE(base::DeleteFile(out_file));
  SEQUENCE_CHECKER(sequence_checker);
  {
    base::File input_file(OutTestFile("puffin_app_v1.crx3"),
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
    base::File patch_file(OutTestFile("puffin_app_v1_to_v2.puff"),
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
    base::File output_file(out_file, base::File::FLAG_CREATE |
                                         base::File::FLAG_WRITE |
                                         base::File::FLAG_WIN_EXCLUSIVE_WRITE);
    base::RunLoop loop;
    PuffinPatcher::Patch(
        std::move(input_file), std::move(patch_file), std::move(output_file),
        patcher,
        base::BindLambdaForTesting(
            [&loop, &sequence_checker](UnpackerError error, int extra_code) {
              DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
              EXPECT_EQ(error, UnpackerError::kNone);
              EXPECT_EQ(extra_code, 0);
              loop.Quit();
            }));
    loop.Run();
  }

  EXPECT_TRUE(base::ContentsEqual(OutTestFile("puffin_app_v2.crx3"), out_file));

  out_file = OutTestFile("puffin_app_v2_to_v1.crx3");
  EXPECT_TRUE(base::DeleteFile(out_file));
  {
    base::File input_file(OutTestFile("puffin_app_v2.crx3"),
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
    base::File patch_file(OutTestFile("puffin_app_v2_to_v1.puff"),
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
    base::File output_file(out_file, base::File::FLAG_CREATE |
                                         base::File::FLAG_WRITE |
                                         base::File::FLAG_WIN_EXCLUSIVE_WRITE);
    base::RunLoop loop;
    PuffinPatcher::Patch(
        std::move(input_file), std::move(patch_file), std::move(output_file),
        patcher,
        base::BindLambdaForTesting(
            [&loop, &sequence_checker](UnpackerError error, int extra_code) {
              DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
              EXPECT_EQ(error, UnpackerError::kNone);
              EXPECT_EQ(extra_code, 0);
              loop.Quit();
            }));
    loop.Run();
  }

  DETACH_FROM_SEQUENCE(sequence_checker);
  EXPECT_TRUE(base::ContentsEqual(OutTestFile("puffin_app_v1.crx3"), out_file));
}

}  // namespace update_client
