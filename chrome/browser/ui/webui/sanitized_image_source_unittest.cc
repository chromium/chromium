// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sanitized_image_source.h"

#include "base/strings/strcat.h"
#include "base/test/mock_callback.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

namespace {

gfx::Image MakeImage(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(5, 5);
  bitmap.eraseColor(color);
  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

}  // namespace

MATCHER_P(MemoryEq, other, "Eq matcher for base::RefCountedMemory contents") {
  return arg->Equals(other);
}

class MockImageDecoder : public image_fetcher::ImageDecoder {
 public:
  MOCK_METHOD3(DecodeImage,
               void(const std::string&,
                    const gfx::Size&,
                    image_fetcher::ImageDecodedCallback));
};

class SanitizedImageSourceTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    auto image_decoder = std::make_unique<MockImageDecoder>();
    mock_image_decoder_ = image_decoder.get();
    sanitized_image_source_ = std::make_unique<SanitizedImageSource>(
        profile_.get(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        std::move(image_decoder));
  }

  void TearDown() override {
    sanitized_image_source_.reset();
    profile_.reset();
    test_url_loader_factory_.ClearResponses();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  MockImageDecoder* mock_image_decoder_;
  std::unique_ptr<SanitizedImageSource> sanitized_image_source_;
};

// Verifies that the image source can handle multiple requests in parallel.
TEST_F(SanitizedImageSourceTest, MultiRequest) {
  std::vector<std::tuple<SkColor, std::string, std::string>> data(
      {{SK_ColorRED, "https://foo.com/img.png", "abc"},
       {SK_ColorBLUE, "https://bar.com/img.png", "def"},
       {SK_ColorGREEN, "https://baz.com/img.png", "ghi"}});

  // Set up expectations and mock data.
  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  for (const auto& datum : data) {
    SkColor color;
    std::string url;
    std::string body;
    std::tie(color, url, body) = datum;
    test_url_loader_factory_.AddResponse(url, body);
    EXPECT_CALL(*mock_image_decoder_,
                DecodeImage(body, gfx::Size(), testing::_))
        .Times(1)
        .WillOnce([color](const std::string&, const gfx::Size&,
                          image_fetcher::ImageDecodedCallback callback) {
          std::move(callback).Run(MakeImage(color));
        });
    EXPECT_CALL(callback, Run(MemoryEq(MakeImage(color).As1xPNGBytes())))
        .Times(1);
  }

  // Issue requests.
  for (const auto& datum : data) {
    std::string url;
    std::tie(std::ignore, url, std::ignore) = datum;
    sanitized_image_source_->StartDataRequest(
        GURL(base::StrCat({chrome::kChromeUIImageURL, "?", url})),
        content::WebContents::Getter(), callback.Get());
  }
  task_environment_.RunUntilIdle();
}

// Verifies that the image source sends back an empty image in case the external
// image download fails.
TEST_F(SanitizedImageSourceTest, FailedLoad) {
  constexpr char kImageUrl[] = "https://foo.com/img.png";

  // Set up expectations and mock data.
  test_url_loader_factory_.AddResponse(kImageUrl, "", net::HTTP_NOT_FOUND);
  EXPECT_CALL(*mock_image_decoder_,
              DecodeImage(testing::_, testing::_, testing::_))
      .Times(0);
  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  EXPECT_CALL(callback,
              Run(MemoryEq(base::MakeRefCounted<base::RefCountedString>())))
      .Times(1);

  // Issue request.
  sanitized_image_source_->StartDataRequest(
      GURL(base::StrCat({chrome::kChromeUIImageURL, "?", kImageUrl})),
      content::WebContents::Getter(), callback.Get());
  task_environment_.RunUntilIdle();
}

// Verifies that the image source ignores requests with a wrong URL.
TEST_F(SanitizedImageSourceTest, WrongUrl) {
  // Set up expectations and mock data.
  EXPECT_CALL(*mock_image_decoder_,
              DecodeImage(testing::_, testing::_, testing::_))
      .Times(0);
  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  EXPECT_CALL(callback,
              Run(MemoryEq(base::MakeRefCounted<base::RefCountedString>())))
      .Times(2);

  // Issue request.
  sanitized_image_source_->StartDataRequest(
      GURL("chrome://abc?https://foo.com/img.png"),
      content::WebContents::Getter(), callback.Get());
  sanitized_image_source_->StartDataRequest(
      GURL(base::StrCat({chrome::kChromeUIImageURL, "?abc"})),
      content::WebContents::Getter(), callback.Get());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
}
