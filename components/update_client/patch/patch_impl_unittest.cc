// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/patch/patch_impl.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/services/patch/public/mojom/file_patcher.mojom.h"
#include "components/update_client/update_client_errors.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

namespace {

// A no-op callback for the patcher.
mojo::PendingRemote<patch::mojom::FilePatcher> MakePatcher() {
  return mojo::PendingRemote<patch::mojom::FilePatcher>();
}

}  // namespace

class PatchImplTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::File CreateTestFile(const std::string& content) {
    base::FilePath file_path;
    EXPECT_TRUE(
        base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &file_path));
    EXPECT_TRUE(base::WriteFile(file_path, content));
    return base::File(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<Patcher> patcher_ = base::MakeRefCounted<PatchChromiumFactory>(
                                        base::BindRepeating(&MakePatcher))
                                        ->Create();
};

TEST_F(PatchImplTest, PuffPatch_InvalidOldFile) {
  base::RunLoop run_loop;
  patcher_->PatchPuffPatch(
      base::File(), CreateTestFile("patch"), CreateTestFile(""),
      base::BindOnce(
          [](base::OnceClosure quit_closure, int result) {
            EXPECT_EQ(result,
                      static_cast<int>(UnpackerError::kPatchInvalidOldFile));
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(PatchImplTest, PuffPatch_InvalidPatchFile) {
  base::RunLoop run_loop;
  patcher_->PatchPuffPatch(
      CreateTestFile("old"), base::File(), CreateTestFile(""),
      base::BindOnce(
          [](base::OnceClosure quit_closure, int result) {
            EXPECT_EQ(result,
                      static_cast<int>(UnpackerError::kPatchInvalidPatchFile));
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(PatchImplTest, PuffPatch_InvalidDestinationFile) {
  base::RunLoop run_loop;
  patcher_->PatchPuffPatch(
      CreateTestFile("old"), CreateTestFile("patch"), base::File(),
      base::BindOnce(
          [](base::OnceClosure quit_closure, int result) {
            EXPECT_EQ(result,
                      static_cast<int>(UnpackerError::kPatchInvalidNewFile));
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(PatchImplTest, ZucchiniPatch_InvalidOldFile) {
  base::RunLoop run_loop;
  patcher_->PatchZucchini(
      base::File(), CreateTestFile("patch"), CreateTestFile(""),
      base::BindOnce(
          [](base::OnceClosure quit_closure, int result) {
            EXPECT_EQ(result,
                      static_cast<int>(UnpackerError::kPatchInvalidOldFile));
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(PatchImplTest, ZucchiniPatch_InvalidPatchFile) {
  base::RunLoop run_loop;
  patcher_->PatchZucchini(
      CreateTestFile("old"), base::File(), CreateTestFile(""),
      base::BindOnce(
          [](base::OnceClosure quit_closure, int result) {
            EXPECT_EQ(result,
                      static_cast<int>(UnpackerError::kPatchInvalidPatchFile));
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(PatchImplTest, ZucchiniPatch_InvalidDestinationFile) {
  base::RunLoop run_loop;
  patcher_->PatchZucchini(
      CreateTestFile("old"), CreateTestFile("patch"), base::File(),
      base::BindOnce(
          [](base::OnceClosure quit_closure, int result) {
            EXPECT_EQ(result,
                      static_cast<int>(UnpackerError::kPatchInvalidNewFile));
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace update_client
