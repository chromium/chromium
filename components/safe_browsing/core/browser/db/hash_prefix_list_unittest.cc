// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/hash_prefix_list.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/core/browser/db/hash_prefix_container.h"
#include "components/safe_browsing/core/browser/db/sb_store_file_format.h"
#include "components/safe_browsing/core/common/proto/v5_store.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

namespace safe_browsing {
namespace {

using ::testing::ElementsAre;

class HashPrefixListTest : public PlatformTest {
 public:
  HashPrefixListTest() { CHECK(temp_dir_.CreateUniqueTempDir()); }

  std::string GetContents(const std::string& extension) {
    std::string contents;
    EXPECT_TRUE(base::ReadFileToString(GetPath(extension), &contents));
    return contents;
  }

  base::FilePath GetPath(const std::string& extension) {
    return GetBasePath().AddExtensionASCII(extension);
  }

  base::FilePath GetBasePath() {
    return temp_dir_.GetPath().AppendASCII("HashPrefixListTest");
  }

  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_env_;
};

TEST_F(HashPrefixListTest, WriteFile) {
  base::HistogramTester histogram_tester;
  HashPrefixList list(GetBasePath(), 4);
  list.Append(4, "fooo");

  V5StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(list.WriteToDisk(sb_file_format));
  EXPECT_EQ(list.IsValid(), V5ApplyUpdateResult::kSuccess);

  histogram_tester.ExpectTotalCount("SafeBrowsing.V5StoreWriteError", 0);
  histogram_tester.ExpectTotalCount("SafeBrowsing.V5StoreFileOpenError", 0);
  histogram_tester.ExpectTotalCount("SafeBrowsing.V5StoreFileWriteError", 0);
  histogram_tester.ExpectTotalCount("SafeBrowsing.SBStoreWriteError", 0);
  histogram_tester.ExpectTotalCount("SafeBrowsing.SBStoreFileOpenError", 0);
  histogram_tester.ExpectTotalCount("SafeBrowsing.SBStoreFileWriteError", 0);

  EXPECT_TRUE(file_format.has_list_details());
  EXPECT_TRUE(file_format.list_details().has_hash_file());
  const auto& hash_file = file_format.list_details().hash_file();
  EXPECT_EQ(GetContents(hash_file.extension()), "fooo");

  HashPrefixMapView view = list.view();
  EXPECT_EQ(view.size(), 1u);
  EXPECT_EQ(view[4], "fooo");
}

TEST_F(HashPrefixListTest, FailedWrite) {
  base::HistogramTester histogram_tester;
  HashPrefixList list(
      GetBasePath().AppendASCII("bad_dir").AppendASCII("some.store"), 4);
  list.Append(4, "foo");

  V5StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_FALSE(list.WriteToDisk(sb_file_format));
  EXPECT_EQ(list.IsValid(), V5ApplyUpdateResult::kMmapFailure);

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5StoreWriteError",
      HashPrefixContainer::WriteError::kFileWriteError, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.V5StoreFileOpenError",
                                      -base::File::FILE_ERROR_NOT_FOUND, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SBStoreWriteError",
      HashPrefixContainer::WriteError::kFileWriteError, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBStoreFileOpenError",
                                      -base::File::FILE_ERROR_NOT_FOUND, 1);
}

TEST_F(HashPrefixListTest, BuffersWrites) {
  HashPrefixList list(GetBasePath(), 4,
                      base::SequencedTaskRunner::GetCurrentDefault(),
                      /*buffer_size=*/4);

  list.Append(4, "fooo");
  EXPECT_EQ(GetContents(list.GetExtensionForTesting()), "");

  list.Append(4, "barr");
  EXPECT_EQ(GetContents(list.GetExtensionForTesting()), "fooo");

  list.Append(4, "somemore");
  EXPECT_EQ(GetContents(list.GetExtensionForTesting()), "fooobarrsomemore");

  list.Append(4, "last");
  EXPECT_EQ(GetContents(list.GetExtensionForTesting()), "fooobarrsomemore");

  V5StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(list.WriteToDisk(sb_file_format));

  EXPECT_TRUE(file_format.has_list_details());
  EXPECT_TRUE(file_format.list_details().has_hash_file());
  const auto& hash_file = file_format.list_details().hash_file();
  EXPECT_EQ(GetContents(hash_file.extension()), "fooobarrsomemorelast");
}

TEST_F(HashPrefixListTest, ReadFile) {
  base::WriteFile(GetPath("foo"), "fooo");

  V5StoreFileFormat file_format;
  auto* hash_file = file_format.mutable_list_details()->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  HashPrefixList list(GetBasePath(), 4);
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_EQ(list.ReadFromDisk(sb_file_format), V5ApplyUpdateResult::kSuccess);
  EXPECT_EQ(list.IsValid(), V5ApplyUpdateResult::kSuccess);

  HashPrefixMapView view = list.view();
  EXPECT_EQ(view.size(), 1u);
  EXPECT_EQ(view[4], "fooo");
}

TEST_F(HashPrefixListTest, ReadFileInvalid) {
  // No file has been created.
  V5StoreFileFormat file_format;
  auto* hash_file = file_format.mutable_list_details()->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  HashPrefixList list(GetBasePath(), 4);
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_EQ(list.ReadFromDisk(sb_file_format),
            V5ApplyUpdateResult::kMmapFailure);
  EXPECT_EQ(list.IsValid(), V5ApplyUpdateResult::kMmapFailure);
}

TEST_F(HashPrefixListTest, ReadFileWrongSize) {
  base::WriteFile(GetPath("foo"), "");

  V5StoreFileFormat file_format;
  auto* hash_file = file_format.mutable_list_details()->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  HashPrefixList list(GetBasePath(), 4);
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_EQ(list.ReadFromDisk(sb_file_format),
            V5ApplyUpdateResult::kMmapFailure);
  EXPECT_EQ(list.IsValid(), V5ApplyUpdateResult::kMmapFailure);
}

TEST_F(HashPrefixListTest, ReadFileInvalidSize) {
  // The file size must be a multiple of the prefix size.
  base::WriteFile(GetPath("foo"), "foo");

  V5StoreFileFormat file_format;
  auto* hash_file = file_format.mutable_list_details()->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(3);

  HashPrefixList list(GetBasePath(), 4);
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_EQ(list.ReadFromDisk(sb_file_format),
            V5ApplyUpdateResult::kFileSizeNotMultipleOfPrefixSize);
}

TEST_F(HashPrefixListTest, WriteAndReadFile) {
  HashPrefixList list(GetBasePath(), 4);
  list.Append(4, "fooo");

  V5StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(list.WriteToDisk(sb_file_format));
  EXPECT_EQ(list.IsValid(), V5ApplyUpdateResult::kSuccess);

  HashPrefixList list_read(GetBasePath(), 4);
  SBStoreFileFormat sb_file_format_read(&file_format);
  EXPECT_EQ(list_read.ReadFromDisk(sb_file_format_read),
            V5ApplyUpdateResult::kSuccess);
  EXPECT_EQ(list_read.IsValid(), V5ApplyUpdateResult::kSuccess);

  HashPrefixMapView view = list_read.view();
  EXPECT_EQ(view.size(), 1u);
  EXPECT_EQ(view[4], "fooo");
}

TEST_F(HashPrefixListTest, ClearingListBeforeWriteDeletesFile) {
  HashPrefixList list(GetBasePath(), 4,
                      base::SequencedTaskRunner::GetCurrentDefault(),
                      /*buffer_size=*/1);
  list.Append(4, "foo");

  std::string extension = list.GetExtensionForTesting();
  EXPECT_EQ(GetContents(extension), "foo");

  EXPECT_TRUE(base::PathExists(GetPath(extension)));
  list.Clear();
  EXPECT_FALSE(base::PathExists(GetPath(extension)));
}

TEST_F(HashPrefixListTest, ClearListOnSeparateThread) {
  scoped_refptr<base::SequencedTaskRunner> db_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});

  std::unique_ptr<HashPrefixList, base::OnTaskRunnerDeleter> list(
      nullptr, base::OnTaskRunnerDeleter(db_task_runner));
  std::string extension;

  // Create list and append to it on the DB thread.
  base::RunLoop run_loop_init;
  db_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](HashPrefixListTest* test,
             std::unique_ptr<HashPrefixList, base::OnTaskRunnerDeleter>*
                 list_out,
             std::string* extension_out,
             scoped_refptr<base::SequencedTaskRunner> db_task_runner,
             base::OnceClosure quit_closure) {
            // Create the list on the DB thread.
            *list_out =
                std::unique_ptr<HashPrefixList, base::OnTaskRunnerDeleter>(
                    new HashPrefixList(test->GetBasePath(), 4, db_task_runner,
                                       /*buffer_size=*/1),
                    base::OnTaskRunnerDeleter(db_task_runner));
            // Append to it.
            list_out->get()->Append(4, "foo");
            // Validate file was created with appropriate content.
            *extension_out = list_out->get()->GetExtensionForTesting();
            EXPECT_EQ(test->GetContents(*extension_out), "foo");
            EXPECT_TRUE(base::PathExists(test->GetPath(*extension_out)));
            std::move(quit_closure).Run();
          },
          base::Unretained(this), &list, &extension, db_task_runner,
          run_loop_init.QuitClosure()));
  run_loop_init.Run();

  // Call `Clear` from a non-DB thread, which should post a task for the DB
  // thread.
  list->Clear();

  // Wait for `Clear` to complete on the DB thread.
  {
    base::RunLoop run_loop;
    db_task_runner->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                     run_loop.QuitClosure());
    run_loop.Run();
  }

  // Verify file was deleted.
  base::RunLoop run_loop_cleanup;
  db_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](HashPrefixListTest* test, const std::string& ext,
             base::OnceClosure quit_closure) {
            // Check file was deleted.
            EXPECT_FALSE(base::PathExists(test->GetPath(ext)));
            std::move(quit_closure).Run();
          },
          base::Unretained(this), extension, run_loop_cleanup.QuitClosure()));
  run_loop_cleanup.Run();

  // For test cleanup, trigger delete and wait for it to complete on the DB
  // thread.
  list.reset();
  {
    base::RunLoop run_loop;
    db_task_runner->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                     run_loop.QuitClosure());
    run_loop.Run();
  }
}

