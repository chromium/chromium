// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/write_to_disk.h"

#include <windows.h>

#include <string.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "chrome/installer/mini_installer/memory_range.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mini_installer {

class WriteToDiskTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override { EXPECT_TRUE(temp_dir_.Delete()); }

  const base::FilePath& temp_dir() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

// Tests a simple write of data below the chunk threshold.
TEST_F(WriteToDiskTest, ASmallVictory) {
  static constexpr char kData[] = "data";
  constexpr size_t kDataLength = sizeof(kData) - 1;

  const MemoryRange data = {reinterpret_cast<const uint8_t*>(&kData[0]),
                            kDataLength};
  const base::FilePath data_path = temp_dir().AppendASCII("data");
  ASSERT_PRED2(WriteToDisk, data, data_path.value().c_str());
  std::string data_string;
  ASSERT_PRED2(base::ReadFileToString, data_path, &data_string);
  ASSERT_EQ(data_string, kData);
}

// Tests a simple write of data above the chunk threshold.
TEST_F(WriteToDiskTest, LargeData) {
  constexpr size_t kBlobSize = 32 * 1024 * 1024 + 13;
  std::vector<uint8_t> blob(kBlobSize);
  base::RandBytes(blob);
  const MemoryRange data = {blob.data(), blob.size()};
  const base::FilePath data_path = temp_dir().AppendASCII("data");
  ASSERT_PRED2(WriteToDisk, data, data_path.value().c_str());
  std::string data_string;
  ASSERT_PRED2(base::ReadFileToString, data_path, &data_string);
  EXPECT_EQ(base::as_byte_span(data_string), blob);
}

// Tests that the last error code is set when there's a failure.
TEST_F(WriteToDiskTest, NoDirectory) {
  static constexpr char kData[] = "data";
  constexpr size_t kDataLength = sizeof(kData) - 1;

  const MemoryRange data = {reinterpret_cast<const uint8_t*>(&kData[0]),
                            kDataLength};
  const base::FilePath data_path =
      temp_dir().AppendASCII("notexist").AppendASCII("data");
  ::SetLastError(ERROR_SUCCESS);
  ASSERT_FALSE(WriteToDisk(data, data_path.value().c_str()));
  ASSERT_NE(::GetLastError(), static_cast<DWORD>(ERROR_SUCCESS));
}

}  // namespace mini_installer
