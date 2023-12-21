// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sanitized_image_source.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "base/test/mock_callback.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
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

using data_decoder::mojom::AnimationFramePtr;

namespace {

AnimationFramePtr MakeImageFrame(SkColor color) {
  auto frame = data_decoder::mojom::AnimationFrame::New();

  SkBitmap bitmap;
  bitmap.allocN32Pixels(5, 5);
  bitmap.eraseColor(color);

  frame->bitmap = bitmap;
  frame->duration = base::TimeDelta();

  return frame;
}

}  // namespace

MATCHER_P(MemoryEq, other, "Eq matcher for base::RefCountedMemory contents") {
  return arg->Equals(other);
}

class MockDataDecoderDelegate
    : public SanitizedImageSource::DataDecoderDelegate {
 public:
  MOCK_METHOD(void,
              DecodeImage,
              (const std::string& data,
               SanitizedImageSource::DecodeImageCallback callback));
  MOCK_METHOD(void,
              DecodeAnimation,
              (const std::string& data,
               SanitizedImageSource::DecodeAnimationCallback callback));
};

class SanitizedImageSourceTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    auto data_decoder_delegate = std::make_unique<MockDataDecoderDelegate>();
    mock_data_decoder_delegate_ = data_decoder_delegate.get();
    sanitized_image_source_ = std::make_unique<SanitizedImageSource>(
        profile_.get(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        std::move(data_decoder_delegate));
  }

  void TearDown() override {
    mock_data_decoder_delegate_ = nullptr;
    sanitized_image_source_.reset();
    profile_.reset();
    test_url_loader_factory_.ClearResponses();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  raw_ptr<MockDataDecoderDelegate> mock_data_decoder_delegate_ = nullptr;
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
    EXPECT_CALL(*mock_data_decoder_delegate_, DecodeAnimation(body, testing::_))
        .Times(1)
        .WillOnce(
            [color](const std::string&,
                    SanitizedImageSource::DecodeAnimationCallback callback) {
              std::vector<AnimationFramePtr> frames;
              frames.push_back(MakeImageFrame(color));
              std::move(callback).Run(std::move(frames));
            });
    auto image = gfx::Image::CreateFrom1xBitmap(MakeImageFrame(color)->bitmap);
    EXPECT_CALL(callback, Run(MemoryEq(image.As1xPNGBytes()))).Times(1);
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
  EXPECT_CALL(*mock_data_decoder_delegate_,
              DecodeAnimation(testing::_, testing::_))
      .Times(0);
  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  EXPECT_CALL(callback, Run(testing::IsNull())).Times(1);

  // Issue request.
  sanitized_image_source_->StartDataRequest(
      GURL(base::StrCat({chrome::kChromeUIImageURL, "?", kImageUrl})),
      content::WebContents::Getter(), callback.Get());
  task_environment_.RunUntilIdle();
}

// Verifies that the image source sends back an error in case the external
// image is served via an HTTP scheme.
TEST_F(SanitizedImageSourceTest, HttpScheme) {
  constexpr char kImageUrl[] = "http://foo.com/img.png";

  // Set up expectations and mock data.
  test_url_loader_factory_.AddResponse(kImageUrl, "abcd");
  EXPECT_CALL(*mock_data_decoder_delegate_,
              DecodeAnimation(testing::_, testing::_))
      .Times(0);
  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  EXPECT_CALL(callback, Run(testing::IsNull())).Times(1);

  // Issue request.
  sanitized_image_source_->StartDataRequest(
      GURL(base::StrCat({chrome::kChromeUIImageURL, "?", kImageUrl})),
      content::WebContents::Getter(), callback.Get());
  task_environment_.RunUntilIdle();
}