TEST_F(HashPrefixListTest, GetMatchingHashPrefix) {
  HashPrefixList list(GetBasePath(), 4);

  std::string s = "abcd";
  list.Append(4, s);

  V5StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(list.WriteToDisk(sb_file_format));
  EXPECT_EQ(list.IsValid(), V5ApplyUpdateResult::kSuccess);

  EXPECT_TRUE(file_format.has_list_details());
  EXPECT_TRUE(file_format.list_details().has_hash_file());

  EXPECT_EQ(list.GetMatchingHashPrefix(s), s);
  EXPECT_EQ(list.GetMatchingHashPrefix(s + "foobar"), s);
  EXPECT_EQ(list.GetMatchingHashPrefix("blah"), "");
}

TEST_F(HashPrefixListTest, ValidAfterWrite) {
  HashPrefixList hash_prefix_list(GetBasePath(), 4);
  hash_prefix_list.Append(4, "fooo");

  V5StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  ASSERT_TRUE(hash_prefix_list.WriteToDisk(sb_file_format));

  HashPrefixMapView view = hash_prefix_list.view();
  EXPECT_EQ(view.size(), 1u);
  EXPECT_EQ(view[4], "fooo");
}

TEST_F(HashPrefixListTest, ExtensionFormat) {
  HashPrefixList list(GetBasePath(), 4);
  list.Append(4, "fooo");

  V5StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(list.WriteToDisk(sb_file_format));
  EXPECT_EQ(list.IsValid(), V5ApplyUpdateResult::kSuccess);

  EXPECT_TRUE(file_format.has_list_details());
  EXPECT_TRUE(file_format.list_details().has_hash_file());
  const std::string& extension =
      file_format.list_details().hash_file().extension();

  uint64_t microsecond_timestamp;
  EXPECT_TRUE(base::StringToUint64(extension, &microsecond_timestamp));
}

