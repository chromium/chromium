// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sanitized_image_source.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
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
#include "url/url_util.h"

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
  raw_ptr<MockImageDecoder> mock_image_decoder_;
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

  ASSERT_EQ(data.size(),
            static_cast<unsigned long>(test_url_loader_factory_.NumPending()));

  // Answer requests and check correctness.
  for (size_t i = 0; i < data.size(); i++) {
    auto [color, url, body] = data[i];
    auto* request = test_url_loader_factory_.GetPendingRequest(i);
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
              request->request.credentials_mode);
    EXPECT_EQ(url, request->request.url);
    test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
        request, body);
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

// Verifies that the image source sends cookies with its data request if and
// only if asked to by URL specification.
TEST_F(SanitizedImageSourceTest, CookiesInclusion) {
  constexpr char kImageUrl[] = "https://foo.com/img.png";
  base::MockCallback<content::URLDataSource::GotDataCallback> callback;

  // Verify that by default, requests are sent without cookies.
  sanitized_image_source_->StartDataRequest(
      GURL(base::StrCat({chrome::kChromeUIImageURL, "?", kImageUrl})),
      content::WebContents::Getter(), callback.Get());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_FALSE(
      test_url_loader_factory_.GetPendingRequest(0)->request.SendsCookies());

  // Encode a URL so that it can be used as a param value.
  url::RawCanonOutputT<char> encoded_url;
  url::EncodeURIComponent(kImageUrl, std::size(kImageUrl), &encoded_url);
  EXPECT_GT(encoded_url.length(), 0);
  auto encoded_url_str =
      base::StringPiece(encoded_url.data(), encoded_url.length());

  // Verify that param-formatted requests can be sent with cookies.
  sanitized_image_source_->StartDataRequest(
      GURL(base::StrCat({chrome::kChromeUIImageURL, "?url=", encoded_url_str,
                         "&withCookies=true"})),
      content::WebContents::Getter(), callback.Get());
  ASSERT_EQ(2, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(
      test_url_loader_factory_.GetPendingRequest(1)->request.SendsCookies());

  // Verify that param-formatted requests can be sent without cookies.
  sanitized_image_source_->StartDataRequest(
      GURL(base::StrCat({chrome::kChromeUIImageURL, "?url=", encoded_url_str,
                         "&withCookies=false"})),
      content::WebContents::Getter(), callback.Get());
  ASSERT_EQ(3, test_url_loader_factory_.NumPending());
  EXPECT_FALSE(
      test_url_loader_factory_.GetPendingRequest(2)->request.SendsCookies());

  sanitized_image_source_->StartDataRequest(
      GURL(base::StrCat({chrome::kChromeUIImageURL, "?url=", encoded_url_str})),
      content::WebContents::Getter(), callback.Get());
  ASSERT_EQ(4, test_url_loader_factory_.NumPending());
  EXPECT_FALSE(
      test_url_loader_factory_.GetPendingRequest(3)->request.SendsCookies());
}
