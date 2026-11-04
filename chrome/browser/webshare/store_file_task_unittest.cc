// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/store_file_task.h"

#include <unistd.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/safe_base_name.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"

namespace webshare {

class StoreFileTaskTest : public testing::Test {
 protected:
  void SetUp() override {
    StoreFileTask::SkipCopyingForTesting();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    share_dir_ = temp_dir_.GetPath().Append("WebShare").Append("share-1");
    ASSERT_TRUE(base::CreateDirectory(share_dir_));
    victim_ = temp_dir_.GetPath().Append("victim.txt");
    ASSERT_TRUE(base::WriteFile(victim_, "secret"));
  }

  // Runs a StoreFileTask for |path|. With SkipCopyingForTesting(), the
  // callback runs synchronously inside Start().
  blink::mojom::ShareError Store(const base::FilePath& path) {
    uint64_t available_space = 1024;
    std::optional<blink::mojom::ShareError> result;
    StoreFileTask task(
        path,
        blink::mojom::SharedFile::New(
            *base::SafeBaseName::Create(path.BaseName()),
            blink::mojom::SerializedBlob::New()),
        available_space,
        base::BindLambdaForTesting(
            [&](blink::mojom::ShareError error) { result = error; }));
    task.Start();
    EXPECT_TRUE(result.has_value());
    return result.value_or(blink::mojom::ShareError::INTERNAL_ERROR);
  }

  std::string ReadContents(const base::FilePath& path) {
    std::string contents;
    EXPECT_TRUE(base::ReadFileToString(path, &contents));
    return contents;
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath share_dir_;
  base::FilePath victim_;
};

TEST_F(StoreFileTaskTest, CreatesFile) {
  const base::FilePath leaf = share_dir_.Append("a.txt");
  EXPECT_EQ(Store(leaf), blink::mojom::ShareError::OK);
  EXPECT_TRUE(base::PathExists(leaf));
  EXPECT_FALSE(base::IsLink(leaf));
}

TEST_F(StoreFileTaskTest, LeafSymlinkIsNotFollowed) {
  const base::FilePath leaf = share_dir_.Append("a.txt");
  ASSERT_TRUE(base::CreateSymbolicLink(victim_, leaf));

  EXPECT_EQ(Store(leaf), blink::mojom::ShareError::OK);

  EXPECT_FALSE(base::IsLink(leaf));
  EXPECT_TRUE(ReadContents(leaf).empty());
  EXPECT_EQ(ReadContents(victim_), "secret");
}

TEST_F(StoreFileTaskTest, DanglingLeafSymlinkIsNotFollowed) {
  const base::FilePath leaf = share_dir_.Append("a.txt");
  const base::FilePath missing = temp_dir_.GetPath().Append("missing.txt");
  ASSERT_TRUE(base::CreateSymbolicLink(missing, leaf));

  EXPECT_EQ(Store(leaf), blink::mojom::ShareError::OK);

  EXPECT_FALSE(base::IsLink(leaf));
  EXPECT_TRUE(ReadContents(leaf).empty());
  EXPECT_FALSE(base::PathExists(missing));
}

TEST_F(StoreFileTaskTest, LeafHardLinkTargetIsNotTruncated) {
  const base::FilePath leaf = share_dir_.Append("a.txt");
  ASSERT_EQ(link(victim_.value().c_str(), leaf.value().c_str()), 0);

  EXPECT_EQ(Store(leaf), blink::mojom::ShareError::OK);

  EXPECT_TRUE(ReadContents(leaf).empty());
  EXPECT_EQ(ReadContents(victim_), "secret");
}

TEST_F(StoreFileTaskTest, StaleFileIsReplaced) {
  const base::FilePath leaf = share_dir_.Append("a.txt");
  ASSERT_TRUE(base::WriteFile(leaf, "old"));

  EXPECT_EQ(Store(leaf), blink::mojom::ShareError::OK);

  EXPECT_TRUE(ReadContents(leaf).empty());
}

TEST_F(StoreFileTaskTest, EmptyDirectoryAtLeafIsReplaced) {
  // DeleteFile() removes an empty directory, so a new file can be created.
  const base::FilePath leaf = share_dir_.Append("a.txt");
  ASSERT_TRUE(base::CreateDirectory(leaf));

  EXPECT_EQ(Store(leaf), blink::mojom::ShareError::OK);

  EXPECT_FALSE(base::DirectoryExists(leaf));
  EXPECT_TRUE(ReadContents(leaf).empty());
}

TEST_F(StoreFileTaskTest, FailsWhenFileCannotBeCreated) {
  // The leaf is a non-empty directory, so it cannot be removed or created.
  const base::FilePath leaf = share_dir_.Append("a.txt");
  ASSERT_TRUE(base::CreateDirectory(leaf.Append("child")));

  EXPECT_EQ(Store(leaf), blink::mojom::ShareError::PERMISSION_DENIED);

  EXPECT_TRUE(base::DirectoryExists(leaf.Append("child")));
}

TEST_F(StoreFileTaskTest, TaskCanBeDeletedInFailureCallback) {
  // StoreFilesTask deletes the StoreFileTask inside the callback. Start() must
  // not touch |this| after it runs the callback.
  const base::FilePath leaf = share_dir_.Append("a.txt");
  ASSERT_TRUE(base::CreateDirectory(leaf.Append("child")));

  uint64_t available_space = 1024;
  std::optional<blink::mojom::ShareError> result;
  std::unique_ptr<StoreFileTask> task;
  task = std::make_unique<StoreFileTask>(
      leaf,
      blink::mojom::SharedFile::New(
          *base::SafeBaseName::Create(leaf.BaseName()),
          blink::mojom::SerializedBlob::New()),
      available_space,
      base::BindLambdaForTesting([&](blink::mojom::ShareError error) {
        result = error;
        task.reset();
      }));
  task->Start();

  EXPECT_EQ(result, blink::mojom::ShareError::PERMISSION_DENIED);
  EXPECT_FALSE(task);
}

}  // namespace webshare
