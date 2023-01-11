// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/minidump_uploader.h"

#include <memory>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/test/scoped_path_override.h"
#include "base/time/time.h"
#include "chromecast/base/cast_sys_info_dummy.h"
#include "chromecast/base/pref_names.h"
#include "chromecast/crash/cast_crashdump_uploader.h"
#include "chromecast/crash/linux/crash_testing_utils.h"
#include "chromecast/public/cast_sys_info.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {

const char kLockfileName[] = "lockfile";
const char kMetadataName[] = "metadata";
const char kMinidumpSubdir[] = "minidumps";
const char kVirtualChannel[] = "virtual-channel";
const char kVirtualChannelName[] = "a-virtual-chanel";

typedef std::vector<std::unique_ptr<DumpInfo>> DumpList;

std::unique_ptr<PrefService> CreateFakePrefService(bool opt_in) {
  std::unique_ptr<TestingPrefServiceSimple> retval(
      new TestingPrefServiceSimple);
  retval->registry()->RegisterBooleanPref(prefs::kOptInStats, opt_in);
  retval->registry()->RegisterStringPref(::metrics::prefs::kMetricsClientID,
                                         "");
  retval->registry()->RegisterStringPref(kVirtualChannel, kVirtualChannelName);
  return std::move(retval);
}

bool DumpsAreEqual(const DumpInfo& l, const DumpInfo& r) {
  return l.crashed_process_dump() == r.crashed_process_dump() &&
         l.logfile() == r.logfile();
}

class MockCastCrashdumpUploader : public CastCrashdumpUploader {
 public:
  explicit MockCastCrashdumpUploader(const CastCrashdumpData& data)
      : CastCrashdumpUploader(data) {}

  MOCK_METHOD2(AddAttachment,
               bool(const std::string& label, const std::string& filename));
  MOCK_METHOD2(SetParameter,
               void(const std::string& key, const std::string& value));
  MOCK_METHOD1(Upload, bool(std::string* response));
};

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;
using ::testing::StrictMock;

class MinidumpUploaderTest : public testing::Test {
 public:
  MinidumpUploaderTest() {}
  ~MinidumpUploaderTest() override {}

 protected:
  void SetUp() override {
    // Set up a temporary directory which will be used as our fake home dir.
    ASSERT_TRUE(fake_home_dir_.CreateUniqueTempDir());
    path_override_.reset(
        new base::ScopedPathOverride(base::DIR_HOME, fake_home_dir_.GetPath()));

    minidump_dir_ = fake_home_dir_.GetPath().Append(kMinidumpSubdir);
    lockfile_ = minidump_dir_.Append(kLockfileName);
    metadata_ = minidump_dir_.Append(kMetadataName);

    // Create minidump directory.
    ASSERT_TRUE(base::CreateDirectory(minidump_dir_));

    CastCrashdumpData data;
    mock_crash_uploader_.reset(new StrictMock<MockCastCrashdumpUploader>(data));
  }