TEST_F(HashPrefixListTest, GetPrefixInfo) {
  HashPrefixList list(GetBasePath(), 4);

  google::protobuf::RepeatedPtrField<
      DatabaseManagerInfo::DatabaseInfo::StoreInfo::PrefixSet>
      prefix_sets;

  list.GetPrefixInfo(&prefix_sets);
  EXPECT_EQ(prefix_sets.size(), 0);

  list.Append(4, "fooo");
  V5StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(list.WriteToDisk(sb_file_format));

  list.GetPrefixInfo(&prefix_sets);
  ASSERT_EQ(prefix_sets.size(), 1);
  EXPECT_EQ(prefix_sets[0].size(), 4u);
  EXPECT_EQ(prefix_sets[0].count(), 1u);
}

TEST_F(HashPrefixListTest, FileInfoFileNotFound) {
  base::HistogramTester histogram_tester;
  HashPrefixContainer::FileInfo file_info(GetBasePath(), 4);
  auto* writer = file_info.GetOrCreateWriter(4, "ext", "V5Store");
  writer->Write("fooo");
  std::optional<HashPrefixContainer::FinalizedFileInfo> finalized_info =
      file_info.Finalize("V5Store");
  ASSERT_TRUE(finalized_info.has_value());

  base::FilePath path =
      HashPrefixContainer::GetPath(GetBasePath(), finalized_info->extension);
  ASSERT_TRUE(base::PathExists(path));
  ASSERT_TRUE(base::DeleteFile(path));

  EXPECT_FALSE(
      file_info.Initialize(finalized_info->extension, finalized_info->file_size,
                           /*initialize_after_write=*/true, "V5Store"));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5StoreWriteError",
      HashPrefixContainer::WriteError::kFileNotFound, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SBStoreWriteError",
      HashPrefixContainer::WriteError::kFileNotFound, 1);
}

