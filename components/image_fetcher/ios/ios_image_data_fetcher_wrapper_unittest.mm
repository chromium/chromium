// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"

#import <UIKit/UIKit.h>

#include <memory>

#include "base/mac/scoped_block.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#import "components/image_fetcher/ios/webp_decoder.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const unsigned char kJPGImage[] = {
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
    0,   178, 192, 7,   255, 217,
};

const unsigned char kPNGImage[] = {
    137, 80,  78,  71,  13,  10,  26,  10,  0,   0,   0,   13,  73,  72,  68,
    82,  0,   0,   0,   1,   0,   0,   0,   1,   1,   0,   0,   0,   0,   55,
    110, 249, 36,  0,   0,   0,   2,   98,  75,  71,  68,  0,   1,   221, 138,
    19,  164, 0,   0,   0,   9,   112, 72,  89,  115, 0,   0,   11,  18,  0,
    0,   11,  18,  1,   210, 221, 126, 252, 0,   0,   0,   9,   118, 112, 65,
    103, 0,   0,   0,   1,   0,   0,   0,   1,   0,   199, 149, 95,  237, 0,
    0,   0,   10,  73,  68,  65,  84,  8,   215, 99,  104, 0,   0,   0,   130,
    0,   129, 221, 67,  106, 244, 0,   0,   0,   25,  116, 69,  88,  116, 99,
    111, 109, 109, 101, 110, 116, 0,   67,  114, 101, 97,  116, 101, 100, 32,
    119, 105, 116, 104, 32,  71,  73,  77,  80,  231, 175, 64,  203, 0,   0,
    0,   37,  116, 69,  88,  116, 100, 97,  116, 101, 58,  99,  114, 101, 97,
    116, 101, 0,   50,  48,  49,  49,  45,  48,  54,  45,  50,  50,  84,  49,
    54,  58,  49,  54,  58,  52,  54,  43,  48,  50,  58,  48,  48,  31,  248,
    231, 223, 0,   0,   0,   37,  116, 69,  88,  116, 100, 97,  116, 101, 58,
    109, 111, 100, 105, 102, 121, 0,   50,  48,  49,  49,  45,  48,  54,  45,
    50,  50,  84,  49,  54,  58,  49,  54,  58,  52,  54,  43,  48,  50,  58,
    48,  48,  110, 165, 95,  99,  0,   0,   0,   17,  116, 69,  88,  116, 106,
    112, 101, 103, 58,  99,  111, 108, 111, 114, 115, 112, 97,  99,  101, 0,
    50,  44,  117, 85,  159, 0,   0,   0,   32,  116, 69,  88,  116, 106, 112,
    101, 103, 58,  115, 97,  109, 112, 108, 105, 110, 103, 45,  102, 97,  99,
    116, 111, 114, 0,   50,  120, 50,  44,  49,  120, 49,  44,  49,  120, 49,
    73,  250, 166, 180, 0,   0,   0,   0,   73,  69,  78,  68,  174, 66,  96,
    130,
};

const unsigned char kWEBPImage[] = {
    82, 73, 70, 70, 74,  0, 0,  0,   87,  69,  66,  80,  86,  80, 56, 88, 10,
    0,  0,  0,  16, 0,   0, 0,  0,   0,   0,   0,   0,   0,   65, 76, 80, 72,
    12, 0,  0,  0,  1,   7, 16, 17,  253, 15,  68,  68,  255, 3,  0,  0,  86,
    80, 56, 32, 24, 0,   0, 0,  48,  1,   0,   157, 1,   42,  1,  0,  1,  0,
    3,  0,  52, 37, 164, 0, 3,  112, 0,   254, 251, 253, 80,  0,
};

const char kTestUrl[] = "http://www.img.com";

const char kWEBPHeaderResponse[] =
    "HTTP/1.1 200 OK\0Content-type: image/webp\0\0";

// Returns a NSData object containing the decoded image.
NSData* DecodedWebpImage() {
  return webp_transcode::WebpDecoder::DecodeWebpImage([NSData
      dataWithBytes:reinterpret_cast<const char*>(kWEBPImage)
             length:sizeof(kWEBPImage)]);
}

}  // namespace

namespace image_fetcher {

class IOSImageDataFetcherWrapperTest : public PlatformTest {
 protected:
  IOSImageDataFetcherWrapperTest()
      : callback_([^(NSData* data) {
          result_data_ = data;
          result_ = [UIImage imageWithData:data];
          called_ = true;
        } copy]) {
    shared_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &factory_);
    image_fetcher_ =
        std::make_unique<IOSImageDataFetcherWrapper>(shared_factory_);
  }

