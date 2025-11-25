// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/backend_storage.h"

#include <optional>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/persistent_cache/backend.h"
#include "components/persistent_cache/mock/mock_backend.h"
#include "components/persistent_cache/mock/mock_backend_storage_delegate.h"
#include "components/persistent_cache/transaction_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace persistent_cache {

namespace {

using testing::EndsWith;
using testing::Eq;
using testing::Field;
using testing::Optional;
using testing::Property;
using testing::Return;

class BackendStorageTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    auto delegate =
        std::make_unique<testing::StrictMock<MockBackendStorageDelegate>>();
    mock_delegate_ = delegate.get();
    backend_storage_.emplace(std::move(delegate), GetStorageDir());
  }

  base::FilePath GetStorageDir() const {
    return temp_dir_.GetPath().Append(kStorageName);
  }

  MockBackendStorageDelegate& mock_delegate() { return *mock_delegate_; }

  BackendStorage& backend_storage() { return *backend_storage_; }

 private:
  static constexpr base::FilePath::CharType kStorageName[] =
      FILE_PATH_LITERAL("StorageName");
  base::ScopedTempDir temp_dir_;
  std::optional<BackendStorage> backend_storage_;
  raw_ptr<MockBackendStorageDelegate> mock_delegate_ = nullptr;
};

TEST_F(BackendStorageTest, ConstructionWorks) {
  // The instance's directory should be set properly.
  ASSERT_EQ(backend_storage().directory(), GetStorageDir());
  // Its directory should exist.
  ASSERT_PRED1(base::DirectoryExists, GetStorageDir());
}

TEST_F(BackendStorageTest, MakePendingBackendFails) {
  base::FilePath base_name(FILE_PATH_LITERAL("some_base_name"));

  EXPECT_CALL(mock_delegate(),
              MakePendingBackend(GetStorageDir(), base_name, true, true))
      .WillOnce(Return(std::nullopt));
  auto result = backend_storage().MakePendingBackend(
      base_name, /*single_connection=*/true, /*journal_mode_wal=*/true);
  EXPECT_EQ(result, std::nullopt);
}

TEST_F(BackendStorageTest, MakePendingBackendSucceeds) {
  base::FilePath base_name(FILE_PATH_LITERAL("some_base_name"));

  PendingBackend pending_backend;
  pending_backend.read_write = true;
  EXPECT_CALL(mock_delegate(),
              MakePendingBackend(GetStorageDir(), base_name, true, true))
      .WillOnce(Return(std::move(pending_backend)));
  auto result = backend_storage().MakePendingBackend(base_name,
                                                     /*single_connection=*/true,
                                                     /*journal_mode_wal=*/true);
  EXPECT_THAT(result, Optional(Field(&PendingBackend::read_write, Eq(true))));
}

TEST_F(BackendStorageTest, DeleteAllFiles) {
  // Nothing should happen if the directory is empty.
  const auto storage_dir = GetStorageDir();
  ASSERT_PRED1(base::IsDirectoryEmpty, storage_dir);
  backend_storage().DeleteAllFiles();

  // Should still have a directory.
  ASSERT_PRED1(base::DirectoryExists, storage_dir);

  // Add a few files.
  for (int i = 0; i < 5; ++i) {
    std::string ascii = base::StrCat({"ascii", base::NumberToString(i)});
    base::WriteFile(storage_dir.AppendASCII(ascii), ascii);
  }
  ASSERT_FALSE(base::IsDirectoryEmpty(storage_dir));

  // Delete should delete the files.
  backend_storage().DeleteAllFiles();

  // Should still have a directory.
  ASSERT_PRED1(base::DirectoryExists, storage_dir);
  ASSERT_PRED1(base::IsDirectoryEmpty, storage_dir);
}

// No-op if the directory is empty.
TEST_F(BackendStorageTest, BringDownTotalFootprintOfFilesEmpty) {
  auto result = backend_storage().BringDownTotalFootprintOfFiles(1024);
  ASSERT_EQ(result.current_footprint, 0);
  ASSERT_EQ(result.number_of_bytes_deleted, 0);
}

