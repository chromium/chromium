// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/upload_list/combining_upload_list.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/upload_list/text_log_upload_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class CombiningUploadListTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    first_reader_ = base::MakeRefCounted<TextLogUploadList>(first_log_path());
    second_reader_ = base::MakeRefCounted<TextLogUploadList>(second_log_path());
    third_reader_ = base::MakeRefCounted<TextLogUploadList>(third_log_path());
  }

 protected:
  base::FilePath first_log_path() {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("uploads1.log"));
  }

  base::FilePath second_log_path() {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("uploads2.log"));
  }

  base::FilePath third_log_path() {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("uploads3.log"));
  }

  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<UploadList> first_reader_;
  scoped_refptr<UploadList> second_reader_;
  scoped_refptr<UploadList> third_reader_;
};

TEST_F(CombiningUploadListTest, ThreeWayCombine) {
  constexpr char kFirstList[] = R"(
1614000000,ddee0000
1614004000,ddee0004
1614008000,ddee0008
1614012000,ddee0012
  )";
  ASSERT_TRUE(base::WriteFile(first_log_path(), kFirstList));

  constexpr char kSecondList[] = R"(
{"upload_time":"1614002000","upload_id":"ddee0002"}
{"upload_time":"1614006000","upload_id":"ddee0006"}
{"upload_time":"1614010000","upload_id":"ddee0010"}
  )";
  ASSERT_TRUE(base::WriteFile(second_log_path(), kSecondList));

  constexpr char kThirdList[] = R"(
{"upload_time":"1614014000","upload_id":"ddee0014"}
  )";
  ASSERT_TRUE(base::WriteFile(third_log_path(), kThirdList));

  std::vector<scoped_refptr<UploadList>> sublists = {
      first_reader_, second_reader_, third_reader_};
  auto combined_upload_list(
      base::MakeRefCounted<CombiningUploadList>(sublists));

  base::RunLoop run_loop;
  combined_upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  // Expect the reports to be returned newest first.
  const base::Time kExpectedUploadTimes[] = {
      base::Time::FromSecondsSinceUnixEpoch(
          1614014000),  // 14: Largest time value
      base::Time::FromSecondsSinceUnixEpoch(1614012000),  // 12
      base::Time::FromSecondsSinceUnixEpoch(1614010000),  // 10
      base::Time::FromSecondsSinceUnixEpoch(1614008000),  //  8
      base::Time::FromSecondsSinceUnixEpoch(1614006000),  //  6
      base::Time::FromSecondsSinceUnixEpoch(1614004000),  //  4
      base::Time::FromSecondsSinceUnixEpoch(1614002000),  //  2
      base::Time::FromSecondsSinceUnixEpoch(
          1614000000),  //  0: Smallest time value
  };
  // clang-format off
  const std::string kExpectedUploadIds[] = {
      "ddee0014",  // Note that the last two digits correspond to the fourth
      "ddee0012",  // and fifth digits of the time, for easy correspondence.
      "ddee0010",
      "ddee0008",
      "ddee0006",
      "ddee0004",
      "ddee0002",
      "ddee0000",
  };
  // clang-format on

  std::vector<const UploadList::UploadInfo*> actual =
      combined_upload_list->GetUploads(20);
  ASSERT_EQ(actual.size(), std::size(kExpectedUploadTimes));

  for (size_t i = 0; i < std::size(kExpectedUploadTimes); i++) {
    EXPECT_EQ(actual[i]->upload_time, kExpectedUploadTimes[i])
        << " for index " << i;
    EXPECT_EQ(actual[i]->state, UploadList::UploadInfo::State::Uploaded)
        << " for index " << i;
    EXPECT_EQ(actual[i]->upload_id, kExpectedUploadIds[i])
        << " for index " << i;
  }

  static constexpr int kSmallerUploadsSize = 3;
  actual = combined_upload_list->GetUploads(kSmallerUploadsSize);
  ASSERT_EQ(actual.size(),
            std::vector<const UploadList::UploadInfo*>::size_type{
                kSmallerUploadsSize});

  for (size_t i = 0; i < kSmallerUploadsSize; i++) {
    EXPECT_EQ(actual[i]->upload_time, kExpectedUploadTimes[i])
        << " for index " << i;
    EXPECT_EQ(actual[i]->state, UploadList::UploadInfo::State::Uploaded)
        << " for index " << i;
    EXPECT_EQ(actual[i]->upload_id, kExpectedUploadIds[i])
        << " for index " << i;
  }
}

