// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/media_parser_android.h"

#include <memory>
#include <vector>

#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"
#include "media/base/test_data_util.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct ExtractVideoFrameResult {
  bool success = false;
  chrome::mojom::VideoFrameDataPtr video_frame_data;
  base::Optional<media::VideoDecoderConfig> config;
};

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
// Returns if the first 3 or 4 bytes of H264 encoded |data| is the start code,
// 0x000001 or 0x00000001.
bool HasH264StartCode(const std::vector<uint8_t>& data) {
  if (data.size() < 4 || (data[0] != 0u || data[1] != 0u))
    return false;
  return data[2] == 0x01 || (data[2] == 0u && data[3] == 0x01);
}
#endif

// Returns if the first few bytes in the YUV frame are not all zero. This is a
// rough method to verify the frame is not empty.
bool HasValidYUVData(const scoped_refptr<media::VideoFrame>& frame) {
  bool valid = false;
  for (size_t i = 0; i < 8; ++i) {
    valid |= *(frame->data(media::VideoFrame::kYPlane) + i);
    if (valid)
      break;
  }
  return valid;
}

// Used in test that do blocking reads from a local file.
class TestMediaDataSource : public chrome::mojom::MediaDataSource {
 public:
  TestMediaDataSource(chrome::mojom::MediaDataSourcePtr* interface,
                      const base::FilePath& file_path)
      : file_path_(file_path), binding_(this, mojo::MakeRequest(interface)) {}

  ~TestMediaDataSource() override {}

 private:
  // chrome::mojom::MediaDataSource implementation.
  void Read(int64_t position,
            int64_t length,
            chrome::mojom::MediaDataSource::ReadCallback callback) override {
    base::File file(file_path_, base::File::Flags::FLAG_OPEN |
                                    base::File::Flags::FLAG_READ);
    auto buffer = std::vector<uint8_t>(length);
    int bytes_read = file.Read(position, (char*)(buffer.data()), length);
    if (bytes_read < length)
      buffer.resize(bytes_read);

    std::move(callback).Run(std::vector<uint8_t>(std::move(buffer)));
  }

  base::FilePath file_path_;
  mojo::Binding<chrome::mojom::MediaDataSource> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestMediaDataSource);
};

class MediaParserAndroidTest : public testing::Test {
 public:
  MediaParserAndroidTest() : ref_factory_(base::DoNothing()) {}
  ~MediaParserAndroidTest() override = default;

  void SetUp() override {
    parser_ = std::make_unique<MediaParserAndroid>(ref_factory_.CreateRef());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDown() override { parser_.reset(); }

 protected:
  MediaParserAndroid* parser() { return parser_.get(); }

  const base::FilePath& temp_dir() const { return temp_dir_.GetPath(); }

  ExtractVideoFrameResult ExtractFrame(const base::FilePath& file_path,
                                       const std::string& mime_type) {
    int64_t size = 0;
    EXPECT_TRUE(base::GetFileSize(file_path, &size));

    chrome::mojom::MediaDataSourcePtr data_source_ptr;
    TestMediaDataSource test_data_source(&data_source_ptr, file_path);

    ExtractVideoFrameResult result;
    base::RunLoop run_loop;
    parser()->ExtractVideoFrame(
        mime_type, size, std::move(data_source_ptr),
        base::BindLambdaForTesting(
            [&](bool success, chrome::mojom::VideoFrameDataPtr video_frame_data,
                const base::Optional<media::VideoDecoderConfig>& config) {
              result.success = success;
              result.video_frame_data = std::move(video_frame_data);
              result.config = config;

              run_loop.Quit();
            }));
    run_loop.Run();

    return result;
  }

 private:
  std::unique_ptr<MediaParserAndroid> parser_;
  base::ScopedTempDir temp_dir_;

  service_manager::ServiceContextRefFactory ref_factory_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DISALLOW_COPY_AND_ASSIGN(MediaParserAndroidTest);
};

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
// Test to verify an encoded video frame can be extracted for h264 codec video
// file. Decoding needs to happen in other process.
TEST_F(MediaParserAndroidTest, VideoFrameExtractionH264) {
  auto result =
      ExtractFrame(media::GetTestDataFilePath("bear.mp4"), "video/mp4");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.video_frame_data->which(),
            chrome::mojom::VideoFrameData::Tag::ENCODED_DATA);
  EXPECT_FALSE(result.video_frame_data->get_encoded_data().empty());
  EXPECT_TRUE(HasH264StartCode(result.video_frame_data->get_encoded_data()));
}
#endif

// Test to verify a decoded video frame can be extracted for vp8 codec video
// file with YUV420 color format.
TEST_F(MediaParserAndroidTest, VideoFrameExtractionVp8) {
  auto result = ExtractFrame(media::GetTestDataFilePath("bear-vp8-webvtt.webm"),
                             "video/webm");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.video_frame_data->which(),
            chrome::mojom::VideoFrameData::Tag::DECODED_FRAME);
  const auto& frame = result.video_frame_data->get_decoded_frame();
  EXPECT_TRUE(frame);
  EXPECT_TRUE(HasValidYUVData(frame));
  EXPECT_TRUE(frame->IsMappable());
  EXPECT_FALSE(frame->HasTextures());
  EXPECT_EQ(frame->storage_type(),
            media::VideoFrame::StorageType::STORAGE_MOJO_SHARED_BUFFER);
}

// Test to verify a decoded video frame can be extracted for vp8 codec with
// alpha plane.
TEST_F(MediaParserAndroidTest, VideoFrameExtractionVp8WithAlphaPlane) {
  auto result =
      ExtractFrame(media::GetTestDataFilePath("bear-vp8a.webm"), "video/webm");
  EXPECT_TRUE(result.success);

  EXPECT_EQ(result.video_frame_data->which(),
            chrome::mojom::VideoFrameData::Tag::DECODED_FRAME);
  const auto& frame = result.video_frame_data->get_decoded_frame();
  EXPECT_TRUE(frame);
  EXPECT_TRUE(HasValidYUVData(frame));
  EXPECT_TRUE(frame->IsMappable());
  EXPECT_FALSE(frame->HasTextures());
  EXPECT_EQ(frame->storage_type(),
            media::VideoFrame::StorageType::STORAGE_MOJO_SHARED_BUFFER);
}

// Test to verify frame extraction will fail on invalid video file.
TEST_F(MediaParserAndroidTest, VideoFrameExtractionInvalidFile) {
  base::FilePath dummy_file = temp_dir().AppendASCII("test.txt");
  EXPECT_GT(base::WriteFile(dummy_file, "123", sizeof("123")), 0);

  auto result = ExtractFrame(dummy_file, "video/webm");
  EXPECT_FALSE(result.success);
}

}  // namespace