// No-op if the directory is smaller than the threshold.
TEST_F(BackendStorageTest, BringDownTotalFootprintOfFilesBelowThreshold) {
  // The delegate considers ".dat" files to be under its purview, and it will
  // also delete ".txt" files.
  static constexpr base::FilePath::CharType kDatExtension[] =
      FILE_PATH_LITERAL(".dat");
  static constexpr base::FilePath::CharType kTxtExtension[] =
      FILE_PATH_LITERAL(".txt");

  EXPECT_CALL(mock_delegate(), GetBaseName(Property(&base::FilePath::value,
                                                    EndsWith(kDatExtension))))
      .WillRepeatedly([](const base::FilePath& path) {
        return path.BaseName().RemoveFinalExtension();
      });
  EXPECT_CALL(mock_delegate(), GetBaseName(Property(&base::FilePath::value,
                                                    EndsWith(kTxtExtension))))
      .WillRepeatedly(Return(base::FilePath()));

  const auto storage_dir = GetStorageDir();
  static constexpr int64_t kMaxSize = 1024;
  auto kContentBytes = base::byte_span_from_cstring("hi mom");

  // Write five pair of files into the dir.
  int64_t expected_size = 0;

  for (int i = 0; i < 5; ++i) {
    std::string ascii = base::StrCat({"ascii", base::NumberToString(i)});

    auto path = storage_dir.AppendASCII(ascii).AddExtension(kDatExtension);
    base::WriteFile(path, kContentBytes);
    expected_size += kContentBytes.size();

    path = storage_dir.AppendASCII(ascii).AddExtension(kTxtExtension);
    base::WriteFile(path, kContentBytes);
    expected_size += kContentBytes.size();
  }
  // Ensure that the true size is lower than the max so that nothing happens
  // below.
  ASSERT_LT(expected_size, kMaxSize);

  // The delegate will not be asked to delete any files.
  ASSERT_EQ(backend_storage()
                .BringDownTotalFootprintOfFiles(kMaxSize)
                .number_of_bytes_deleted,
            0);
}

// The oldest entries are deleted first when the directory is too big.
TEST_F(BackendStorageTest, BringDownTotalFootprintOfFilesAboveThreshold) {
  // The delegate considers ".dat" files to be under its purview, and it will
  // also delete ".txt" files.
  static constexpr base::FilePath::CharType kDatExtension[] =
      FILE_PATH_LITERAL(".dat");
  static constexpr base::FilePath::CharType kTxtExtension[] =
      FILE_PATH_LITERAL(".txt");

  EXPECT_CALL(mock_delegate(), GetBaseName(Property(&base::FilePath::value,
                                                    EndsWith(kDatExtension))))
      .WillRepeatedly([](const base::FilePath& path) {
        return path.BaseName().RemoveFinalExtension();
      });
  EXPECT_CALL(mock_delegate(), GetBaseName(Property(&base::FilePath::value,
                                                    EndsWith(kTxtExtension))))
      .WillRepeatedly(Return(base::FilePath()));

  const auto storage_dir = GetStorageDir();

  // Write four pair of files into the dir that fill it precisely.
  int64_t expected_size = 0;
  static constexpr int64_t kMaxSize = 1024;
  std::vector<uint8_t> data(kMaxSize / 8, 42);
  for (int i = 0; i < 4; ++i) {
    std::string ascii = base::StrCat({"ascii", base::NumberToString(i)});

    auto path = storage_dir.AppendASCII(ascii).AddExtension(kDatExtension);
    ASSERT_TRUE(base::WriteFile(path, data));
    expected_size += data.size();

    path = storage_dir.AppendASCII(ascii).AddExtension(kTxtExtension);
    ASSERT_TRUE(base::WriteFile(path, data));
    expected_size += data.size();
  }
  ASSERT_EQ(expected_size, kMaxSize);

  // Write two more pair that are older than the rest, and which put the
  // directory over the limit.
  base::Time last_access_time = base::Time::Now();
  base::Time last_modified_time = last_access_time - base::Hours(1);
  for (int i = 0; i < 2; ++i) {
    std::string ascii = base::StrCat({"old", base::NumberToString(i)});

    auto path = storage_dir.AppendASCII(ascii).AddExtension(kDatExtension);
    ASSERT_TRUE(base::WriteFile(path, data));
    expected_size += data.size();
    ASSERT_TRUE(base::TouchFile(path, last_access_time, last_modified_time));

    path = storage_dir.AppendASCII(ascii).AddExtension(kTxtExtension);
    ASSERT_TRUE(base::WriteFile(path, data));
    expected_size += data.size();
    ASSERT_TRUE(base::TouchFile(path, last_access_time, last_modified_time));

    // The delegate should be asked to delete these two files.
    EXPECT_CALL(mock_delegate(),
                DeleteFiles(GetStorageDir(), base::FilePath::FromASCII(ascii)))
        .WillOnce(Return(data.size() * 2));
  }

  // Ensure that the true size is lower than the max so that nothing happens
  // below.
  ASSERT_GT(expected_size, kMaxSize);

  // Delete the four older files.
  auto result = backend_storage().BringDownTotalFootprintOfFiles(kMaxSize);
  ASSERT_EQ(result.current_footprint, kMaxSize);
  ASSERT_EQ(result.number_of_bytes_deleted,
            static_cast<int64_t>(data.size()) * 4);
}

}  // namespace

}  // namespace persistent_cache
