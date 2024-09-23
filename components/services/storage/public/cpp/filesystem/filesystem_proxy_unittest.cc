// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/filesystem/filesystem_proxy.h"

#include <memory>

#include "base/check.h"
#include "base/files/file_error_or.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_impl.h"
#include "components/services/storage/public/mojom/filesystem/directory.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

template <typename ValueType>
using FileErrorOr = base::FileErrorOr<ValueType>;

namespace storage {

namespace {

constexpr char kFile1Contents[] = "Hello, world!";
constexpr char kFile2Contents[] = "Goodbye, cruel world!";
constexpr char kDir1File1Contents[] = "asdf";
constexpr char kDir1File2Contents[] = "qwerty";

using ::testing::UnorderedElementsAre;

std::string ReadFileContents(base::File* file) {
  std::vector<uint8_t> buffer(file->GetLength());
  CHECK(file->ReadAndCheck(0, buffer));
  return std::string(buffer.begin(), buffer.end());
}

}  // namespace

class FilesystemProxyTest : public testing::TestWithParam<bool> {
 public:
  const base::FilePath kFile1{FILE_PATH_LITERAL("file1")};
  const base::FilePath kFile2{FILE_PATH_LITERAL("file2")};
  const base::FilePath kDir1{FILE_PATH_LITERAL("dir1")};
  const base::FilePath kDir1File1{FILE_PATH_LITERAL("dir1file1")};
  const base::FilePath kDir1File2{FILE_PATH_LITERAL("dir1file2")};
  const base::FilePath kDir1Dir1{FILE_PATH_LITERAL("dir1dir1")};
  const base::FilePath kDir2{FILE_PATH_LITERAL("dir2")};
  const base::FilePath kDir2File1{FILE_PATH_LITERAL("dir2file1")};

  FilesystemProxyTest() = default;

  void SetUp() override {
    CHECK(temp_dir_.CreateUniqueTempDir());
    const base::FilePath root = temp_dir_.GetPath();

    // Populate the temporary root with some files and subdirectories.
    CHECK(base::CreateDirectory(root.Append(kDir1)));
    CHECK(base::CreateDirectory(root.Append(kDir1).Append(kDir1Dir1)));
    CHECK(base::CreateDirectory(root.Append(kDir2)));
    CHECK(base::WriteFile(root.Append(kFile1), kFile1Contents));
    CHECK(base::WriteFile(root.Append(kFile2), kFile2Contents));
    CHECK(base::WriteFile(root.Append(kDir1).Append(kDir1File1),
                          kDir1File1Contents));
    CHECK(base::WriteFile(root.Append(kDir1).Append(kDir1File2),
                          kDir1File2Contents));

    if (UseRestrictedFilesystem()) {
      // Run a remote FilesystemImpl on a background thread to exercise
      // restricted FilesystemProxy behavior.
      mojo::PendingRemote<mojom::Directory> remote;
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
          ->PostTask(
              FROM_HERE,
              base::BindOnce(
                  [](const base::FilePath& root,
                     mojo::PendingReceiver<mojom::Directory> receiver) {
                    mojo::MakeSelfOwnedReceiver(
                        std::make_unique<FilesystemImpl>(
                            root,
                            storage::FilesystemImpl::ClientType::kUntrusted),
                        std::move(receiver));
                  },
                  root, remote.InitWithNewPipeAndPassReceiver()));
      proxy_ = std::make_unique<FilesystemProxy>(
          FilesystemProxy::RESTRICTED, root, std::move(remote),
          base::ThreadPool::CreateSequencedTaskRunner({}));
    } else {
      proxy_ = std::make_unique<FilesystemProxy>(FilesystemProxy::UNRESTRICTED,
                                                 root);
    }
  }

  void TearDown() override {
    proxy_.reset();
    CHECK(temp_dir_.Delete());
  }

  base::FilePath GetTestRoot() { return temp_dir_.GetPath(); }

  FilesystemProxy& proxy() { return *proxy_; }

  base::FilePath MakeAbsolute(const base::FilePath& path) {
    DCHECK(!path.IsAbsolute());
    return GetTestRoot().Append(path);
  }