  std::unique_ptr<DumpInfo> GenerateDumpWithFiles(
      const base::FilePath& minidump_path,
      const base::FilePath& logfile_path,
      const std::vector<std::string>* attachments = nullptr) {
    // Must pass in non-empty MinidumpParams to circumvent the internal checks.
    std::unique_ptr<DumpInfo> dump(new DumpInfo(
        minidump_path.value(), logfile_path.value(), base::Time::Now(),
        MinidumpParams(0, "_", "_", "_", "_", "_", "_", "_", "_"),
        attachments));

    CHECK(AppendLockFile(lockfile_.value(), metadata_.value(), *dump));
    base::File minidump(
        minidump_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    base::File logfile(logfile_path,
                       base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    CHECK(minidump.IsValid());
    CHECK(logfile.IsValid());

    if (attachments) {
      for (const auto& attachment : *attachments) {
        base::File attachment_file(
            base::FilePath(attachment),
            base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
        CHECK(attachment_file.IsValid());
      }
    }

    return dump;
  }

  MockCastCrashdumpUploader& mock_crash_uploader() {
    return *mock_crash_uploader_;
  }

  CastSysInfoDummy& sys_info_dummy() { return sys_info_dummy_; }

  base::FilePath minidump_dir_;  // Path to the minidump directory.
  base::FilePath lockfile_;      // Path to the lockfile in |minidump_dir_|.
  base::FilePath metadata_;      // Path to the metadata in |minidump_dir_|.

 private:
  base::ScopedTempDir fake_home_dir_;
  std::unique_ptr<base::ScopedPathOverride> path_override_;

  CastSysInfoDummy sys_info_dummy_;
  std::unique_ptr<StrictMock<MockCastCrashdumpUploader>> mock_crash_uploader_;
};

TEST_F(MinidumpUploaderTest, AvoidsLockingWithoutDumps) {
  class LockingTest : public SynchronizedMinidumpManager {
   public:
    explicit LockingTest(MinidumpUploader* minidump_uploader)
        : minidump_uploader_(minidump_uploader) {}
    ~LockingTest() override = default;

    bool Run() { return AcquireLockAndDoWork(); }

    // SynchronizedMinidumpManager implementation:
    bool DoWork() override {
      // This should fail if it attempts to get the lock.
      return minidump_uploader_->UploadAllMinidumps();
    }

   private:
    MinidumpUploader* const minidump_uploader_;
  };
  MinidumpUploader uploader(&sys_info_dummy(), "", &mock_crash_uploader(),
                            base::BindRepeating(&CreateFakePrefService, true));
  // Will lock for the first run to initialize file state.
  ASSERT_TRUE(uploader.UploadAllMinidumps());

  LockingTest lt(&uploader);
  EXPECT_TRUE(lt.Run());
}

TEST_F(MinidumpUploaderTest, RemovesDumpsWithoutOptIn) {
  const base::FilePath& minidump_path = minidump_dir_.Append("ayy");
  const base::FilePath& logfile_path = minidump_dir_.Append("lmao");

  // Write a dump info entry.
  GenerateDumpWithFiles(minidump_path, logfile_path);
  MinidumpUploader uploader(&sys_info_dummy(), "", &mock_crash_uploader(),
                            base::BindRepeating(&CreateFakePrefService, false));

  // MinidumpUploader should not call upon CastCrashdumpUploader.
  ASSERT_TRUE(uploader.UploadAllMinidumps());

  // Ensure dump files were deleted, lockfile was emptied.
  ASSERT_FALSE(base::PathExists(minidump_path));
  ASSERT_FALSE(base::PathExists(logfile_path));

  int64_t size = -1;
  ASSERT_TRUE(base::GetFileSize(lockfile_, &size));
  ASSERT_EQ(size, 0);
}

TEST_F(MinidumpUploaderTest, SavesDumpInfoWithUploadFailure) {
  const base::FilePath& minidump_path = minidump_dir_.Append("ayy");
  const base::FilePath& logfile_path = minidump_dir_.Append("lmao");

  // Write one entry with appropriate files.
  std::unique_ptr<DumpInfo> dump(
      GenerateDumpWithFiles(minidump_path, logfile_path));
  MinidumpUploader uploader(&sys_info_dummy(), "", &mock_crash_uploader(),
                            base::BindRepeating(&CreateFakePrefService, true));

  // Induce an upload failure.
  EXPECT_CALL(mock_crash_uploader(),
              AddAttachment("log_file", logfile_path.value()))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_crash_uploader(), SetParameter(_, _)).Times(AtLeast(0));
  EXPECT_CALL(mock_crash_uploader(), Upload(_)).WillOnce(Return(false));
  ASSERT_TRUE(uploader.UploadAllMinidumps());

  // Ensure dump files were preserved, lockfile was not emptied.
  ASSERT_TRUE(base::PathExists(minidump_path));
  ASSERT_TRUE(base::PathExists(logfile_path));

  DumpList dumps;
  ASSERT_TRUE(FetchDumps(lockfile_.value(), &dumps));
  ASSERT_TRUE(DumpsAreEqual(*dump, *dumps.front()));
}

TEST_F(MinidumpUploaderTest, SavesRemainingDumpInfoWithMidwayUploadFailure) {
  const base::FilePath& minidump_path = minidump_dir_.Append("ayy");
  const base::FilePath& logfile_path = minidump_dir_.Append("lmao");
  const base::FilePath& minidump_path2 = minidump_dir_.Append("ayy2");
  const base::FilePath& logfile_path2 = minidump_dir_.Append("lmao2");

  // Write two entries, each with their own files.
  GenerateDumpWithFiles(minidump_path, logfile_path);
  std::unique_ptr<DumpInfo> dump2(
      GenerateDumpWithFiles(minidump_path2, logfile_path2));
  {
    MinidumpUploader uploader(
        &sys_info_dummy(), "", &mock_crash_uploader(),
        base::BindRepeating(&CreateFakePrefService, true));

    // First allow a successful upload, then induce failure.
    EXPECT_CALL(mock_crash_uploader(),
                AddAttachment("log_file", logfile_path.value()))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_crash_uploader(),
                AddAttachment("log_file", logfile_path2.value()))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_crash_uploader(), SetParameter(_, _)).Times(AtLeast(0));
    EXPECT_CALL(mock_crash_uploader(), Upload(_))
        .WillOnce(Return(true))
        .WillOnce(Return(false));
    ASSERT_TRUE(uploader.UploadAllMinidumps());
  }

