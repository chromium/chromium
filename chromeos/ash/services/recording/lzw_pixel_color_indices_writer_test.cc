// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/lzw_pixel_color_indices_writer.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chromeos/ash/services/recording/gif_file_writer.h"
#include "chromeos/ash/services/recording/public/mojom/recording_service.mojom.h"
#include "chromeos/ash/services/recording/recording_file_io_helper.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace recording {

class FakeDelegate : public RecordingFileIoHelper::Delegate {
 public:
  FakeDelegate() = default;
  FakeDelegate(const FakeDelegate&) = delete;
  FakeDelegate& operator=(const FakeDelegate&) = delete;
  ~FakeDelegate() override = default;

  // RecordingFileIoHelper::Delegate:
  void NotifyFailure(mojom::RecordingStatus status) override {}
};

class LzwTest : public testing::Test {
 public:
  LzwTest() {
    const bool dir_created = temp_dir_.CreateUniqueTempDir();
    DCHECK(dir_created);
  }
  LzwTest(const LzwTest&) = delete;
  LzwTest& operator=(const LzwTest&) = delete;
  ~LzwTest() override = default;

  // Returns a path for the given `file_name` under the temp dir created by this
  // fixture.
  base::FilePath GetPathInTempDir(const std::string& file_name) {
    return temp_dir_.GetPath().Append(file_name);
  }

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(LzwTest, VerifyOutputStream) {
  const auto gif_path = GetPathInTempDir("test.gif");
  FakeDelegate delegate;
  GifFileWriter gif_file_writer(
      mojo::PendingRemote<mojom::DriveFsQuotaDelegate>(), gif_path, &delegate);
  LzwPixelColorIndicesWriter lzw_encoder(&gif_file_writer);

  // Let's assume we have the following image with only 3 colors; red, green,
  // and blue.
  //
  // +---+---+---+
  // | R | R | R |
  // +---+---+---+
  // | G | G | G |
  // +---+---+---+
  // | B | B | B |
  // +---+---+---+
  //
  // This means we only have 3 color indices; R => 0, G => 1, B => 2. The bit
  // depth in this case is 2. Let's feed this pixel color indices to the LZW
  // encoder.
  const ColorIndices color_indices{0, 0, 0, 1, 1, 1, 2, 2, 2};
  lzw_encoder.EncodeAndWrite(color_indices, /*color_bit_depth=*/2);

  // Verify that the contents of the file are the expected output of the
  // encoder.
  std::optional<std::vector<uint8_t>> actual_file_contents =
      base::ReadFileToBytes(gif_path);
  ASSERT_TRUE(actual_file_contents.has_value());
  EXPECT_THAT(
      *actual_file_contents,
      testing::ElementsAre(0x02,    // LZW minimum code size.
                           0x04,    // Number of bytes in the data sub-block.
                           0x84,    // -+
                           0x83,    //  +-- LZW-compressed indices.
                           0xA2,    // -+
                           0x54,    // Clear code + EoI code.
                           0x00));  // Block terminator.
}

}  // namespace recording