  std::string ReadFileContentsAtPath(const base::FilePath& path) {
    FileErrorOr<base::File> result =
        proxy().OpenFile(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    return ReadFileContents(&result.value());
  }

 private:
  bool UseRestrictedFilesystem() { return GetParam(); }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<FilesystemProxy> proxy_;
};

TEST_P(FilesystemProxyTest, PathExists) {
  EXPECT_TRUE(proxy().PathExists(kFile1));
  EXPECT_TRUE(proxy().PathExists(kDir1));
  EXPECT_TRUE(proxy().PathExists(kDir1.Append(kDir1File1)));
  EXPECT_FALSE(proxy().PathExists(kDir2.Append(kFile2)));
}

TEST_P(FilesystemProxyTest, GetDirectoryEntries) {
  FileErrorOr<std::vector<base::FilePath>> result = proxy().GetDirectoryEntries(
      base::FilePath(), FilesystemProxy::DirectoryEntryType::kFilesOnly);
  EXPECT_THAT(result, base::test::ValueIs(UnorderedElementsAre(
                          MakeAbsolute(kFile1), MakeAbsolute(kFile2))));

  result = proxy().GetDirectoryEntries(
      base::FilePath(),
      FilesystemProxy::DirectoryEntryType::kFilesAndDirectories);
  EXPECT_THAT(result, base::test::ValueIs(UnorderedElementsAre(
                          MakeAbsolute(kFile1), MakeAbsolute(kFile2),
                          MakeAbsolute(kDir1), MakeAbsolute(kDir2))));

  result = proxy().GetDirectoryEntries(
      kDir1, FilesystemProxy::DirectoryEntryType::kFilesOnly);
  EXPECT_THAT(result, base::test::ValueIs(UnorderedElementsAre(
                          MakeAbsolute(kDir1.Append(kDir1File1)),
                          MakeAbsolute(kDir1.Append(kDir1File2)))));

  result = proxy().GetDirectoryEntries(
      kDir1, FilesystemProxy::DirectoryEntryType::kFilesAndDirectories);
  EXPECT_THAT(result, base::test::ValueIs(UnorderedElementsAre(
                          MakeAbsolute(kDir1.Append(kDir1File1)),
                          MakeAbsolute(kDir1.Append(kDir1File2)),
                          MakeAbsolute(kDir1.Append(kDir1Dir1)))));

  EXPECT_EQ(
      base::File::FILE_ERROR_NOT_FOUND,
      proxy()
          .GetDirectoryEntries(base::FilePath(FILE_PATH_LITERAL("nope")),
                               FilesystemProxy::DirectoryEntryType::kFilesOnly)
          .error());
}

TEST_P(FilesystemProxyTest, OpenFileOpenIfExists) {
  const base::FilePath kNewFilename{FILE_PATH_LITERAL("new_file")};
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            proxy()
                .OpenFile(kNewFilename, base::File::FLAG_OPEN |
                                            base::File::FLAG_READ |
                                            base::File::FLAG_WRITE)
                .error());

