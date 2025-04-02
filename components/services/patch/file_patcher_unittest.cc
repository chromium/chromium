// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/services/patch/file_patcher_impl.h"
#include "components/services/patch/in_process_file_patcher.h"
#include "components/services/patch/public/cpp/patch.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace patch {
namespace {

base::FilePath TestFile(std::string_view basename) {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path));
  return path.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("patch_service")
      .AppendASCII(basename);
}

int DoZucchini(const base::FilePath& old_file,
               const base::FilePath& patch_file,
               const base::FilePath& new_file) {
  base::RunLoop run_loop;
  zucchini::status::Code result = zucchini::status::kStatusSuccess;
  base::File old =
      base::File(old_file, base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File patch =
      base::File(patch_file, base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File output = base::File(
      new_file, base::File::FLAG_CREATE | base::File::FLAG_READ |
                    base::File::FLAG_WRITE | base::File::FLAG_WIN_SHARE_DELETE |
                    base::File::FLAG_CAN_DELETE_ON_CLOSE);
  EXPECT_TRUE(old.IsValid());
  EXPECT_TRUE(patch.IsValid());
  EXPECT_TRUE(output.IsValid());
  ZucchiniPatch(LaunchInProcessFilePatcher(), std::move(old), std::move(patch),
                std::move(output),
                base::BindLambdaForTesting([&](zucchini::status::Code code) {
                  result = code;
                  run_loop.QuitClosure().Run();
                }));
  run_loop.Run();
  return result;
}

}  // namespace

class PatchTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    new_file_ = temp_dir_.GetPath().AppendASCII("new_file");
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath new_file_;
};

TEST_F(PatchTest, ZucchiniPatchSuccess) {
  ASSERT_EQ(DoZucchini(TestFile("old_file"), TestFile("patch_file"), new_file_),
            0);
  EXPECT_EQ(ReadFileToBytes(new_file_), ReadFileToBytes(TestFile("new_file")));
}

TEST_F(PatchTest, ZucchiniPatchFail) {
  ASSERT_NE(DoZucchini(TestFile("new_file"), TestFile("patch_file"), new_file_),
            0);
}

TEST_F(PatchTest, ZucchiniPatchMalformed) {
  ASSERT_NE(DoZucchini(TestFile("old_file"), TestFile("old_file"), new_file_),
            0);
}

}  // namespace patch