// Verifies that the image source ignores requests with a wrong URL.
TEST_F(SanitizedImageSourceTest, WrongUrl) {
  // Set up expectations and mock data.
  EXPECT_CALL(*mock_data_decoder_delegate_,
              DecodeAnimation(testing::_, testing::_))
      .Times(0);
  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  EXPECT_CALL(callback, Run(testing::IsNull())).Times(2);

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

// Verifies that the image source sends a Google Photos auth token with its data
// request if and only if asked to by URL specification.
TEST_F(SanitizedImageSourceTest, GooglePhotosImage) {
  constexpr char kImageUrl[] = "https://lh3.googleusercontent.com/img.png";
  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakePrimaryAccountAvailable("test@gmail.com",
                                                signin::ConsentLevel::kSync);
  sanitized_image_source_->set_identity_manager_for_test(
      identity_test_env.identity_manager());

  // Verify that by default, requests are sent with no auth token.
  sanitized_image_source_->StartDataRequest(
      GURL(base::StrCat({chrome::kChromeUIImageURL, "?", kImageUrl})),
      content::WebContents::Getter(), callback.Get());
  EXPECT_FALSE(identity_test_env.IsAccessTokenRequestPending());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_FALSE(
      test_url_loader_factory_.GetPendingRequest(0)->request.headers.HasHeader(
          net::HttpRequestHeaders::kAuthorization));

  // Encode a URL so that it can be used as a param value.
  url::RawCanonOutputT<char> encoded_url;
  url::EncodeURIComponent(kImageUrl, &encoded_url);
  EXPECT_GT(encoded_url.length(), 0u);

  // Verify that param-formatted requests can be sent with auth tokens.
  sanitized_image_source_->StartDataRequest(
      GURL(base::StrCat({chrome::kChromeUIImageURL, "?url=", encoded_url.view(),
                         "&isGooglePhotos=true"})),
      content::WebContents::Getter(), callback.Get());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(identity_test_env.IsAccessTokenRequestPending());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "token", base::Time::Max());
  EXPECT_FALSE(identity_test_env.IsAccessTokenRequestPending());
  ASSERT_EQ(2, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(
      test_url_loader_factory_.GetPendingRequest(1)->request.headers.HasHeader(
          net::HttpRequestHeaders::kAuthorization));

  // Verify that param-formatted requests can be sent without auth tokens.
  sanitized_image_source_->StartDataRequest(
      GURL(base::StrCat({chrome::kChromeUIImageURL, "?url=", encoded_url.view(),
                         "&isGooglePhotos=false"})),
      content::WebContents::Getter(), callback.Get());
  EXPECT_FALSE(identity_test_env.IsAccessTokenRequestPending());
  ASSERT_EQ(3, test_url_loader_factory_.NumPending());
  EXPECT_FALSE(
      test_url_loader_factory_.GetPendingRequest(2)->request.headers.HasHeader(
          net::HttpRequestHeaders::kAuthorization));

  sanitized_image_source_->StartDataRequest(
      GURL(base::StrCat(
          {chrome::kChromeUIImageURL, "?url=", encoded_url.view()})),
      content::WebContents::Getter(), callback.Get());
  EXPECT_FALSE(identity_test_env.IsAccessTokenRequestPending());

  ASSERT_EQ(4, test_url_loader_factory_.NumPending());
  EXPECT_FALSE(
      test_url_loader_factory_.GetPendingRequest(3)->request.headers.HasHeader(
          net::HttpRequestHeaders::kAuthorization));

  // Verify that no download is attempted when authentication fails.
  sanitized_image_source_->StartDataRequest(
      GURL(base::StrCat({chrome::kChromeUIImageURL, "?url=", encoded_url.view(),
                         "&isGooglePhotos=true"})),
      content::WebContents::Getter(), callback.Get());
  ASSERT_EQ(4, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(identity_test_env.IsAccessTokenRequestPending());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
  EXPECT_FALSE(identity_test_env.IsAccessTokenRequestPending());
  ASSERT_EQ(4, test_url_loader_factory_.NumPending());

  // Verify that no auth token is sent for URLs not served by Google Photos.
  constexpr char kBadImageUrl[] = "https://foo.com/img.png";
  url::RawCanonOutputT<char> encoded_bad_url;
  url::EncodeURIComponent(kBadImageUrl, &encoded_bad_url);
  EXPECT_GT(encoded_bad_url.length(), 0u);

  sanitized_image_source_->StartDataRequest(
      GURL(base::StrCat({chrome::kChromeUIImageURL, "?url=",
                         encoded_bad_url.view(), "&isGooglePhotos=true"})),
      content::WebContents::Getter(), callback.Get());
  EXPECT_FALSE(identity_test_env.IsAccessTokenRequestPending());
  ASSERT_EQ(5, test_url_loader_factory_.NumPending());
  EXPECT_FALSE(
      test_url_loader_factory_.GetPendingRequest(4)->request.headers.HasHeader(
          net::HttpRequestHeaders::kAuthorization));
}

TEST_F(SanitizedImageSourceTest, StaticImage) {
  const std::string test_body = "abc";
  const std::string test_url = "https://foo.com/img.png";

  // Set up expectations and mock data.
  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  EXPECT_CALL(*mock_data_decoder_delegate_,
              DecodeAnimation(test_body, testing::_))
      .Times(1)
      .WillOnce([](const std::string&,
                   SanitizedImageSource::DecodeAnimationCallback callback) {
        std::vector<AnimationFramePtr> frames;
        frames.push_back(MakeImageFrame(SK_ColorRED));
        std::move(callback).Run(std::move(frames));
      });
  auto image =
      gfx::Image::CreateFrom1xBitmap(MakeImageFrame(SK_ColorRED)->bitmap);
  EXPECT_CALL(callback, Run(MemoryEq(image.As1xPNGBytes()))).Times(1);

  // Issue requests.
  sanitized_image_source_->StartDataRequest(
      GURL(base::StrCat({chrome::kChromeUIImageURL, "?", test_url})),
      content::WebContents::Getter(), callback.Get());

  // Answer requests and check correctness.
  auto* request = test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
            request->request.credentials_mode);
  EXPECT_EQ(test_url, request->request.url);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, test_body);

  task_environment_.RunUntilIdle();
}