  // Info should exist in the lockfile, but should only be non-uploaded dump.
  DumpList dumps;
  ASSERT_TRUE(FetchDumps(lockfile_.value(), &dumps));
  ASSERT_TRUE(DumpsAreEqual(*dump2, *dumps.front()));

  // Ensure uploaded files are gone, non-uploaded files remain.
  ASSERT_FALSE(base::PathExists(minidump_path));
  ASSERT_FALSE(base::PathExists(logfile_path));
  ASSERT_TRUE(base::PathExists(minidump_path2));
  ASSERT_TRUE(base::PathExists(logfile_path2));

  {
    MinidumpUploader uploader(
        &sys_info_dummy(), "", &mock_crash_uploader(),
        base::BindRepeating(&CreateFakePrefService, true));

    // Finally, upload successfully.
    EXPECT_CALL(mock_crash_uploader(),
                AddAttachment("log_file", logfile_path2.value()))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_crash_uploader(), SetParameter(_, _)).Times(AtLeast(0));
    EXPECT_CALL(mock_crash_uploader(), Upload(_)).WillOnce(Return(true));
    ASSERT_TRUE(uploader.UploadAllMinidumps());
  }

  // Ensure all dump files have been removed, lockfile has been emptied.
  int64_t size = -1;
  ASSERT_TRUE(base::GetFileSize(lockfile_, &size));
  ASSERT_EQ(size, 0);

  ASSERT_TRUE(base::DeleteFile(lockfile_));
  ASSERT_TRUE(base::DeleteFile(metadata_));
  ASSERT_TRUE(base::IsDirectoryEmpty(minidump_dir_));
}

TEST_F(MinidumpUploaderTest, FailsUploadWithMissingMinidumpFile) {
  const base::FilePath& minidump_path = minidump_dir_.Append("ayy");
  const base::FilePath& logfile_path = minidump_dir_.Append("lmao");

  // Write one entry with appropriate files.
  GenerateDumpWithFiles(minidump_path, logfile_path);
  MinidumpUploader uploader(&sys_info_dummy(), "", &mock_crash_uploader(),
                            base::BindRepeating(&CreateFakePrefService, true));

  // No CastCrashdumpUploader methods should be called.
  ASSERT_TRUE(base::DeleteFile(minidump_path));
  ASSERT_TRUE(uploader.UploadAllMinidumps());

  // Ensure dump files were deleted, lockfile was emptied.
  ASSERT_FALSE(base::PathExists(minidump_path));
  ASSERT_FALSE(base::PathExists(logfile_path));

  int64_t size = -1;
  ASSERT_TRUE(base::GetFileSize(lockfile_, &size));
  ASSERT_EQ(size, 0);
}