TEST_F(HashPrefixListTest, FileInfoFileSizeMismatch) {
  base::HistogramTester histogram_tester;
  HashPrefixContainer::FileInfo file_info(GetBasePath(), 4);
  auto* writer = file_info.GetOrCreateWriter(4, "ext", "V5Store");
  writer->Write("fooo");
  std::optional<HashPrefixContainer::FinalizedFileInfo> finalized_info =
      file_info.Finalize("V5Store");
  ASSERT_TRUE(finalized_info.has_value());

  base::FilePath path =
      HashPrefixContainer::GetPath(GetBasePath(), finalized_info->extension);
  ASSERT_TRUE(base::AppendToFile(path, "bar"));

  EXPECT_FALSE(
      file_info.Initialize(finalized_info->extension, finalized_info->file_size,
                           /*initialize_after_write=*/true, "V5Store"));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5StoreWriteError",
      HashPrefixContainer::WriteError::kFileSizeMismatch, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SBStoreWriteError",
      HashPrefixContainer::WriteError::kFileSizeMismatch, 1);
}

TEST_F(HashPrefixListTest, WriteInvalidTotalSize) {
  base::HistogramTester histogram_tester;
  HashPrefixList list(GetBasePath(), 4);
  list.Append(4, "foo");  // Size 3, prefix size 4.

  V5StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_FALSE(list.WriteToDisk(sb_file_format));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5StoreWriteError",
      HashPrefixContainer::WriteError::kInvalidTotalSize, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SBStoreWriteError",
      HashPrefixContainer::WriteError::kInvalidTotalSize, 1);
}

TEST_F(HashPrefixListTest, ReadFileEmptyExtension) {
  V5StoreFileFormat file_format;
  auto* hash_file = file_format.mutable_list_details()->mutable_hash_file();
  hash_file->set_extension("");
  hash_file->set_file_size(4);

  HashPrefixList list(GetBasePath(), 4);
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_EQ(list.ReadFromDisk(sb_file_format),
            V5ApplyUpdateResult::kMmapFailure);
  EXPECT_EQ(list.IsValid(), V5ApplyUpdateResult::kMmapFailure);
}

TEST_F(HashPrefixListTest, ReadFileEmptyExtensionFileExists) {
  // Create a file at the base path (no extension)
  base::WriteFile(GetBasePath(), "fooo");

  V5StoreFileFormat file_format;
  auto* hash_file = file_format.mutable_list_details()->mutable_hash_file();
  hash_file->set_extension("");
  hash_file->set_file_size(4);

  HashPrefixList list(GetBasePath(), 4);
  SBStoreFileFormat sb_file_format(&file_format);
  // It should fail even if the file exists because we don't allow empty
  // extension.
  EXPECT_EQ(list.ReadFromDisk(sb_file_format),
            V5ApplyUpdateResult::kMmapFailure);
  EXPECT_EQ(list.IsValid(), V5ApplyUpdateResult::kMmapFailure);

  HashPrefixMapView view = list.view();
  EXPECT_TRUE(view.empty());

  // Cleanup
  base::DeleteFile(GetBasePath());
}

TEST_F(HashPrefixListTest, GetPaths) {
  HashPrefixList list(GetBasePath(), 4);
  // Before any prefixes are added, GetPaths() should be empty.
  EXPECT_TRUE(list.GetPaths().empty());

  list.Append(4, "fooo");
  // Before WriteToDisk(), the file info is created for writing but not
  // readable.
  EXPECT_TRUE(list.GetPaths().empty());

  V5StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(list.WriteToDisk(sb_file_format));

  // After WriteToDisk(), the file is readable.
  EXPECT_THAT(
      list.GetPaths(),
      ElementsAre(GetPath(file_format.list_details().hash_file().extension())));
}

}  // namespace
}  // namespace safe_browsing
