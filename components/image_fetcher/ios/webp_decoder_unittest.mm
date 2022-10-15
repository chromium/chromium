// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/image_fetcher/ios/webp_decoder.h"

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace webp_transcode {
namespace {

class WebpDecoderDelegate : public WebpDecoder::Delegate {
 public:
  WebpDecoderDelegate() : image_([[NSMutableData alloc] init]) {}

  NSData* GetImage() const { return image_; }

  // WebpDecoder::Delegate methods.
  MOCK_METHOD1(OnFinishedDecoding, void(bool success));
  MOCK_METHOD2(SetImageFeatures,
               void(size_t total_size, WebpDecoder::DecodedImageFormat format));
  void OnDataDecoded(NSData* data) override { [image_ appendData:data]; }

 private:
  ~WebpDecoderDelegate() override {}

  __strong NSMutableData* image_;
};

class WebpDecoderTest : public testing::Test {
 public:
  WebpDecoderTest()
      : delegate_(new WebpDecoderDelegate),
        decoder_(new WebpDecoder(delegate_.get())) {}

  NSData* LoadImage(const base::FilePath& filename) {
    base::FilePath path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.AppendASCII("components/test/data/webp_transcode")
               .Append(filename);
    return
        [NSData dataWithContentsOfFile:base::SysUTF8ToNSString(path.value())];
  }

  std::vector<uint8_t>* DecompressData(NSData* data,
                                       WebpDecoder::DecodedImageFormat format) {
    base::ScopedCFTypeRef<CGDataProviderRef> provider(
        CGDataProviderCreateWithCFData((CFDataRef)data));
    base::ScopedCFTypeRef<CGImageRef> image;
    switch (format) {
      case WebpDecoder::JPEG:
        image.reset(CGImageCreateWithJPEGDataProvider(
            provider, nullptr, false, kCGRenderingIntentDefault));
        break;
      case WebpDecoder::PNG:
        image.reset(CGImageCreateWithPNGDataProvider(
            provider, nullptr, false, kCGRenderingIntentDefault));
        break;
      case WebpDecoder::TIFF:
        ADD_FAILURE() << "Data already decompressed";
        return nil;
    }
    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);
    base::ScopedCFTypeRef<CGColorSpaceRef> color_space(
        CGColorSpaceCreateDeviceRGB());
    size_t bytes_per_pixel = 4;
    size_t bytes_per_row = bytes_per_pixel * width;
    size_t bits_per_component = 8;
    std::vector<uint8_t>* result =
        new std::vector<uint8_t>(width * height * bytes_per_pixel, 0);
    base::ScopedCFTypeRef<CGContextRef> context(CGBitmapContextCreate(
        &result->front(), width, height, bits_per_component, bytes_per_row,
        color_space,
        base::to_underlying(kCGImageAlphaPremultipliedLast) |
            base::to_underlying(kCGBitmapByteOrder32Big)));
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
    // Check that someting has been written in |result|.
    std::vector<uint8_t> zeroes(width * height * bytes_per_pixel, 0);
    EXPECT_NE(0, memcmp(&result->front(), &zeroes.front(), zeroes.size()))
        << "Decompression failed.";
    return result;
  }

  // Compares data, allowing an averaged absolute difference of 1.
  bool CompareUncompressedData(const uint8_t* ptr_1,
                               const uint8_t* ptr_2,
                               size_t size) {
    uint64_t difference = 0;
    for (size_t i = 0; i < size; ++i) {
      // Casting to int to avoid overflow.
      int error = abs(int(ptr_1[i]) - int(ptr_2[i]));
      EXPECT_GE(difference + error, difference)
          << "Image difference too big (overflow).";
      difference += error;
    }
    double average_difference = double(difference) / double(size);
    DLOG(INFO) << "Average image difference: " << average_difference;
    return average_difference < 1.5;
  }

  bool CheckCompressedImagesEqual(NSData* data_1,
                                  NSData* data_2,
                                  WebpDecoder::DecodedImageFormat format) {
    std::unique_ptr<std::vector<uint8_t>> uncompressed_1(
        DecompressData(data_1, format));
    std::unique_ptr<std::vector<uint8_t>> uncompressed_2(
        DecompressData(data_2, format));
    if (uncompressed_1->size() != uncompressed_2->size()) {
      DLOG(ERROR) << "Image sizes don't match";
      return false;
    }
    return CompareUncompressedData(&uncompressed_1->front(),
                                   &uncompressed_2->front(),
                                   uncompressed_1->size());
  }

  bool CheckTiffImagesEqual(NSData* image_1, NSData* image_2) {
    if ([image_1 length] != [image_2 length]) {
      DLOG(ERROR) << "Image lengths don't match";
      return false;
    }
    // Compare headers.
    const size_t kHeaderSize = WebpDecoder::GetHeaderSize();
    NSData* header_1 = [image_1 subdataWithRange:NSMakeRange(0, kHeaderSize)];
    NSData* header_2 = [image_2 subdataWithRange:NSMakeRange(0, kHeaderSize)];
    if (!header_1 || !header_2)
      return false;
    if (![header_1 isEqualToData:header_2]) {
      DLOG(ERROR) << "Headers don't match.";
      return false;
    }
    return CompareUncompressedData(
        static_cast<const uint8_t*>([image_1 bytes]) + kHeaderSize,
        static_cast<const uint8_t*>([image_2 bytes]) + kHeaderSize,
        [image_1 length] - kHeaderSize);
  }

 protected:
  scoped_refptr<WebpDecoderDelegate> delegate_;
  scoped_refptr<WebpDecoder> decoder_;
};

}  // namespace

