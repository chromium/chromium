// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/zip_file_creator.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/services/file_util/public/mojom/file_util_service.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip_reader.h"

namespace {

class ZipFileCreatorTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateDirectory(zip_base_dir()));
  }

  base::FilePath zip_archive_path() const {
    return dir_.GetPath().AppendASCII("test.zip");
  }

  base::FilePath zip_base_dir() const {
    return dir_.GetPath().AppendASCII("files");
  }

  mojo::PendingRemote<chrome::mojom::FileUtilService> LaunchService() {
    mojo::PendingRemote<chrome::mojom::FileUtilService> service;
    content::ServiceProcessHost::Launch(
        service.InitWithNewPipeAndPassReceiver());
    return service;
  }

 protected:
  base::ScopedTempDir dir_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ZipFileCreatorTest, FailZipForAbsentFile) {
  base::RunLoop run_loop;

  std::vector<base::FilePath> paths;
  paths.push_back(base::FilePath(FILE_PATH_LITERAL("not.exist")));

  const scoped_refptr<ZipFileCreator> creator =
      base::MakeRefCounted<ZipFileCreator>(zip_base_dir(), paths,
                                           zip_archive_path());

  creator->SetCompletionCallback(run_loop.QuitClosure());
  creator->Start(LaunchService());

  run_loop.Run();
  EXPECT_EQ(ZipFileCreator::kSuccess, creator->GetResult());

  // Check final progress.
  {
    const ZipFileCreator::Progress progress = creator->GetProgress();
    EXPECT_EQ(0, progress.bytes);
    EXPECT_EQ(0, progress.files);
    EXPECT_EQ(0, progress.directories);
    EXPECT_LE(2, progress.update_count);
    EXPECT_EQ(ZipFileCreator::kSuccess, progress.result);
  }
}

IN_PROC_BROWSER_TEST_F(ZipFileCreatorTest, SomeFilesZip) {
  // Prepare files.
  const base::FilePath kDir1(FILE_PATH_LITERAL("foo"));
  const base::FilePath kFile1(kDir1.AppendASCII("bar"));
  const base::FilePath kFile2(FILE_PATH_LITERAL("random"));
  const int kRandomDataSize = 100000;
  const std::string kRandomData = base::RandBytesAsString(kRandomDataSize);
  {
    const base::ScopedAllowBlockingForTesting allow_io;
    ASSERT_TRUE(base::CreateDirectory(zip_base_dir().Append(kDir1)));
    ASSERT_TRUE(base::WriteFile(zip_base_dir().Append(kFile1), "123"));
    ASSERT_TRUE(base::WriteFile(zip_base_dir().Append(kFile2), kRandomData));
  }

  base::RunLoop run_loop;

  const scoped_refptr<ZipFileCreator> creator =
      base::MakeRefCounted<ZipFileCreator>(
          zip_base_dir(), std::initializer_list<base::FilePath>{kDir1, kFile2},
          zip_archive_path());

  creator->SetCompletionCallback(run_loop.QuitClosure());
  creator->Start(LaunchService());

  run_loop.Run();
  EXPECT_EQ(ZipFileCreator::kSuccess, creator->GetResult());

  // Check final progress.
  {
    const ZipFileCreator::Progress progress = creator->GetProgress();
    EXPECT_EQ(100003, progress.bytes);
    EXPECT_EQ(2, progress.files);
    EXPECT_EQ(1, progress.directories);
    EXPECT_LE(2, progress.update_count);
    EXPECT_GT(100, progress.update_count);
    EXPECT_EQ(ZipFileCreator::kSuccess, progress.result);
  }

  const base::ScopedAllowBlockingForTesting allow_io;

  // Check the archive content.
  zip::ZipReader reader;
  ASSERT_TRUE(reader.Open(zip_archive_path()));
  EXPECT_EQ(3, reader.num_entries());
  while (const zip::ZipReader::Entry* const entry = reader.Next()) {
    // ZipReader returns directory path with trailing slash.
    if (entry->path == kDir1.AsEndingWithSeparator()) {
      EXPECT_TRUE(entry->is_directory);
    } else if (entry->path == kFile1) {
      EXPECT_FALSE(entry->is_directory);
      EXPECT_EQ(3, entry->original_size);
    } else if (entry->path == kFile2) {
      EXPECT_FALSE(entry->is_directory);
      EXPECT_EQ(kRandomDataSize, entry->original_size);

      const base::FilePath out = dir_.GetPath().AppendASCII("archived_content");
      zip::FilePathWriterDelegate writer(out);
      EXPECT_TRUE(reader.ExtractCurrentEntry(
          &writer, std::numeric_limits<uint64_t>::max()));
      EXPECT_TRUE(base::ContentsEqual(zip_base_dir().Append(kFile2), out));
    } else {
      ADD_FAILURE();
    }
  }
  EXPECT_TRUE(reader.ok());
}