TEST_F(CombiningUploadListTest, SortCaptureTimeOrUploadTime) {
  // If we have both capture_time or upload_time, we sort by capture_time, but
  // we'll use upload_time if that's what we have.
  constexpr char kUploadAndCaptureTimes[] = R"(
{"capture_time":"1614001000","upload_id":"ddee0001","upload_time":"1614999959"}
{"capture_time":"1614004000","upload_id":"ddee0004","upload_time":"1614999999"}
{"capture_time":"1614007000","upload_id":"ddee0007","upload_time":"1600000000"}
  )";
  ASSERT_TRUE(base::WriteFile(first_log_path(), kUploadAndCaptureTimes));
  constexpr char kJustCaptureTimes[] = R"(
{"capture_time":"1614002000","upload_id":"ddee0002"}
{"capture_time":"1614005000","upload_id":"ddee0005"}
{"capture_time":"1614008000","upload_id":"ddee0008"}
  )";
  ASSERT_TRUE(base::WriteFile(second_log_path(), kJustCaptureTimes));
  constexpr char kJustUploadTimes[] = R"(
{"upload_time":"1614003000","upload_id":"ddee0003"}
{"upload_time":"1614006000","upload_id":"ddee0006"}
{"upload_time":"1614009000","upload_id":"ddee0009"}
  )";
  ASSERT_TRUE(base::WriteFile(third_log_path(), kJustUploadTimes));

  std::vector<scoped_refptr<UploadList>> sublists = {
      first_reader_, second_reader_, third_reader_};
  auto combined_upload_list(
      base::MakeRefCounted<CombiningUploadList>(sublists));

  base::RunLoop run_loop;
  combined_upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  const base::Time kExpectedUploadTimes[] = {
      base::Time::FromSecondsSinceUnixEpoch(1614009000),
      base::Time(),  // Sorted by capture time 1614008000
      base::Time::FromSecondsSinceUnixEpoch(
          1600000000),  // Sorted by capture time 1614007000
      base::Time::FromSecondsSinceUnixEpoch(1614006000),
      base::Time(),  // Sorted by capture time 1614005000
      base::Time::FromSecondsSinceUnixEpoch(
          1614999999),  // Sorted by capture time 1614004000
      base::Time::FromSecondsSinceUnixEpoch(1614003000),
      base::Time(),  // Sorted by capture time 1614002000
      base::Time::FromSecondsSinceUnixEpoch(
          1614999959),  // Sorted by capture time 1614001000
  };
  // clang-format off
  const base::Time kExpectedCaptureTimes[] = {
      base::Time(),                         // Sorted by upload_time 1614009000
      base::Time::FromSecondsSinceUnixEpoch(1614008000),
      base::Time::FromSecondsSinceUnixEpoch(1614007000),
      base::Time(),                         // Sorted by upload_time 1614006000
      base::Time::FromSecondsSinceUnixEpoch(1614005000),
      base::Time::FromSecondsSinceUnixEpoch(1614004000),
      base::Time(),                         // Sorted by upload_time 1614003000
      base::Time::FromSecondsSinceUnixEpoch(1614002000),
      base::Time::FromSecondsSinceUnixEpoch(1614001000),
  };
  const std::string kExpectedUploadIds[] = {
      "ddee0009",  // Here, the last digit matches the fourth digit of the time
      "ddee0008",  // we expect to be used as the sort key.
      "ddee0007",
      "ddee0006",
      "ddee0005",
      "ddee0004",
      "ddee0003",
      "ddee0002",
      "ddee0001",
  };
  // clang-format on

  const std::vector<const UploadList::UploadInfo*> actual =
      combined_upload_list->GetUploads(20);
  ASSERT_EQ(actual.size(), std::size(kExpectedUploadTimes));

  for (size_t i = 0; i < std::size(kExpectedUploadTimes); i++) {
    EXPECT_EQ(actual[i]->upload_time, kExpectedUploadTimes[i])
        << " for index " << i;
    EXPECT_EQ(actual[i]->capture_time, kExpectedCaptureTimes[i])
        << " for index " << i;
    EXPECT_EQ(actual[i]->upload_id, kExpectedUploadIds[i])
        << " for index " << i;
  }
}

