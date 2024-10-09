// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/ios/ios_image_decoder_impl.h"

#import <UIKit/UIKit.h>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

namespace {

static unsigned char kJPGImage[] = {
    255, 216, 255, 224, 0,   16,  74,  70, 73, 70, 0,   1,   1,   1,   0,
    72,  0,   72,  0,   0,   255, 254, 0,  19, 67, 114, 101, 97,  116, 101,
    100, 32,  119, 105, 116, 104, 32,  71, 73, 77, 80,  255, 219, 0,   67,
    0,   5,   3,   4,   4,   4,   3,   5,  4,  4,  4,   5,   5,   5,   6,
    7,   12,  8,   7,   7,   7,   7,   15, 11, 11, 9,   12,  17,  15,  18,
    18,  17,  15,  17,  17,  19,  22,  28, 23, 19, 20,  26,  21,  17,  17,
    24,  33,  24,  26,  29,  29,  31,  31, 31, 19, 23,  34,  36,  34,  30,
    36,  28,  30,  31,  30,  255, 219, 0,  67, 1,  5,   5,   5,   7,   6,
    7,   14,  8,   8,   14,  30,  20,  17, 20, 30, 30,  30,  30,  30,  30,
    30,  30,  30,  30,  30,  30,  30,  30, 30, 30, 30,  30,  30,  30,  30,
    30,  30,  30,  30,  30,  30,  30,  30, 30, 30, 30,  30,  30,  30,  30,
    30,  30,  30,  30,  30,  30,  30,  30, 30, 30, 30,  30,  30,  30,  255,
    192, 0,   17,  8,   0,   1,   0,   1,  3,  1,  34,  0,   2,   17,  1,
    3,   17,  1,   255, 196, 0,   21,  0,  1,  1,  0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,  0,  0,  8,   255, 196, 0,   20,
    16,  1,   0,   0,   0,   0,   0,   0,  0,  0,  0,   0,   0,   0,   0,
    0,   0,   0,   255, 196, 0,   20,  1,  1,  0,  0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,  0,  0,  255, 196, 0,   20,  17,
    1,   0,   0,   0,   0,   0,   0,   0,  0,  0,  0,   0,   0,   0,   0,
    0,   0,   255, 218, 0,   12,  3,   1,  0,  2,  17,  3,   17,  0,   63,
    0,   178, 192, 7,   255, 217};

static unsigned char kWEBPImage[] = {
    82, 73, 70, 70, 74,  0, 0,  0,   87,  69,  66,  80,  86,  80, 56, 88, 10,
    0,  0,  0,  16, 0,   0, 0,  0,   0,   0,   0,   0,   0,   65, 76, 80, 72,
    12, 0,  0,  0,  1,   7, 16, 17,  253, 15,  68,  68,  255, 3,  0,  0,  86,
    80, 56, 32, 24, 0,   0, 0,  48,  1,   0,   157, 1,   42,  1,  0,  1,  0,
    3,  0,  52, 37, 164, 0, 3,  112, 0,   254, 251, 253, 80,  0};

}  // namespace

namespace image_fetcher {

class IOSImageDecoderImplTest : public PlatformTest {
 public:
  void OnImageDecoded(const gfx::Image& image) { decoded_image_ = image; }

 protected:
  IOSImageDecoderImplTest() {
    ios_image_decoder_impl_ = CreateIOSImageDecoder();
  }

  ~IOSImageDecoderImplTest() override = default;

  base::test::TaskEnvironment scoped_task_evironment_;
  std::unique_ptr<ImageDecoder> ios_image_decoder_impl_;

  gfx::Image decoded_image_;
};

TEST_F(IOSImageDecoderImplTest, JPGImage) {
  ASSERT_TRUE(decoded_image_.IsEmpty());

  std::string image_data =
      std::string(reinterpret_cast<char*>(kJPGImage), sizeof(kJPGImage));
  ios_image_decoder_impl_->DecodeImage(
      image_data, gfx::Size(), /*data_decoder=*/nullptr,
      base::BindOnce(&IOSImageDecoderImplTest::OnImageDecoded,
                     base::Unretained(this)));

  scoped_task_evironment_.RunUntilIdle();

  EXPECT_FALSE(decoded_image_.IsEmpty());
}

TEST_F(IOSImageDecoderImplTest, WebpImage) {
  ASSERT_TRUE(decoded_image_.IsEmpty());

  std::string image_data =
      std::string(reinterpret_cast<char*>(kWEBPImage), sizeof(kWEBPImage));
  ios_image_decoder_impl_->DecodeImage(
      image_data, gfx::Size(), /*data_decoder=*/nullptr,
      base::BindOnce(&IOSImageDecoderImplTest::OnImageDecoded,
                     base::Unretained(this)));

  scoped_task_evironment_.RunUntilIdle();

  EXPECT_FALSE(decoded_image_.IsEmpty());
}

}  // namespace image_fetcher