TEST_F(MinidumpUploaderTest, UploadsWithoutMissingLogFile) {
  const base::FilePath& minidump_path = minidump_dir_.Append("ayy");
  const base::FilePath& logfile_path = minidump_dir_.Append("lmao");

  // Write one entry with appropriate files.
  GenerateDumpWithFiles(minidump_path, logfile_path);
  MinidumpUploader uploader(&sys_info_dummy(), "", &mock_crash_uploader(),
                            base::BindRepeating(&CreateFakePrefService, true));

  // Delete logfile, crash uploader should still work as intended.
  ASSERT_TRUE(base::DeleteFile(logfile_path));
  EXPECT_CALL(mock_crash_uploader(), SetParameter(_, _)).Times(AtLeast(0));
  EXPECT_CALL(mock_crash_uploader(), Upload(_)).WillOnce(Return(true));
  ASSERT_TRUE(uploader.UploadAllMinidumps());

  // Ensure dump files were deleted, lockfile was emptied.
  ASSERT_FALSE(base::PathExists(minidump_path));
  ASSERT_FALSE(base::PathExists(logfile_path));

  int64_t size = -1;
  ASSERT_TRUE(base::GetFileSize(lockfile_, &size));
  ASSERT_EQ(size, 0);
}

TEST_F(MinidumpUploaderTest, UploadsWithMultipleAttachments) {
  const base::FilePath& minidump_path = minidump_dir_.Append("ayy");
  const base::FilePath& logfile_path = minidump_dir_.Append("lmao");
  std::vector<std::string> attachments = {
      minidump_dir_.Append("attachment-01").value(), "/tmp/attachment-02"};

  // Write one entry with appropriate files.
  GenerateDumpWithFiles(minidump_path, logfile_path, &attachments);
  MinidumpUploader uploader(&sys_info_dummy(), "", &mock_crash_uploader(),
                            base::BindRepeating(&CreateFakePrefService, true));

  // Allow a successful upload.
  ASSERT_TRUE(base::DeleteFile(logfile_path));
  EXPECT_CALL(mock_crash_uploader(),
              AddAttachment("attachment_0", attachments[0]))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_crash_uploader(),
              AddAttachment("attachment_1", attachments[1]))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_crash_uploader(), SetParameter(_, _)).Times(AtLeast(0));
  EXPECT_CALL(mock_crash_uploader(), Upload(_)).WillOnce(Return(true));
  ASSERT_TRUE(uploader.UploadAllMinidumps());

  // Ensure dump files were deleted, lockfile was emptied.
  ASSERT_FALSE(base::PathExists(minidump_path));
  ASSERT_FALSE(base::PathExists(logfile_path));
  ASSERT_FALSE(base::PathExists(base::FilePath(attachments[0])));
  ASSERT_TRUE(base::PathExists(base::FilePath(attachments[1])));

  int64_t size = -1;
  ASSERT_TRUE(base::GetFileSize(lockfile_, &size));
  ASSERT_EQ(size, 0);
}