  ASSERT_OK_AND_ASSIGN(
      base::File file1,
      proxy().OpenFile(kFile1, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                   base::File::FLAG_WRITE));
  EXPECT_EQ(kFile1Contents, ReadFileContents(&file1));
}

TEST_P(FilesystemProxyTest, OpenFileCreateAndOpenOnlyIfNotExists) {
  EXPECT_EQ(
      base::File::FILE_ERROR_EXISTS,
      proxy()
          .OpenFile(kFile1, base::File::FLAG_CREATE | base::File::FLAG_READ |
                                base::File::FLAG_WRITE)
          .error());

  const base::FilePath kNewFilename{FILE_PATH_LITERAL("new_file")};
  ASSERT_OK_AND_ASSIGN(
      base::File new_file,
      proxy().OpenFile(kNewFilename, base::File::FLAG_CREATE |
                                         base::File::FLAG_READ |
                                         base::File::FLAG_WRITE));
  EXPECT_EQ("", ReadFileContents(&new_file));

  const std::string kData = "yeet";
  EXPECT_TRUE(
      new_file.WriteAndCheck(0, base::as_bytes(base::make_span(kData))));
  EXPECT_EQ(kData, ReadFileContents(&new_file));
}

TEST_P(FilesystemProxyTest, OpenFileAlwaysOpen) {
  ASSERT_OK_AND_ASSIGN(base::File file1,
                       proxy().OpenFile(kFile1, base::File::FLAG_OPEN_ALWAYS |
                                                    base::File::FLAG_READ |
                                                    base::File::FLAG_WRITE));
  EXPECT_TRUE(file1.IsValid());
  EXPECT_EQ(kFile1Contents, ReadFileContents(&file1));

  const base::FilePath kNewFilename{FILE_PATH_LITERAL("new_file")};
  ASSERT_OK_AND_ASSIGN(
      base::File new_file,
      proxy().OpenFile(kNewFilename, base::File::FLAG_OPEN_ALWAYS |
                                         base::File::FLAG_READ |
                                         base::File::FLAG_WRITE));
  EXPECT_TRUE(new_file.IsValid());
  EXPECT_EQ("", ReadFileContents(&new_file));
}

TEST_P(FilesystemProxyTest, OpenFileAlwaysCreate) {
  ASSERT_OK_AND_ASSIGN(base::File file1,
                       proxy().OpenFile(kFile1, base::File::FLAG_CREATE_ALWAYS |
                                                    base::File::FLAG_READ |
                                                    base::File::FLAG_WRITE));
  EXPECT_TRUE(file1.IsValid());
  EXPECT_EQ("", ReadFileContents(&file1));

  const base::FilePath kNewFilename{FILE_PATH_LITERAL("new_file")};
  ASSERT_OK_AND_ASSIGN(
      base::File new_file,
      proxy().OpenFile(kNewFilename, base::File::FLAG_CREATE_ALWAYS |
                                         base::File::FLAG_READ |
                                         base::File::FLAG_WRITE));
  EXPECT_TRUE(new_file.IsValid());
  EXPECT_EQ("", ReadFileContents(&new_file));
}

TEST_P(FilesystemProxyTest, OpenFileOpenIfExistsAndTruncate) {
  const base::FilePath kNewFilename{FILE_PATH_LITERAL("new_file")};
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            proxy()
                .OpenFile(kNewFilename, base::File::FLAG_OPEN_TRUNCATED |
                                            base::File::FLAG_READ |
                                            base::File::FLAG_WRITE)
                .error());

  ASSERT_OK_AND_ASSIGN(
      base::File file1,
      proxy().OpenFile(kFile1, base::File::FLAG_OPEN_TRUNCATED |
                                   base::File::FLAG_READ |
                                   base::File::FLAG_WRITE));
  EXPECT_TRUE(file1.IsValid());
  EXPECT_EQ("", ReadFileContents(&file1));
}