  void SetupFetcher() {
    image_fetcher_->FetchImageDataWebpDecoded(GURL(kTestUrl), callback_);
    EXPECT_EQ(nil, result_);
    EXPECT_EQ(false, called_);
  }

  // Message loop for the main test thread.
  base::test::TaskEnvironment environment_;

  base::mac::ScopedBlock<ImageDataFetcherBlock> callback_;
  network::TestURLLoaderFactory factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  std::unique_ptr<IOSImageDataFetcherWrapper> image_fetcher_;
  NSData* result_data_ = nil;
  UIImage* result_ = nil;
  bool called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(IOSImageDataFetcherWrapperTest);
};

TEST_F(IOSImageDataFetcherWrapperTest, TestError) {
  SetupFetcher();
  factory_.AddResponse(kTestUrl, "", net::HTTP_NOT_FOUND);
  environment_.RunUntilIdle();
  EXPECT_EQ(nil, result_);
  EXPECT_TRUE(called_);
}

TEST_F(IOSImageDataFetcherWrapperTest, TestJpg) {
  SetupFetcher();
  factory_.AddResponse(
      kTestUrl,
      std::string(reinterpret_cast<const char*>(kJPGImage), sizeof(kJPGImage)));
  environment_.RunUntilIdle();
  EXPECT_NE(nil, result_);
  EXPECT_TRUE(called_);
}

TEST_F(IOSImageDataFetcherWrapperTest, TestPng) {
  SetupFetcher();
  factory_.AddResponse(
      kTestUrl,
      std::string(reinterpret_cast<const char*>(kPNGImage), sizeof(kPNGImage)));
  environment_.RunUntilIdle();
  EXPECT_NE(nil, result_);
  EXPECT_TRUE(called_);
}

TEST_F(IOSImageDataFetcherWrapperTest, TestGoodWebP) {
  SetupFetcher();

  std::string content(reinterpret_cast<const char*>(kWEBPImage),
                      sizeof(kWEBPImage));
  network::mojom::URLResponseHeadPtr head =
      network::mojom::URLResponseHead::New();
  head->headers = new net::HttpResponseHeaders(
      std::string(kWEBPHeaderResponse, base::size(kWEBPHeaderResponse)));
  head->mime_type = "image/webp";
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = content.size();
  factory_.AddResponse(GURL(kTestUrl), std::move(head), content, status);
  environment_.RunUntilIdle();
  EXPECT_NE(nil, result_);
  EXPECT_TRUE(called_);
}

TEST_F(IOSImageDataFetcherWrapperTest, TestGoodWebPNoHeader) {
  SetupFetcher();
  factory_.AddResponse(kTestUrl,
                       std::string(reinterpret_cast<const char*>(kWEBPImage),
                                   sizeof(kWEBPImage)));
  environment_.RunUntilIdle();
  EXPECT_TRUE([DecodedWebpImage() isEqualToData:result_data_]);
  EXPECT_TRUE(called_);
}

TEST_F(IOSImageDataFetcherWrapperTest, TestBadWebP) {
  SetupFetcher();

  std::string content = "This is not a valid WebP image";
  network::mojom::URLResponseHeadPtr head =
      network::mojom::URLResponseHead::New();
  head->headers = new net::HttpResponseHeaders(
      std::string(kWEBPHeaderResponse, base::size(kWEBPHeaderResponse)));
  head->mime_type = "image/webp";
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = content.size();
  factory_.AddResponse(GURL(kTestUrl), std::move(head), content, status);
  environment_.RunUntilIdle();
  EXPECT_EQ(nil, result_);
  EXPECT_TRUE(called_);
}

TEST_F(IOSImageDataFetcherWrapperTest, DeleteDuringWebPDecoding) {
  SetupFetcher();
  // To control timing of events precisely below, this needs to inject the
  // result synchronously, so it's done via some test-only methods rather than
  // via TestURLLoaderFactory.
  image_fetcher::RequestMetadata metadata;
  metadata.mime_type = "image/webp";
  metadata.http_response_code = 200;
  image_fetcher_->AccessImageDataFetcherForTesting()->InjectResultForTesting(
      metadata, std::string(reinterpret_cast<const char*>(kWEBPImage),
                            sizeof(kWEBPImage)));

  // Delete the image fetcher, and check that the callback is called.
  image_fetcher_.reset();
  environment_.RunUntilIdle();
  EXPECT_TRUE([DecodedWebpImage() isEqualToData:result_data_]);
  EXPECT_TRUE(called_);
}

TEST_F(IOSImageDataFetcherWrapperTest, TestCallbacksNotCalledDuringDeletion) {
  image_fetcher_->FetchImageDataWebpDecoded(GURL(kTestUrl), callback_);
  image_fetcher_.reset();
  EXPECT_FALSE(called_);
}

}  // namespace image_fetcher
