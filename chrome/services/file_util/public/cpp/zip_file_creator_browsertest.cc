// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/zip_file_creator.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip_reader.h"

namespace {

void TestCallback(bool* out_success, base::OnceClosure quit, bool success) {
  *out_success = success;
  std::move(quit).Run();
}

bool CreateFile(const base::FilePath& file, const std::string& content) {
  return base::WriteFile(file, content.c_str(), content.size()) ==
         static_cast<int>(content.size());
}

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
  bool success = true;

  std::vector<base::FilePath> paths;
  paths.push_back(base::FilePath(FILE_PATH_LITERAL("not.exist")));
  (new ZipFileCreator(
       base::BindOnce(&TestCallback, &success, run_loop.QuitClosure()),
       zip_base_dir(), paths, zip_archive_path()))
      ->Start(LaunchService());

  run_loop.Run();
  EXPECT_FALSE(success);
}

IN_PROC_BROWSER_TEST_F(ZipFileCreatorTest, SomeFilesZip) {
  // Prepare files.
  const base::FilePath kDir1(FILE_PATH_LITERAL("foo"));
  const base::FilePath kFile1(kDir1.AppendASCII("bar"));
  const base::FilePath kFile2(FILE_PATH_LITERAL("random"));
  const int kRandomDataSize = 100000;
  const std::string kRandomData = base::RandBytesAsString(kRandomDataSize);
  {
    base::ScopedAllowBlockingForTesting allow_io;
    base::CreateDirectory(zip_base_dir().Append(kDir1));
    base::WriteFile(zip_base_dir().Append(kFile1), "123", 3);
    base::WriteFile(zip_base_dir().Append(kFile2), kRandomData.c_str(),
                    kRandomData.size());
  }

  bool success = false;
  base::RunLoop run_loop;

  std::vector<base::FilePath> paths;
  paths.push_back(kDir1);
  paths.push_back(kFile1);
  paths.push_back(kFile2);
  (new ZipFileCreator(
       base::BindOnce(&TestCallback, &success, run_loop.QuitClosure()),
       zip_base_dir(), paths, zip_archive_path()))
      ->Start(LaunchService());

  run_loop.Run();
  EXPECT_TRUE(success);

  base::ScopedAllowBlockingForTesting allow_io;

  // Check the archive content.
  zip::ZipReader reader;
  ASSERT_TRUE(reader.Open(zip_archive_path()));
  EXPECT_EQ(3, reader.num_entries());
  while (reader.HasMore()) {
    ASSERT_TRUE(reader.OpenCurrentEntryInZip());
    const zip::ZipReader::EntryInfo* entry = reader.current_entry_info();
    // ZipReader returns directory path with trailing slash.
    if (entry->file_path() == kDir1.AsEndingWithSeparator()) {
      EXPECT_TRUE(entry->is_directory());
    } else if (entry->file_path() == kFile1) {
      EXPECT_FALSE(entry->is_directory());
      EXPECT_EQ(3, entry->original_size());
    } else if (entry->file_path() == kFile2) {
      EXPECT_FALSE(entry->is_directory());
      EXPECT_EQ(kRandomDataSize, entry->original_size());

      const base::FilePath out = dir_.GetPath().AppendASCII("archived_content");
      zip::FilePathWriterDelegate writer(out);
      EXPECT_TRUE(reader.ExtractCurrentEntry(
          &writer, std::numeric_limits<uint64_t>::max()));
      EXPECT_TRUE(base::ContentsEqual(zip_base_dir().Append(kFile2), out));
    } else {
      ADD_FAILURE();
    }
    ASSERT_TRUE(reader.AdvanceToNextEntry());
  }
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
  // root_dir/1/7.txt -> Hello1/7
  // root_dir/2
  // root_dir/2/1.txt -> Hello2/1
  // ...
  // root_dir/2/7.txt -> Hello2/7
  //...
  //...
  // root_dir/10/7.txt -> Hello10/7

  base::FilePath root_dir = zip_base_dir().Append("root_dir");

  // File paths to file content. Used for validation.
  std::map<base::FilePath, std::string> file_tree_content;
  {
    base::ScopedAllowBlockingForTesting allow_io;
    ASSERT_TRUE(base::CreateDirectory(root_dir));

    for (int i = 1; i < 90; i++) {
      base::FilePath file(std::to_string(i) + ".txt");
      std::string content = "Hello" + std::to_string(i);
      ASSERT_TRUE(CreateFile(root_dir.Append(file), content));
      file_tree_content[file] = content;
    }
    for (int i = 1; i <= 10; i++) {
      base::FilePath dir(std::to_string(i));
      ASSERT_TRUE(base::CreateDirectory(root_dir.Append(dir)));
      file_tree_content[dir] = std::string();
      for (int j = 1; j <= 7; j++) {
        base::FilePath file = dir.Append(std::to_string(j) + ".txt");
        std::string content =
            "Hello" + std::to_string(i) + "/" + std::to_string(j);
        ASSERT_TRUE(CreateFile(root_dir.Append(file), content));
        file_tree_content[file] = content;
      }
    }
  }

  // Sanity check on the files created.
  constexpr size_t kEntryCount = 89 /* files under root dir */ +
                                 10 /* 1 to 10 dirs */ +
                                 10 * 7 /* files under 1 to 10 dirs */;
  DCHECK_EQ(kEntryCount, file_tree_content.size());

  bool success = false;
  base::RunLoop run_loop;
  (new ZipFileCreator(
       base::BindOnce(&TestCallback, &success, run_loop.QuitClosure()),
       root_dir,
       std::vector<base::FilePath>(),  // Empty means zip everything in dir.
       zip_archive_path()))
      ->Start(LaunchService());

  run_loop.Run();
  EXPECT_TRUE(success);

  // Check the archive content.
  zip::ZipReader reader;
  ASSERT_TRUE(reader.Open(zip_archive_path()));
  EXPECT_EQ(file_tree_content.size(),
            static_cast<size_t>(reader.num_entries()));
  while (reader.HasMore()) {
    ASSERT_TRUE(reader.OpenCurrentEntryInZip());
    const zip::ZipReader::EntryInfo* entry = reader.current_entry_info();

    base::FilePath path(entry->file_path());
    path = path.StripTrailingSeparators();
    auto iter = file_tree_content.find(path);
    ASSERT_NE(iter, file_tree_content.end())
        << "Path not found in unzipped archive: " << path.value();
    const std::string& expected_content = iter->second;
    if (expected_content.empty()) {
      EXPECT_TRUE(entry->is_directory());
    } else {
      // It's a file.
      EXPECT_FALSE(entry->is_directory());
      std::string actual_content;
      EXPECT_TRUE(reader.ExtractCurrentEntryToString(
          10 * 1024,  // 10KB, any of our test data is less than that.
          &actual_content));
      EXPECT_EQ(expected_content, actual_content);
    }
    file_tree_content.erase(iter);
    ASSERT_TRUE(reader.AdvanceToNextEntry());
  }
  EXPECT_TRUE(file_tree_content.empty());
}