TEST_F(MinidumpUploaderTest, DeletesLingeringFiles) {
  const base::FilePath& minidump_path = minidump_dir_.Append("ayy");
  const base::FilePath& logfile_path = minidump_dir_.Append("lmao");
  const base::FilePath& temp1 = minidump_dir_.Append("chrome");
  const base::FilePath& temp2 = minidump_dir_.Append("cast");

  // Create "fake" lingering files in minidump directory.
  base::File generator(temp1,
                       base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  generator.Close();
  generator.Initialize(temp2,
                       base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  generator.Close();
  ASSERT_TRUE(base::PathExists(temp1));
  ASSERT_TRUE(base::PathExists(temp2));

  // Write a real entry.
  GenerateDumpWithFiles(minidump_path, logfile_path);
  MinidumpUploader uploader(&sys_info_dummy(), "", &mock_crash_uploader(),
                            base::BindRepeating(&CreateFakePrefService, true));

  EXPECT_CALL(mock_crash_uploader(),
              AddAttachment("log_file", logfile_path.value()))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_crash_uploader(), SetParameter(_, _)).Times(AtLeast(0));
  EXPECT_CALL(mock_crash_uploader(), Upload(_)).WillOnce(Return(true));
  ASSERT_TRUE(uploader.UploadAllMinidumps());

  // Ensure dump/lingering files were deleted, lockfile was emptied.
  ASSERT_FALSE(base::PathExists(minidump_path));
  ASSERT_FALSE(base::PathExists(logfile_path));
  ASSERT_FALSE(base::PathExists(temp1));
  ASSERT_FALSE(base::PathExists(temp2));

  int64_t size = -1;
  ASSERT_TRUE(base::GetFileSize(lockfile_, &size));
  ASSERT_EQ(size, 0);
}

TEST_F(MinidumpUploaderTest, SchedulesRebootWhenRatelimited) {
  const base::FilePath& minidump_path = minidump_dir_.Append("ayy");
  const base::FilePath& logfile_path = minidump_dir_.Append("lmao");

  MinidumpUploader uploader(&sys_info_dummy(), "", &mock_crash_uploader(),
                            base::BindRepeating(&CreateFakePrefService, true));
  // Generate max dumps.
  for (int i = 0; i < SynchronizedMinidumpManager::kRatelimitPeriodMaxDumps + 1;
       i++)
    GenerateDumpWithFiles(minidump_path, logfile_path);

  // MinidumpUploader should call CastCrashdumpUploader once (other |max| dumps
  // files do not exist). Reboot should be scheduled, as this is first
  // ratelimit.
  EXPECT_CALL(mock_crash_uploader(),
              AddAttachment("log_file", logfile_path.value()))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(mock_crash_uploader(), SetParameter(_, _)).Times(AtLeast(0));
  EXPECT_CALL(mock_crash_uploader(), Upload(_))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  ASSERT_TRUE(uploader.UploadAllMinidumps());
  ASSERT_TRUE(uploader.reboot_scheduled());

  // Ensure dump files were deleted, lockfile was emptied.
  ASSERT_FALSE(base::PathExists(minidump_path));
  ASSERT_FALSE(base::PathExists(logfile_path));

  int64_t size = -1;
  ASSERT_TRUE(base::GetFileSize(lockfile_, &size));
  ASSERT_EQ(size, 0);

  // Generate one dump for a second pass.
  GenerateDumpWithFiles(minidump_path, logfile_path);
  MinidumpUploader uploader2(&sys_info_dummy(), "", &mock_crash_uploader(),
                             base::BindRepeating(&CreateFakePrefService, true));

  // Since a reboot was scheduled, the rate limit was cleared.  New uploads
  // should be scheduled.
  EXPECT_CALL(mock_crash_uploader(),
              AddAttachment("log_file", logfile_path.value()))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_crash_uploader(), Upload(_)).WillOnce(Return(true));
  ASSERT_TRUE(uploader2.UploadAllMinidumps());
  ASSERT_FALSE(uploader2.reboot_scheduled());

  // Ensure dump files were deleted, lockfile was emptied.
  ASSERT_FALSE(base::PathExists(minidump_path));
  ASSERT_FALSE(base::PathExists(logfile_path));

  ASSERT_TRUE(base::GetFileSize(lockfile_, &size));
  ASSERT_EQ(size, 0);
}

TEST_F(MinidumpUploaderTest, UploadInitializesFileState) {
  MinidumpUploader uploader(&sys_info_dummy(), "", &mock_crash_uploader(),
                            base::BindRepeating(&CreateFakePrefService, true));
  ASSERT_TRUE(base::IsDirectoryEmpty(minidump_dir_));
  ASSERT_TRUE(uploader.UploadAllMinidumps());
  base::File lockfile(lockfile_, base::File::FLAG_OPEN | base::File::FLAG_READ);
  EXPECT_TRUE(lockfile.IsValid());
  base::File metadata(lockfile_, base::File::FLAG_OPEN | base::File::FLAG_READ);
  EXPECT_TRUE(metadata.IsValid());
}

}  // namespace
}  // namespace chromecast