TEST_P(FilesystemProxyTest, OpenFileReadOnly) {
  ASSERT_OK_AND_ASSIGN(
      base::File file,
      proxy().OpenFile(kFile1, base::File::FLAG_OPEN | base::File::FLAG_READ));
  EXPECT_TRUE(file.IsValid());

  // Writes should fail.
  EXPECT_FALSE(file.WriteAtCurrentPosAndCheck(
      base::as_bytes(base::make_span("doesn't matter"))));
  EXPECT_EQ(kFile1Contents, ReadFileContents(&file));
}

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/40221280): Re-enable when OpenFileWriteOnly works on Fuchsia.
#define MAYBE_OpenFileWriteOnly DISABLED_OpenFileWriteOnly
#else
#define MAYBE_OpenFileWriteOnly OpenFileWriteOnly
#endif
TEST_P(FilesystemProxyTest, MAYBE_OpenFileWriteOnly) {
  ASSERT_OK_AND_ASSIGN(base::File file,
                       proxy().OpenFile(kFile2, base::File::FLAG_CREATE_ALWAYS |
                                                    base::File::FLAG_WRITE));
  EXPECT_TRUE(file.IsValid());

  const std::string kData{"files can have a little data, as a treat"};
  EXPECT_TRUE(file.WriteAndCheck(0, base::as_bytes(base::make_span(kData))));

  // Reading from this handle should fail.
  std::vector<uint8_t> data;
  EXPECT_FALSE(file.ReadAndCheck(0, data));
  file.Close();

  // But the file contents should still have been written.
  EXPECT_EQ(kData, ReadFileContentsAtPath(kFile2));
}

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/40221278): Re-enable when OpenFileAppendOnly works on Fuchsia.
#define MAYBE_OpenFileAppendOnly DISABLED_OpenFileAppendOnly
#else
#define MAYBE_OpenFileAppendOnly OpenFileAppendOnly
#endif
TEST_P(FilesystemProxyTest, MAYBE_OpenFileAppendOnly) {
  const base::FilePath kFile3{FILE_PATH_LITERAL("file3")};
  ASSERT_OK_AND_ASSIGN(base::File file,
                       proxy().OpenFile(kFile3, base::File::FLAG_CREATE |
                                                    base::File::FLAG_APPEND));
  EXPECT_TRUE(file.IsValid());

  const std::string kData{"files can have a little data, as a treat"};
  EXPECT_TRUE(
      file.WriteAtCurrentPosAndCheck(base::as_bytes(base::make_span(kData))));

  // Attempt to write somewhere other than the end of the file. The offset
  // should be ignored and the data should be appended instead.
  const std::string kMoreData{"!"};
  EXPECT_TRUE(
      file.WriteAndCheck(0, base::as_bytes(base::make_span(kMoreData))));

  // Reading should still fail.
  std::vector<uint8_t> data;
  EXPECT_FALSE(file.ReadAndCheck(0, data));
  file.Close();

  // But we should have all the appended data in the file.
  EXPECT_EQ(kData + kMoreData, ReadFileContentsAtPath(kFile3));
}

TEST_P(FilesystemProxyTest, DeleteFile) {
  ASSERT_OK_AND_ASSIGN(
      base::File file,
      proxy().OpenFile(kFile1, base::File::FLAG_OPEN | base ::File::FLAG_READ));
  EXPECT_TRUE(file.IsValid());
  file.Close();

  EXPECT_TRUE(proxy().DeleteFile(kFile1));
  EXPECT_THAT(
      proxy().OpenFile(kFile1, base::File::FLAG_OPEN | base ::File::FLAG_READ),
      base::test::ErrorIs(base::File::FILE_ERROR_NOT_FOUND));
}

TEST_P(FilesystemProxyTest, CreateAndRemoveDirectory) {
  const base::FilePath kNewDirectoryName{FILE_PATH_LITERAL("new_dir")};

  EXPECT_TRUE(proxy().DeleteFile(kNewDirectoryName));

  EXPECT_EQ(base::File::FILE_OK, proxy().CreateDirectory(kNewDirectoryName));
  EXPECT_TRUE(proxy().PathExists(kNewDirectoryName));

  EXPECT_TRUE(proxy().DeleteFile(kNewDirectoryName));

  EXPECT_FALSE(proxy().PathExists(kNewDirectoryName));
  EXPECT_TRUE(proxy().DeleteFile(kNewDirectoryName));
}

TEST_P(FilesystemProxyTest, DeleteFileFailsOnSubDirectory) {
  // kDir1 has a subdirectory kDir1Dir1, which DeleteFile can't remove.
  EXPECT_TRUE(proxy().PathExists(kDir1));
  EXPECT_FALSE(proxy().DeleteFile(kDir1));
  EXPECT_TRUE(proxy().PathExists(kDir1));
}

TEST_P(FilesystemProxyTest, GetFileInfo) {
  std::optional<base::File::Info> file1_info = proxy().GetFileInfo(kFile1);
  ASSERT_TRUE(file1_info.has_value());
  EXPECT_FALSE(file1_info->is_directory);
  EXPECT_EQ(static_cast<int>(std::size(kFile1Contents) - 1), file1_info->size);

  std::optional<base::File::Info> dir1_info = proxy().GetFileInfo(kDir1);
  ASSERT_TRUE(dir1_info.has_value());
  EXPECT_TRUE(dir1_info->is_directory);

  std::optional<base::File::Info> dir1_file1_info =
      proxy().GetFileInfo(kDir1.Append(kDir1File1));
  ASSERT_TRUE(dir1_file1_info.has_value());
  EXPECT_FALSE(dir1_file1_info->is_directory);
  EXPECT_EQ(static_cast<int>(std::size(kDir1File1Contents) - 1),
            dir1_file1_info->size);

  const base::FilePath kBadFilename{FILE_PATH_LITERAL("bad_file")};
  EXPECT_FALSE(proxy().GetFileInfo(kBadFilename).has_value());
}