TEST_F(SanitizedImageSourceTest, StaticImageWithWebPEncode) {
  const std::string test_body = "abc";
  const std::string test_url = "https://foo.com/img.png";

  // Set up expectations and mock data.
  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  EXPECT_CALL(*mock_data_decoder_delegate_,
              DecodeAnimation(test_body, testing::_))
      .Times(1)
      .WillOnce([](const std::string&,
                   SanitizedImageSource::DecodeAnimationCallback callback) {
        std::vector<AnimationFramePtr> frames;
        frames.push_back(MakeImageFrame(SK_ColorRED));
        std::move(callback).Run(std::move(frames));
      });
  auto image =
      gfx::Image::CreateFrom1xBitmap(MakeImageFrame(SK_ColorRED)->bitmap);
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce([](scoped_refptr<base::RefCountedMemory> bytes) {
        std::string data_string(reinterpret_cast<char const*>(bytes->data()));
        // Make sure the image is encoded into WebP format.
        EXPECT_TRUE(base::StartsWith(data_string, "RIFF"));
      });

  // Issue requests.
  sanitized_image_source_->StartDataRequest(
      GURL(base::StrCat(
          {chrome::kChromeUIImageURL, "?url=", test_url, "&encodeType=webp"})),
      content::WebContents::Getter(), callback.Get());

  // Answer requests and check correctness.
  auto* request = test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
            request->request.credentials_mode);
  EXPECT_EQ(test_url, request->request.url);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, test_body);

  task_environment_.RunUntilIdle();
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(SanitizedImageSourceTest, AnimatedImage) {
  const std::string test_body = "abc";
  const std::string test_url = "https://foo.com/img.png";

  // Set up expectations and mock data.
  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  EXPECT_CALL(*mock_data_decoder_delegate_,
              DecodeAnimation(test_body, testing::_))
      .Times(1)
      .WillOnce([](const std::string&,
                   SanitizedImageSource::DecodeAnimationCallback callback) {
        std::vector<AnimationFramePtr> frames;
        frames.push_back(MakeImageFrame(SK_ColorRED));
        frames.push_back(MakeImageFrame(SK_ColorBLUE));
        frames.push_back(MakeImageFrame(SK_ColorGREEN));
        std::move(callback).Run(std::move(frames));
      });
  auto image =
      gfx::Image::CreateFrom1xBitmap(MakeImageFrame(SK_ColorRED)->bitmap);
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce([](scoped_refptr<base::RefCountedMemory> bytes) {
        std::string data_string(reinterpret_cast<char const*>(bytes->data()));
        // Make sure the image is encoded into WebP format.
        EXPECT_TRUE(base::StartsWith(data_string, "RIFF"));
      });

  // Issue requests.
  sanitized_image_source_->StartDataRequest(
      GURL(base::StrCat({chrome::kChromeUIImageURL, "?", test_url})),
      content::WebContents::Getter(), callback.Get());

  // Answer requests and check correctness.
  auto* request = test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
            request->request.credentials_mode);
  EXPECT_EQ(test_url, request->request.url);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, test_body);

  task_environment_.RunUntilIdle();
}

TEST_F(SanitizedImageSourceTest, AnimatedImageWithStaticEncode) {
  const std::string test_body = "abc";
  const std::string test_url = "https://foo.com/img.png";

  // Set up expectations and mock data.
  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  EXPECT_CALL(*mock_data_decoder_delegate_, DecodeImage(test_body, testing::_))
      .Times(1)
      .WillOnce([](const std::string&,
                   SanitizedImageSource::DecodeImageCallback callback) {
        std::move(callback).Run(std::move(MakeImageFrame(SK_ColorRED)->bitmap));
      });
  auto image =
      gfx::Image::CreateFrom1xBitmap(MakeImageFrame(SK_ColorRED)->bitmap);
  // Make sure the image is encoded into static PNG bytes.
  EXPECT_CALL(callback, Run(MemoryEq(image.As1xPNGBytes()))).Times(1);

  // Issue requests.
  sanitized_image_source_->StartDataRequest(
      GURL(base::StrCat({chrome::kChromeUIImageURL, "?url=", test_url,
                         "&staticEncode=true"})),
      content::WebContents::Getter(), callback.Get());

  // Answer requests and check correctness.
  auto* request = test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
            request->request.credentials_mode);
  EXPECT_EQ(test_url, request->request.url);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, test_body);

  task_environment_.RunUntilIdle();
}

#endif  // BUILDFLAG(IS_CHROMEOS)