TEST_F(WebpDecoderTest, DecodeToJpeg) {
  // Load a WebP image from disk.
  NSData* webp_image = LoadImage(base::FilePath("test.webp"));
  ASSERT_TRUE(webp_image != nil);
  // Load reference image.
  NSData* jpg_image = LoadImage(base::FilePath("test.jpg"));
  ASSERT_TRUE(jpg_image != nil);
  // Convert to JPEG.
  EXPECT_CALL(*delegate_, OnFinishedDecoding(true)).Times(1);
  EXPECT_CALL(*delegate_, SetImageFeatures(testing::_, WebpDecoder::JPEG))
      .Times(1);
  decoder_->OnDataReceived(webp_image);
  // Compare to reference image.
  EXPECT_TRUE(CheckCompressedImagesEqual(jpg_image, delegate_->GetImage(),
                                         WebpDecoder::JPEG));
}

TEST_F(WebpDecoderTest, DecodeToPng) {
  // Load a WebP image from disk.
  NSData* webp_image = LoadImage(base::FilePath("test_alpha.webp"));
  ASSERT_TRUE(webp_image != nil);
  // Load reference image.
  NSData* png_image = LoadImage(base::FilePath("test_alpha.png"));
  ASSERT_TRUE(png_image != nil);
  // Convert to PNG.
  EXPECT_CALL(*delegate_, OnFinishedDecoding(true)).Times(1);
  EXPECT_CALL(*delegate_, SetImageFeatures(testing::_, WebpDecoder::PNG))
      .Times(1);
  decoder_->OnDataReceived(webp_image);
  // Compare to reference image.
  EXPECT_TRUE(CheckCompressedImagesEqual(png_image, delegate_->GetImage(),
                                         WebpDecoder::PNG));
}

TEST_F(WebpDecoderTest, DecodeToTiff) {
  // Load a WebP image from disk.
  NSData* webp_image = LoadImage(base::FilePath("test_small.webp"));
  ASSERT_TRUE(webp_image != nil);
  // Load reference image.
  NSData* tiff_image = LoadImage(base::FilePath("test_small.tiff"));
  ASSERT_TRUE(tiff_image != nil);
  // Convert to TIFF.
  EXPECT_CALL(*delegate_, OnFinishedDecoding(true)).Times(1);
  EXPECT_CALL(*delegate_,
              SetImageFeatures([tiff_image length], WebpDecoder::TIFF))
      .Times(1);
  decoder_->OnDataReceived(webp_image);
  // Compare to reference image.
  EXPECT_TRUE(CheckTiffImagesEqual(tiff_image, delegate_->GetImage()));
}

TEST_F(WebpDecoderTest, StreamedDecode) {
  // Load a WebP image from disk.
  NSData* webp_image = LoadImage(base::FilePath("test.webp"));
  ASSERT_TRUE(webp_image != nil);
  // Load reference image.
  NSData* jpg_image = LoadImage(base::FilePath("test.jpg"));
  ASSERT_TRUE(jpg_image != nil);
  // Convert to JPEG in chunks.
  EXPECT_CALL(*delegate_, OnFinishedDecoding(true)).Times(1);
  EXPECT_CALL(*delegate_, SetImageFeatures(testing::_, WebpDecoder::JPEG))
      .Times(1);
  const size_t kChunkSize = 10;
  unsigned int num_chunks = 0;
  while ([webp_image length] > kChunkSize) {
    NSData* chunk = [webp_image subdataWithRange:NSMakeRange(0, kChunkSize)];
    decoder_->OnDataReceived(chunk);
    webp_image = [webp_image
        subdataWithRange:NSMakeRange(kChunkSize,
                                     [webp_image length] - kChunkSize)];
    ++num_chunks;
  }
  if ([webp_image length] > 0u) {
    decoder_->OnDataReceived(webp_image);
    ++num_chunks;
  }
  ASSERT_GT(num_chunks, 3u) << "Not enough chunks";
  // Compare to reference image.
  EXPECT_TRUE(CheckCompressedImagesEqual(jpg_image, delegate_->GetImage(),
                                         WebpDecoder::JPEG));
}

TEST_F(WebpDecoderTest, InvalidFormat) {
  EXPECT_CALL(*delegate_, OnFinishedDecoding(false)).Times(1);
  const char dummy_image[] = "(>'-')> <('-'<) ^('-')^ <('-'<) (>'-')>";
  NSData* data = [[NSData alloc] initWithBytes:dummy_image
                                        length:std::size(dummy_image)];
  decoder_->OnDataReceived(data);
  EXPECT_EQ(0u, [delegate_->GetImage() length]);
}

TEST_F(WebpDecoderTest, DecodeAborted) {
  EXPECT_CALL(*delegate_, OnFinishedDecoding(false)).Times(1);
  decoder_->Stop();
  EXPECT_EQ(0u, [delegate_->GetImage() length]);
}

}  // namespace webp_transcode