TEST_P(FilesystemProxyTest, RenameFile) {
  const base::FilePath kNewFilename{FILE_PATH_LITERAL("new_file")};
  EXPECT_EQ(base::File::FILE_OK, proxy().RenameFile(kFile1, kNewFilename));

  EXPECT_EQ(
      base::File::FILE_ERROR_NOT_FOUND,
      proxy()
          .OpenFile(kFile1, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                base::File::FLAG_WRITE)
          .error());

  ASSERT_OK_AND_ASSIGN(
      base::File new_file,
      proxy().OpenFile(kNewFilename, base::File::FLAG_OPEN |
                                         base::File::FLAG_READ |
                                         base::File::FLAG_WRITE));
  EXPECT_TRUE(new_file.IsValid());
  EXPECT_EQ(kFile1Contents, ReadFileContents(&new_file));
}

TEST_P(FilesystemProxyTest, RenameNonExistentFile) {
  const base::FilePath kBadFilename{FILE_PATH_LITERAL("bad_file")};
  const base::FilePath kNewFilename{FILE_PATH_LITERAL("new_file")};
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            proxy().RenameFile(kBadFilename, kNewFilename));
}

TEST_P(FilesystemProxyTest, LockFile) {
  const base::FilePath kLockFilename{FILE_PATH_LITERAL("lox")};
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<FilesystemProxy::FileLock> result,
                       proxy().LockFile(kLockFilename));
  EXPECT_NE(nullptr, result);

  EXPECT_THAT(proxy().LockFile(kLockFilename),
              base::test::ErrorIs(base::File::FILE_ERROR_IN_USE));

  // Synchronously release so we can re-acquire the lock.
  EXPECT_EQ(base::File::Error::FILE_OK, result->Release());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<FilesystemProxy::FileLock> result2,
                       proxy().LockFile(kLockFilename));
  EXPECT_NE(nullptr, result2);

  // Test that destruction also implicitly releases the lock.
  result2.reset();

  // And once again we should be able to reacquire the lock.
  ASSERT_OK_AND_ASSIGN(result, proxy().LockFile(kLockFilename));
  EXPECT_NE(nullptr, result);
}

TEST_P(FilesystemProxyTest, AbsolutePathEqualToRoot) {
  // Verifies that if a delegate is given an absolute path identical to its
  // root path, it is correctly resolved to an empty relative path and can
  // operate correctly.
  FileErrorOr<std::vector<base::FilePath>> result = proxy().GetDirectoryEntries(
      GetTestRoot(), FilesystemProxy::DirectoryEntryType::kFilesAndDirectories);
  EXPECT_THAT(result, base::test::ValueIs(UnorderedElementsAre(
                          MakeAbsolute(kFile1), MakeAbsolute(kFile2),
                          MakeAbsolute(kDir1), MakeAbsolute(kDir2))));
}

TEST_P(FilesystemProxyTest, AbsolutePathWithinRoot) {
  // Verifies that if a delegate is given an absolute path which falls within
  // its root path, it is correctly resolved to a relative path suitable for use
  // with the Directory IPC interface.
  FileErrorOr<std::vector<base::FilePath>> result = proxy().GetDirectoryEntries(
      GetTestRoot().Append(kDir1),
      FilesystemProxy::DirectoryEntryType::kFilesAndDirectories);
  EXPECT_THAT(result, base::test::ValueIs(UnorderedElementsAre(
                          MakeAbsolute(kDir1.Append(kDir1File1)),
                          MakeAbsolute(kDir1.Append(kDir1File2)),
                          MakeAbsolute(kDir1.Append(kDir1Dir1)))));
}

INSTANTIATE_TEST_SUITE_P(, FilesystemProxyTest, testing::Bool());

}  // namespace storage