TEST_F(CombiningUploadListTest, Clear) {
  constexpr char kUploadAndCaptureTimes[] = R"(
{"capture_time":"1614001000","upload_id":"ddee0001","upload_time":"1614999959"}
{"capture_time":"1614004000","upload_id":"ddee0004","upload_time":"1614999999"}
{"capture_time":"1614007000","upload_id":"ddee0007","upload_time":"1600000000"}
)";
  ASSERT_TRUE(base::WriteFile(first_log_path(), kUploadAndCaptureTimes));
  constexpr char kJustCaptureTimes[] = R"(
{"capture_time":"1614002000","upload_id":"ddee0002"}
{"capture_time":"1614005000","upload_id":"ddee0005"}
{"capture_time":"1614008000","upload_id":"ddee0008"}
)";
  ASSERT_TRUE(base::WriteFile(second_log_path(), kJustCaptureTimes));
  constexpr char kJustUploadTimes[] = R"(
{"upload_time":"1614003000","upload_id":"ddee0003"}
{"upload_time":"1614006000","upload_id":"ddee0006"}
{"upload_time":"1614009000","upload_id":"ddee0009"}
)";
  ASSERT_TRUE(base::WriteFile(third_log_path(), kJustUploadTimes));

  std::vector<scoped_refptr<UploadList>> sublists = {
      first_reader_, second_reader_, third_reader_};
  auto combined_upload_list(
      base::MakeRefCounted<CombiningUploadList>(sublists));

  base::RunLoop run_loop;
  combined_upload_list->Clear(base::Time::FromSecondsSinceUnixEpoch(1614004000),
                              base::Time::FromSecondsSinceUnixEpoch(1614006001),
                              run_loop.QuitClosure());
  run_loop.Run();

  // Should have removed the middle entry from each file.
  std::string first_contents;
  base::ReadFileToString(first_log_path(), &first_contents);
  // Can't use raw string here because of the 80 column rule.
  EXPECT_EQ(first_contents,
            "{\"capture_time\":\"1614001000\",\"upload_id\":\"ddee0001\","
            "\"upload_time\":\"1614999959\"}\n"
            "{\"capture_time\":\"1614007000\",\"upload_id\":\"ddee0007\","
            "\"upload_time\":\"1600000000\"}\n");
  std::string second_contents;
  base::ReadFileToString(second_log_path(), &second_contents);
  EXPECT_EQ(second_contents,
            R"({"capture_time":"1614002000","upload_id":"ddee0002"}
{"capture_time":"1614008000","upload_id":"ddee0008"}
)");

  std::string third_contents;
  base::ReadFileToString(third_log_path(), &third_contents);
  EXPECT_EQ(third_contents,
            R"({"upload_time":"1614003000","upload_id":"ddee0003"}
{"upload_time":"1614009000","upload_id":"ddee0009"}
)");
}

class MockUploadList final : public UploadList {
 public:
  MOCK_METHOD0(LoadUploadList, std::vector<std::unique_ptr<UploadInfo>>());
  MOCK_METHOD2(ClearUploadList, void(const base::Time&, const base::Time&));
  MOCK_METHOD1(RequestSingleUpload, void(const std::string&));

 protected:
  ~MockUploadList() override = default;
};

TEST_F(CombiningUploadListTest, RequestSingleUpload) {
  auto mock_list1 = base::MakeRefCounted<MockUploadList>();
  auto mock_list2 = base::MakeRefCounted<MockUploadList>();
  auto combined_lists = base::MakeRefCounted<CombiningUploadList>(
      std::vector<scoped_refptr<UploadList>>({mock_list1, mock_list2}));

  constexpr char kLocalId[] = "12345";
  EXPECT_CALL(*mock_list1, RequestSingleUpload(kLocalId));
  EXPECT_CALL(*mock_list2, RequestSingleUpload(kLocalId));
  combined_lists->RequestSingleUploadAsync(kLocalId);
}

}  // namespace