IN_PROC_BROWSER_TEST_F(ZipFileCreatorTest, Cancellation) {
  // Prepare big file.
  const base::FilePath kFile("big");
  const int64_t kSize = 4'000'000'000;

  {
    const base::ScopedAllowBlockingForTesting allow_io;
    base::File f(zip_base_dir().Append(kFile),
                 base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(f.SetLength(kSize));
  }

  base::RunLoop run_loop;

  const scoped_refptr<ZipFileCreator> creator =
      base::MakeRefCounted<ZipFileCreator>(
          zip_base_dir(), std::initializer_list<base::FilePath>{kFile},
          zip_archive_path());

  // Cancel the ZIP creation operation as soon as we get indication of progress.
  creator->SetProgressCallback(base::BindLambdaForTesting([&]() {
    EXPECT_EQ(ZipFileCreator::kInProgress, creator->GetResult());
    creator->Stop();
  }));

  creator->SetCompletionCallback(run_loop.QuitClosure());
  creator->Start(LaunchService());
  run_loop.Run();
  EXPECT_EQ(ZipFileCreator::kCancelled, creator->GetResult());
}

IN_PROC_BROWSER_TEST_F(ZipFileCreatorTest, DISABLED_BigFile) {
  // Prepare big file (ie bigger than 4GB).
  const base::FilePath kFile("big");
  const int64_t kSize = 5'000'000'000;

  {
    const base::ScopedAllowBlockingForTesting allow_io;
    base::File f(zip_base_dir().Append(kFile),
                 base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(f.SetLength(kSize));
  }

  base::RunLoop run_loop;

  const scoped_refptr<ZipFileCreator> creator =
      base::MakeRefCounted<ZipFileCreator>(
          zip_base_dir(), std::initializer_list<base::FilePath>{kFile},
          zip_archive_path());

  // Check initial progress.
  {
    const ZipFileCreator::Progress progress = creator->GetProgress();
    EXPECT_EQ(0, progress.bytes);
    EXPECT_EQ(0, progress.files);
    EXPECT_EQ(0, progress.directories);
    EXPECT_EQ(0, progress.update_count);
    EXPECT_EQ(ZipFileCreator::kInProgress, progress.result);
  }

  creator->SetCompletionCallback(run_loop.QuitClosure());
  creator->Start(LaunchService());

  run_loop.Run();
  EXPECT_EQ(ZipFileCreator::kSuccess, creator->GetResult());

  // Check final progress.
  {
    const ZipFileCreator::Progress progress = creator->GetProgress();
    EXPECT_EQ(kSize, progress.bytes);
    EXPECT_EQ(1, progress.files);
    EXPECT_EQ(0, progress.directories);
    EXPECT_LE(2, progress.update_count);
    EXPECT_GT(100, progress.update_count);
    EXPECT_EQ(ZipFileCreator::kSuccess, progress.result);
  }

  const base::ScopedAllowBlockingForTesting allow_io;

  // Check the archive content.
  zip::ZipReader reader;
  ASSERT_TRUE(reader.Open(zip_archive_path()));
  while (const zip::ZipReader::Entry* const entry = reader.Next()) {
    if (entry->path == kFile) {
      EXPECT_FALSE(entry->is_directory);
      EXPECT_EQ(kSize, entry->original_size);
    } else {
      ADD_FAILURE();
    }
  }
  EXPECT_TRUE(reader.ok());
}

IN_PROC_BROWSER_TEST_F(ZipFileCreatorTest, ZipDirectoryWithManyFiles) {
  // Create the following file tree structure:
  // root_dir/
  // root_dir/1.txt -> Hello1
  // root_dir/2.txt -> Hello2
  // ...
  // root_dir/89.txt -> Hello89
  // root_dir/1
  // root_dir/1/1.txt -> Hello1/1
  // ...
  // root_dir/1/70.txt -> Hello1/70
  // root_dir/2
  // root_dir/2/1.txt -> Hello2/1
  // ...
  // root_dir/2/70.txt -> Hello2/70
  //...
  //...
  // root_dir/10/70.txt -> Hello10/70

  base::FilePath root_dir = zip_base_dir().Append("root_dir");

  // File paths to file content. Used for validation.
  std::map<base::FilePath, std::string> file_tree_content;
  {
    const base::ScopedAllowBlockingForTesting allow_io;
    ASSERT_TRUE(base::CreateDirectory(root_dir));

    for (int i = 1; i < 90; i++) {
      base::FilePath file(base::NumberToString(i) + ".txt");
      std::string content = "Hello" + base::NumberToString(i);
      ASSERT_TRUE(base::WriteFile(root_dir.Append(file), content));
      file_tree_content[file] = content;
    }
    for (int i = 1; i <= 10; i++) {
      base::FilePath dir(base::NumberToString(i));
      ASSERT_TRUE(base::CreateDirectory(root_dir.Append(dir)));
      file_tree_content[dir] = std::string();
      for (int j = 1; j <= 70; j++) {
        base::FilePath file = dir.Append(base::NumberToString(j) + ".txt");
        std::string content =
            "Hello" + base::NumberToString(i) + "/" + base::NumberToString(j);
        ASSERT_TRUE(base::WriteFile(root_dir.Append(file), content));
        file_tree_content[file] = content;
      }
    }
  }

  // Sanity check on the files created.
  constexpr size_t kEntryCount = 89 /* files under root dir */ +
                                 10 /* 1 to 10 dirs */ +
                                 10 * 70 /* files under 1 to 10 dirs */;
  DCHECK_EQ(kEntryCount, file_tree_content.size());

  base::RunLoop run_loop;
  const scoped_refptr<ZipFileCreator> creator =
      base::MakeRefCounted<ZipFileCreator>(
          root_dir,
          std::initializer_list<base::FilePath>{},  // Everything in root_dir
          zip_archive_path());

  creator->SetCompletionCallback(run_loop.QuitClosure());
  creator->Start(LaunchService());

  run_loop.Run();
  EXPECT_EQ(ZipFileCreator::kSuccess, creator->GetResult());

  // Check final progress.
  {
    const ZipFileCreator::Progress progress = creator->GetProgress();
    EXPECT_EQ(6894, progress.bytes);
    EXPECT_EQ(789, progress.files);
    EXPECT_EQ(10, progress.directories);
    EXPECT_LE(2, progress.update_count);
    EXPECT_GT(100, progress.update_count);
    EXPECT_EQ(ZipFileCreator::kSuccess, progress.result);
  }

  // Check the archive content.
  zip::ZipReader reader;
  ASSERT_TRUE(reader.Open(zip_archive_path()));
  EXPECT_EQ(file_tree_content.size(),
            static_cast<size_t>(reader.num_entries()));
  while (const zip::ZipReader::Entry* const entry = reader.Next()) {
    base::FilePath path = entry->path.StripTrailingSeparators();
    auto iter = file_tree_content.find(path);
    ASSERT_NE(iter, file_tree_content.end())
        << "Path not found in unzipped archive: " << path;
    const std::string& expected_content = iter->second;
    if (expected_content.empty()) {
      EXPECT_TRUE(entry->is_directory);
    } else {
      // It's a file.
      EXPECT_FALSE(entry->is_directory);
      std::string actual_content;
      EXPECT_TRUE(reader.ExtractCurrentEntryToString(
          10 * 1024,  // 10KB, any of our test data is less than that.
          &actual_content));
      EXPECT_EQ(expected_content, actual_content);
    }
    file_tree_content.erase(iter);
  }
  EXPECT_TRUE(reader.ok());
  EXPECT_TRUE(file_tree_content.empty());
}
