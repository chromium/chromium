// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/logo_service_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_provider_logos/features.h"
#include "components/search_provider_logos/fixed_logo_api.h"
#include "components/search_provider_logos/google_logo_api.h"
#include "components/search_provider_logos/logo_cache.h"
#include "components/search_provider_logos/logo_observer.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::StrictMock;

using sync_preferences::TestingPrefServiceSyncable;

namespace search_provider_logos {

namespace {

using MockLogoCallback = base::MockCallback<LogoCallback>;
using MockEncodedLogoCallback = base::MockCallback<EncodedLogoCallback>;

scoped_refptr<base::RefCountedString> EncodeBitmapAsPNG(
    const SkBitmap& bitmap) {
  scoped_refptr<base::RefCountedMemory> png_bytes =
      gfx::Image::CreateFrom1xBitmap(bitmap).As1xPNGBytes();
  scoped_refptr<base::RefCountedString> str = new base::RefCountedString();
  str->data().assign(png_bytes->front_as<char>(), png_bytes->size());
  return str;
}

std::string EncodeBitmapAsPNGBase64(const SkBitmap& bitmap) {
  scoped_refptr<base::RefCountedString> png_bytes = EncodeBitmapAsPNG(bitmap);
  std::string encoded_image_base64;
  base::Base64Encode(png_bytes->data(), &encoded_image_base64);
  return encoded_image_base64;
}

SkBitmap MakeBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorBLUE);
  return bitmap;
}

SkBitmap MakeBitmap2(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorRED);
  return bitmap;
}

EncodedLogo EncodeLogo(const Logo& logo) {
  EncodedLogo encoded_logo;
  encoded_logo.encoded_image = EncodeBitmapAsPNG(logo.image);
  encoded_logo.dark_encoded_image = EncodeBitmapAsPNG(logo.dark_image);
  encoded_logo.metadata = logo.metadata;
  return encoded_logo;
}

Logo DecodeLogo(const EncodedLogo& encoded_logo) {
  Logo logo;
  logo.image =
      gfx::Image::CreateFrom1xPNGBytes(encoded_logo.encoded_image->front(),
                                       encoded_logo.encoded_image->size())
          .AsBitmap();
  if (encoded_logo.dark_encoded_image) {
    logo.dark_image = gfx::Image::CreateFrom1xPNGBytes(
                          encoded_logo.dark_encoded_image->front(),
                          encoded_logo.dark_encoded_image->size())
                          .AsBitmap();
  }
  logo.metadata = encoded_logo.metadata;
  return logo;
}

Logo GetSampleLogo(const GURL& logo_url, base::Time response_time) {
  Logo logo;
  logo.image = MakeBitmap(2, 5);
  logo.dark_image = MakeBitmap2(20, 50);
  logo.metadata.can_show_after_expiration = false;
  logo.metadata.expiration_time =
      response_time + base::TimeDelta::FromHours(19);
  logo.metadata.fingerprint = "8bc33a80";
  logo.metadata.source_url =
      AppendPreliminaryParamsToDoodleURL(false, logo_url);
  logo.metadata.on_click_url = GURL("https://www.google.com/search?q=potato");
  logo.metadata.alt_text = "A logo about potatoes";
  logo.metadata.animated_url = GURL("https://www.google.com/logos/doodle.png");
  logo.metadata.dark_animated_url =
      GURL("https://www.google.com/logos/dark_doodle.png");
  logo.metadata.mime_type = "image/png";
  logo.metadata.dark_mime_type = "image/png";
  return logo;
}

Logo GetSampleLogoWithoutDarkImage(const GURL& logo_url,
                                   base::Time response_time) {
  Logo logo;
  logo.image = MakeBitmap(2, 5);
  logo.metadata.can_show_after_expiration = false;
  logo.metadata.expiration_time =
      response_time + base::TimeDelta::FromHours(19);
  logo.metadata.fingerprint = "8bc33a80";
  logo.metadata.source_url =
      AppendPreliminaryParamsToDoodleURL(false, logo_url);
  logo.metadata.on_click_url = GURL("https://www.google.com/search?q=potato");
  logo.metadata.alt_text = "A logo about potatoes";
  logo.metadata.animated_url = GURL("https://www.google.com/logos/doodle.png");
  logo.metadata.mime_type = "image/png";
  return logo;
}

Logo GetSampleLogo2(const GURL& logo_url, base::Time response_time) {
  Logo logo;
  logo.image = MakeBitmap(4, 3);
  logo.metadata.can_show_after_expiration = true;
  logo.metadata.expiration_time = base::Time();
  logo.metadata.fingerprint = "71082741021409127";
  logo.metadata.source_url =
      AppendPreliminaryParamsToDoodleURL(false, logo_url);
  logo.metadata.on_click_url = GURL("https://example.com/page25");
  logo.metadata.alt_text = "The logo for example.com";
  logo.metadata.mime_type = "image/jpeg";
  return logo;
}

std::string MakeServerResponse(const SkBitmap& image,
                               const SkBitmap& dark_image,
                               const std::string& on_click_url,
                               const std::string& alt_text,
                               const std::string& animated_url,
                               const std::string& dark_animated_url,
                               const std::string& mime_type,
                               const std::string& dark_mime_type,
                               const std::string& fingerprint,
                               base::TimeDelta time_to_live) {
  base::DictionaryValue dict;

  std::string data_uri = "data:";
  data_uri += mime_type;
  data_uri += ";base64,";
  data_uri += EncodeBitmapAsPNGBase64(image);

  std::string dark_data_uri = "data:";
  dark_data_uri += dark_mime_type;
  dark_data_uri += ";base64,";
  dark_data_uri += EncodeBitmapAsPNGBase64(dark_image);

  dict.SetString("ddljson.target_url", on_click_url);
  dict.SetString("ddljson.alt_text", alt_text);
  if (animated_url.empty()) {
    dict.SetString("ddljson.doodle_type", "SIMPLE");
    if (!image.isNull())
      dict.SetString("ddljson.data_uri", data_uri);
    if (!dark_image.isNull())
      dict.SetString("ddljson.dark_data_uri", dark_data_uri);
  } else {
    dict.SetString("ddljson.doodle_type", "ANIMATED");
    dict.SetBoolean("ddljson.large_image.is_animated_gif", true);
    dict.SetString("ddljson.large_image.url", animated_url);
    dict.SetString("ddljson.dark_large_image.url", dark_animated_url);
    if (!image.isNull())
      dict.SetString("ddljson.cta_data_uri", data_uri);
    if (!dark_image.isNull())
      dict.SetString("ddljson.dark_cta_data_uri", dark_data_uri);
  }
  dict.SetString("ddljson.fingerprint", fingerprint);
  if (time_to_live != base::TimeDelta())
    dict.SetInteger("ddljson.time_to_live_ms",
                    static_cast<int>(time_to_live.InMilliseconds()));

  std::string output;
  base::JSONWriter::Write(dict, &output);
  return output;
}

std::string MakeServerResponse(const Logo& logo, base::TimeDelta time_to_live) {
  return MakeServerResponse(
      logo.image, logo.dark_image, logo.metadata.on_click_url.spec(),
      logo.metadata.alt_text, logo.metadata.animated_url.spec(),
      logo.metadata.dark_animated_url.spec(), logo.metadata.mime_type,
      logo.metadata.dark_mime_type, logo.metadata.fingerprint, time_to_live);
}

template <typename Arg, typename Matcher>
bool Match(const Arg& arg,
           const Matcher& matcher,
           ::testing::MatchResultListener* result_listener) {
  return ::testing::Matcher<Arg>(matcher).MatchAndExplain(arg, result_listener);
}

MATCHER_P(DecodesTo, decoded_logo, "") {
  return Match(DecodeLogo(arg), Eq(decoded_logo), result_listener);
}

class MockLogoCache : public LogoCache {
 public:
  MockLogoCache() : LogoCache(base::FilePath()) {
    // Delegate actions to the *Internal() methods by default.
    ON_CALL(*this, UpdateCachedLogoMetadata(_))
        .WillByDefault(
            Invoke(this, &MockLogoCache::UpdateCachedLogoMetadataInternal));
    ON_CALL(*this, GetCachedLogoMetadata())
        .WillByDefault(
            Invoke(this, &MockLogoCache::GetCachedLogoMetadataInternal));
    ON_CALL(*this, SetCachedLogo(_))
        .WillByDefault(Invoke(this, &MockLogoCache::SetCachedLogoInternal));
  }

  MOCK_METHOD1(UpdateCachedLogoMetadata, void(const LogoMetadata& metadata));
  MOCK_METHOD0(GetCachedLogoMetadata, const LogoMetadata*());
  MOCK_METHOD1(SetCachedLogo, void(const EncodedLogo* logo));
  // GetCachedLogo() can't be mocked since it returns a scoped_ptr, which is
  // non-copyable. Instead create a method that's pinged when GetCachedLogo() is
  // called.
  MOCK_METHOD0(OnGetCachedLogo, void());

  void EncodeAndSetCachedLogo(const Logo& logo) {
    EncodedLogo encoded_logo = EncodeLogo(logo);
    SetCachedLogo(&encoded_logo);
  }

  void ExpectSetCachedLogo(const Logo* expected_logo) {
    Mock::VerifyAndClearExpectations(this);
    if (expected_logo) {
      EXPECT_CALL(*this, SetCachedLogo(Pointee(DecodesTo(*expected_logo))));
    } else {
      EXPECT_CALL(*this, SetCachedLogo(nullptr));
    }
  }

  void UpdateCachedLogoMetadataInternal(const LogoMetadata& metadata) {
    ASSERT_TRUE(logo_.get());
    ASSERT_TRUE(metadata_.get());
    EXPECT_EQ(metadata_->fingerprint, metadata.fingerprint);
    metadata_.reset(new LogoMetadata(metadata));
    logo_->metadata = metadata;
  }

  const LogoMetadata* GetCachedLogoMetadataInternal() {
    return metadata_.get();
  }

  void SetCachedLogoInternal(const EncodedLogo* logo) {
    logo_ = logo ? std::make_unique<EncodedLogo>(*logo) : nullptr;
    metadata_ = logo ? std::make_unique<LogoMetadata>(logo->metadata) : nullptr;
  }

  std::unique_ptr<EncodedLogo> GetCachedLogo() override {
    OnGetCachedLogo();
    return logo_ ? std::make_unique<EncodedLogo>(*logo_) : nullptr;
  }

 private:
  std::unique_ptr<LogoMetadata> metadata_;
  std::unique_ptr<EncodedLogo> logo_;
};

class FakeImageDecoder : public image_fetcher::ImageDecoder {
 public:
  void DecodeImage(const std::string& image_data,
                   const gfx::Size& desired_image_frame_size,
                   image_fetcher::ImageDecodedCallback callback) override {
    gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(
        reinterpret_cast<const uint8_t*>(image_data.data()), image_data.size());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), image));
  }
};

// A helper class that wraps around all the dependencies required to simulate
// signing in/out.
class SigninHelper {
 public:
  explicit SigninHelper() : identity_test_env_(&test_url_loader_factory_) {}

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  void SignIn() {
    std::string email("user@gmail.com");
    identity_test_env_.SetCookieAccounts(
        {{email, signin::GetTestGaiaIdForEmail(email)}});
  }

  void SignOut() {
    identity_test_env_.SetCookieAccounts({});
  }

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
};

class LogoServiceImplTest : public ::testing::Test {
 protected:
  LogoServiceImplTest()
      : template_url_service_(nullptr, 0),
        logo_cache_(new NiceMock<MockLogoCache>()),
        shared_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        use_gray_background_(false) {
    test_url_loader_factory_.SetInterceptor(base::BindRepeating(
        &LogoServiceImplTest::CapturingInterceptor, base::Unretained(this)));

    // Default search engine with logo. All 3P doodle_urls use ddljson API.
    AddSearchEngine("ex", "Logo Example",
                    "https://example.com/?q={searchTerms}",
                    GURL("https://example.com/logo.json"),
                    /*make_default=*/true);

    test_clock_.SetNow(base::Time::FromJsTime(INT64_C(1388686828000)));
    logo_service_ = std::make_unique<LogoServiceImpl>(
        base::FilePath(), signin_helper_.identity_manager(),
        &template_url_service_, std::make_unique<FakeImageDecoder>(),
        shared_factory_,
        base::BindRepeating(&LogoServiceImplTest::use_gray_background,
                            base::Unretained(this)));
    logo_service_->SetClockForTests(&test_clock_);
    logo_service_->SetLogoCacheForTests(base::WrapUnique(logo_cache_));
  }

  void TearDown() override {
    // |logo_service_|'s owns |logo_cache_|, which gets destroyed on
    // a background sequence after the LogoService's destruction. Ensure that
    // |logo_cache_| is actually destroyed before the test ends to make gmock
    // happy.
    logo_service_->Shutdown();
    logo_service_.reset();
    task_environment_.RunUntilIdle();
  }

  // Returns the response that the server would send for the given logo.
  std::string ServerResponse(const Logo& logo);

  // Sets the response to be returned when the LogoService fetches the logo.
  void SetServerResponse(const std::string& response,
                         int error_code = net::OK,
                         net::HttpStatusCode response_code = net::HTTP_OK);

  // Sets the response to be returned when the LogoService fetches the logo and
  // provides the given fingerprint.
  void SetServerResponseWhenFingerprint(
      const std::string& fingerprint,
      const std::string& response_when_fingerprint,
      int error_code = net::OK,
      net::HttpStatusCode response_code = net::HTTP_OK);

  const GURL& DoodleURL() const;

  // Calls logo_service_->GetLogo() and waits for the asynchronous response(s).
  void GetLogo(LogoCallbacks callbacks);
  void GetDecodedLogo(LogoCallback cached, LogoCallback fresh);
  void GetEncodedLogo(EncodedLogoCallback cached, EncodedLogoCallback fresh);

  void AddSearchEngine(base::StringPiece keyword,
                       base::StringPiece short_name,
                       const std::string& url,
                       GURL doodle_url,
                       bool make_default);

  void CapturingInterceptor(const network::ResourceRequest& request);

  bool use_gray_background() const { return use_gray_background_; }

  base::test::TaskEnvironment task_environment_;
  TemplateURLService template_url_service_;
  base::SimpleTestClock test_clock_;
  NiceMock<MockLogoCache>* logo_cache_;

  // Used for mocking |logo_service_| URLs.
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  std::unique_ptr<LogoServiceImpl> logo_service_;

  SigninHelper signin_helper_;

  GURL latest_url_;
  bool use_gray_background_;
};

void LogoServiceImplTest::CapturingInterceptor(
    const network::ResourceRequest& request) {
  latest_url_ = request.url;
}

std::string LogoServiceImplTest::ServerResponse(const Logo& logo) {
  base::TimeDelta time_to_live;
  if (!logo.metadata.expiration_time.is_null())
    time_to_live = logo.metadata.expiration_time - test_clock_.Now();
  return MakeServerResponse(logo, time_to_live);
}

void LogoServiceImplTest::SetServerResponse(const std::string& response,
                                            int error_code,
                                            net::HttpStatusCode response_code) {
  SetServerResponseWhenFingerprint(std::string(), response, error_code,
                                   response_code);
}

void LogoServiceImplTest::SetServerResponseWhenFingerprint(
    const std::string& fingerprint,
    const std::string& response_when_fingerprint,
    int error_code,
    net::HttpStatusCode response_code) {
  GURL url_with_fp = AppendFingerprintParamToDoodleURL(
      AppendPreliminaryParamsToDoodleURL(false, DoodleURL()), fingerprint);

  auto head = network::mojom::URLResponseHead::New();
  std::string headers(base::StringPrintf(
      "HTTP/1.1 %d %s\nContent-type: text/html\n\n",
      static_cast<int>(response_code), GetHttpReasonPhrase(response_code)));
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->mime_type = "text/html";
  network::URLLoaderCompletionStatus status;
  status.error_code = error_code;
  status.decoded_body_length = response_when_fingerprint.size();

  test_url_loader_factory_.AddResponse(url_with_fp, std::move(head),
                                       response_when_fingerprint, status);
}

const GURL& LogoServiceImplTest::DoodleURL() const {
  return template_url_service_.GetDefaultSearchProvider()->doodle_url();
}

void LogoServiceImplTest::GetLogo(LogoCallbacks callbacks) {
  logo_service_->GetLogo(std::move(callbacks));
  task_environment_.RunUntilIdle();
}

void LogoServiceImplTest::GetDecodedLogo(LogoCallback cached,
                                         LogoCallback fresh) {
  LogoCallbacks callbacks;
  callbacks.on_cached_decoded_logo_available = std::move(cached);
  callbacks.on_fresh_decoded_logo_available = std::move(fresh);
  GetLogo(std::move(callbacks));
}

void LogoServiceImplTest::GetEncodedLogo(EncodedLogoCallback cached,
                                         EncodedLogoCallback fresh) {
  LogoCallbacks callbacks;
  callbacks.on_cached_encoded_logo_available = std::move(cached);
  callbacks.on_fresh_encoded_logo_available = std::move(fresh);
  GetLogo(std::move(callbacks));
}

void LogoServiceImplTest::AddSearchEngine(base::StringPiece keyword,
                                          base::StringPiece short_name,
                                          const std::string& url,
                                          GURL doodle_url,
                                          bool make_default) {
  TemplateURLData search_url;
  search_url.SetKeyword(base::ASCIIToUTF16(keyword));
  search_url.SetShortName(base::ASCIIToUTF16(short_name));
  search_url.SetURL(url);
  search_url.doodle_url = doodle_url;

  TemplateURL* template_url =
      template_url_service_.Add(std::make_unique<TemplateURL>(search_url));
  if (make_default) {
    template_url_service_.SetUserSelectedDefaultSearchProvider(template_url);
  }
}

// Tests -----------------------------------------------------------------------

TEST_F(LogoServiceImplTest, CTARequestedBackgroundCanUpdate) {
  std::string response =
      ServerResponse(GetSampleLogo(DoodleURL(), test_clock_.Now()));
  GURL query_with_gray_background = AppendFingerprintParamToDoodleURL(
      AppendPreliminaryParamsToDoodleURL(true, DoodleURL()), std::string());
  GURL query_without_gray_background = AppendFingerprintParamToDoodleURL(
      AppendPreliminaryParamsToDoodleURL(false, DoodleURL()), std::string());

  use_gray_background_ = false;
  test_url_loader_factory_.ClearResponses();
  test_url_loader_factory_.AddResponse(query_without_gray_background.spec(),
                                       response, net::HTTP_OK);
  {
    StrictMock<MockLogoCallback> fresh;
    EXPECT_CALL(fresh, Run(_, _));
    LogoCallbacks callbacks;
    callbacks.on_fresh_decoded_logo_available = fresh.Get();
    logo_service_->GetLogo(std::move(callbacks));
    task_environment_.RunUntilIdle();
  }
  EXPECT_EQ(latest_url_.query().find("graybg:1"), std::string::npos);

  use_gray_background_ = true;
  test_url_loader_factory_.ClearResponses();
  test_url_loader_factory_.AddResponse(query_with_gray_background.spec(),
                                       response, net::HTTP_OK);
  {
    StrictMock<MockLogoCallback> fresh;
    EXPECT_CALL(fresh, Run(_, _));
    LogoCallbacks callbacks;
    callbacks.on_fresh_decoded_logo_available = fresh.Get();
    logo_service_->GetLogo(std::move(callbacks));
    task_environment_.RunUntilIdle();
  }
  EXPECT_NE(latest_url_.query().find("graybg:1"), std::string::npos);
}

TEST_F(LogoServiceImplTest, DownloadAndCacheLogo) {
  StrictMock<MockLogoCallback> cached;
  StrictMock<MockLogoCallback> fresh;
  Logo logo = GetSampleLogo(DoodleURL(), test_clock_.Now());
  SetServerResponse(ServerResponse(logo));
  logo_cache_->ExpectSetCachedLogo(&logo);
  EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(base::nullopt)));
  EXPECT_CALL(fresh, Run(LogoCallbackReason::DETERMINED, Eq(logo)));
  GetDecodedLogo(cached.Get(), fresh.Get());
}

TEST_F(LogoServiceImplTest, DownloadAndCacheLogoWithoutDarkImage) {
  StrictMock<MockLogoCallback> cached;
  StrictMock<MockLogoCallback> fresh;
  Logo logo = GetSampleLogoWithoutDarkImage(DoodleURL(), test_clock_.Now());
  SetServerResponse(ServerResponse(logo));
  logo_cache_->ExpectSetCachedLogo(&logo);
  EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(base::nullopt)));
  EXPECT_CALL(fresh, Run(LogoCallbackReason::DETERMINED, Eq(logo)));
  GetDecodedLogo(cached.Get(), fresh.Get());
}

TEST_F(LogoServiceImplTest, DownloadAndCacheEncodedLogo) {
  StrictMock<MockEncodedLogoCallback> cached;
  StrictMock<MockEncodedLogoCallback> fresh;
  Logo logo = GetSampleLogo(DoodleURL(), test_clock_.Now());
  EncodedLogo encoded_logo = EncodeLogo(logo);
  SetServerResponse(ServerResponse(logo));
  logo_cache_->ExpectSetCachedLogo(&logo);
  EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(base::nullopt)));
  EXPECT_CALL(fresh, Run(LogoCallbackReason::DETERMINED, Eq(encoded_logo)));
  GetEncodedLogo(cached.Get(), fresh.Get());
}

TEST_F(LogoServiceImplTest, DownloadAndCacheEncodedLogoWithoutDarkImage) {
  StrictMock<MockEncodedLogoCallback> cached;
  StrictMock<MockEncodedLogoCallback> fresh;
  Logo logo = GetSampleLogoWithoutDarkImage(DoodleURL(), test_clock_.Now());
  EncodedLogo encoded_logo = EncodeLogo(logo);
  SetServerResponse(ServerResponse(logo));
  logo_cache_->ExpectSetCachedLogo(&logo);
  EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(base::nullopt)));
  EXPECT_CALL(fresh, Run(LogoCallbackReason::DETERMINED, Eq(encoded_logo)));
  GetEncodedLogo(cached.Get(), fresh.Get());
}

TEST_F(LogoServiceImplTest, ShouldReturnDisabledWhenDSEHasNoLogo) {
  AddSearchEngine("cr", "Chromium", "https://www.chromium.org/?q={searchTerms}",
                  GURL(/* logo disabled */), /*make_default=*/true);

  {
    StrictMock<MockLogoCallback> cached;
    StrictMock<MockLogoCallback> fresh;
    EXPECT_CALL(cached, Run(LogoCallbackReason::DISABLED, Eq(base::nullopt)));
    EXPECT_CALL(fresh, Run(LogoCallbackReason::DISABLED, Eq(base::nullopt)));
    GetDecodedLogo(cached.Get(), fresh.Get());
  }

  {
    StrictMock<MockEncodedLogoCallback> cached;
    StrictMock<MockEncodedLogoCallback> fresh;
    EXPECT_CALL(cached, Run(LogoCallbackReason::DISABLED, Eq(base::nullopt)));
    EXPECT_CALL(fresh, Run(LogoCallbackReason::DISABLED, Eq(base::nullopt)));
    GetEncodedLogo(cached.Get(), fresh.Get());
  }
}

TEST_F(LogoServiceImplTest, EmptyCacheAndFailedDownload) {
  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(nullptr)).Times(AnyNumber());

  {
    StrictMock<MockLogoCallback> cached;
    StrictMock<MockLogoCallback> fresh;
    SetServerResponse("server is borked");
    EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(base::nullopt)));
    EXPECT_CALL(fresh, Run(LogoCallbackReason::REVALIDATED, Eq(base::nullopt)));
    GetDecodedLogo(cached.Get(), fresh.Get());
  }

  {
    StrictMock<MockLogoCallback> cached;
    StrictMock<MockLogoCallback> fresh;
    SetServerResponse("", net::ERR_FAILED, net::HTTP_OK);
    EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(base::nullopt)));
    EXPECT_CALL(fresh, Run(LogoCallbackReason::FAILED, Eq(base::nullopt)));
    GetDecodedLogo(cached.Get(), fresh.Get());
  }

  {
    StrictMock<MockLogoCallback> cached;
    StrictMock<MockLogoCallback> fresh;
    SetServerResponse("", net::OK, net::HTTP_BAD_GATEWAY);
    EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(base::nullopt)));
    EXPECT_CALL(fresh, Run(LogoCallbackReason::FAILED, Eq(base::nullopt)));
    GetDecodedLogo(cached.Get(), fresh.Get());
  }
}

TEST_F(LogoServiceImplTest, AcceptMinimalLogoResponse) {
  Logo logo;
  logo.image = MakeBitmap(1, 2);
  logo.metadata.source_url =
      AppendPreliminaryParamsToDoodleURL(false, DoodleURL());
  logo.metadata.can_show_after_expiration = true;
  logo.metadata.mime_type = "image/png";

  std::string response =
      ")]}' {\"ddljson\":{\"data_uri\":\"data:image/png;base64," +
      EncodeBitmapAsPNGBase64(logo.image) + "\"}}";

  SetServerResponse(response);

  StrictMock<MockLogoCallback> cached;
  StrictMock<MockLogoCallback> fresh;
  EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(base::nullopt)));
  EXPECT_CALL(fresh, Run(LogoCallbackReason::DETERMINED, Eq(logo)));

  GetDecodedLogo(cached.Get(), fresh.Get());
}

TEST_F(LogoServiceImplTest, ReturnCachedLogo) {
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_.Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);
  SetServerResponseWhenFingerprint(cached_logo.metadata.fingerprint, "",
                                   net::ERR_FAILED, net::HTTP_OK);

  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));

  StrictMock<MockLogoCallback> cached;
  StrictMock<MockLogoCallback> fresh;
  EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(cached_logo)));
  EXPECT_CALL(fresh, Run(LogoCallbackReason::FAILED, Eq(base::nullopt)));

  GetDecodedLogo(cached.Get(), fresh.Get());
}

TEST_F(LogoServiceImplTest, ValidateCachedLogo) {
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_.Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  // During revalidation, the image data and mime_type are absent.
  Logo fresh_logo = cached_logo;
  fresh_logo.image.reset();
  fresh_logo.dark_image.reset();
  fresh_logo.metadata.mime_type.clear();
  fresh_logo.metadata.dark_mime_type.clear();
  fresh_logo.metadata.expiration_time =
      test_clock_.Now() + base::TimeDelta::FromDays(8);
  SetServerResponseWhenFingerprint(fresh_logo.metadata.fingerprint,
                                   ServerResponse(fresh_logo));

  {
    EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(1);
    EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
    EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));

    StrictMock<MockLogoCallback> cached;
    StrictMock<MockLogoCallback> fresh;
    EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(cached_logo)));
    EXPECT_CALL(fresh, Run(LogoCallbackReason::REVALIDATED, Eq(base::nullopt)));
    GetDecodedLogo(cached.Get(), fresh.Get());
  }

  ASSERT_TRUE(logo_cache_->GetCachedLogoMetadata());
  EXPECT_EQ(fresh_logo.metadata.expiration_time,
            logo_cache_->GetCachedLogoMetadata()->expiration_time);

  {
    // Ensure that cached logo is still returned correctly on subsequent
    // requests. In particular, the metadata should stay valid.
    // https://crbug.com/480090
    EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(1);
    EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
    EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));

    StrictMock<MockLogoCallback> cached;
    StrictMock<MockLogoCallback> fresh;
    EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(cached_logo)));
    EXPECT_CALL(fresh, Run(LogoCallbackReason::REVALIDATED, Eq(base::nullopt)));
    GetDecodedLogo(cached.Get(), fresh.Get());
  }
}

TEST_F(LogoServiceImplTest, UpdateCachedLogoMetadata) {
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_.Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  Logo fresh_logo = cached_logo;
  fresh_logo.image.reset();
  fresh_logo.metadata.mime_type.clear();
  fresh_logo.metadata.on_click_url = GURL("https://new.onclick.url");
  fresh_logo.metadata.alt_text = "new alt text";
  fresh_logo.metadata.animated_url = GURL("https://new.animated.url");
  fresh_logo.metadata.expiration_time =
      test_clock_.Now() + base::TimeDelta::FromDays(8);
  SetServerResponseWhenFingerprint(fresh_logo.metadata.fingerprint,
                                   ServerResponse(fresh_logo));

  // On the first request, the cached logo should be used.
  {
    StrictMock<MockLogoCallback> cached;
    StrictMock<MockLogoCallback> fresh;
    EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(cached_logo)));
    EXPECT_CALL(fresh, Run(LogoCallbackReason::REVALIDATED, Eq(base::nullopt)));
    GetDecodedLogo(cached.Get(), fresh.Get());
  }

  // Subsequently, the cached image should be returned along with the updated
  // metadata.
  {
    Logo expected_logo = fresh_logo;
    expected_logo.image = cached_logo.image;
    expected_logo.metadata.mime_type = cached_logo.metadata.mime_type;
    StrictMock<MockLogoCallback> cached;
    StrictMock<MockLogoCallback> fresh;
    EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(expected_logo)));
    EXPECT_CALL(fresh, Run(LogoCallbackReason::REVALIDATED, Eq(base::nullopt)));
    GetDecodedLogo(cached.Get(), fresh.Get());
  }
}

TEST_F(LogoServiceImplTest, UpdateCachedLogo) {
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_.Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  Logo fresh_logo = GetSampleLogo2(DoodleURL(), test_clock_.Now());
  SetServerResponseWhenFingerprint(cached_logo.metadata.fingerprint,
                                   ServerResponse(fresh_logo));

  logo_cache_->ExpectSetCachedLogo(&fresh_logo);
  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));

  StrictMock<MockLogoCallback> cached;
  StrictMock<MockLogoCallback> fresh;
  EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(cached_logo)));
  EXPECT_CALL(fresh, Run(LogoCallbackReason::DETERMINED, Eq(fresh_logo)));
  GetDecodedLogo(cached.Get(), fresh.Get());
}

TEST_F(LogoServiceImplTest, InvalidateCachedLogo) {
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_.Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  // This response means there's no current logo.
  SetServerResponseWhenFingerprint(cached_logo.metadata.fingerprint,
                                   ")]}' {\"update\":{}}");

  logo_cache_->ExpectSetCachedLogo(nullptr);
  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));

  StrictMock<MockLogoCallback> cached;
  StrictMock<MockLogoCallback> fresh;
  EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(cached_logo)));
  EXPECT_CALL(fresh, Run(LogoCallbackReason::DETERMINED, Eq(base::nullopt)));

  GetDecodedLogo(cached.Get(), fresh.Get());
}

TEST_F(LogoServiceImplTest, DeleteCachedLogoFromOldUrl) {
  SetServerResponse("", net::ERR_FAILED, net::HTTP_OK);
  Logo cached_logo =
      GetSampleLogo(GURL("https://oldsearchprovider.com"), test_clock_.Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(nullptr)).Times(AnyNumber());
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));

  StrictMock<MockLogoCallback> cached;
  StrictMock<MockLogoCallback> fresh;
  EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(base::nullopt)));
  EXPECT_CALL(fresh, Run(LogoCallbackReason::FAILED, Eq(base::nullopt)));

  GetDecodedLogo(cached.Get(), fresh.Get());
}

TEST_F(LogoServiceImplTest, LogoWithTTLCannotBeShownAfterExpiration) {
  Logo logo = GetSampleLogo(DoodleURL(), test_clock_.Now());
  base::TimeDelta time_to_live = base::TimeDelta::FromDays(3);
  logo.metadata.expiration_time = test_clock_.Now() + time_to_live;
  SetServerResponse(ServerResponse(logo));
  LogoCallbacks callbacks;
  callbacks.on_fresh_decoded_logo_available = base::Bind(
      [](LogoCallbackReason type, const base::Optional<Logo>& logo) {});
  GetLogo(std::move(callbacks));

  const LogoMetadata* cached_metadata = logo_cache_->GetCachedLogoMetadata();
  ASSERT_TRUE(cached_metadata);
  EXPECT_FALSE(cached_metadata->can_show_after_expiration);
  EXPECT_EQ(test_clock_.Now() + time_to_live, cached_metadata->expiration_time);
}

TEST_F(LogoServiceImplTest, LogoWithoutTTLCanBeShownAfterExpiration) {
  Logo logo = GetSampleLogo(DoodleURL(), test_clock_.Now());
  base::TimeDelta time_to_live = base::TimeDelta();
  SetServerResponse(MakeServerResponse(logo, time_to_live));
  LogoCallbacks callbacks;
  callbacks.on_fresh_decoded_logo_available = base::Bind(
      [](LogoCallbackReason type, const base::Optional<Logo>& logo) {});
  GetLogo(std::move(callbacks));

  const LogoMetadata* cached_metadata = logo_cache_->GetCachedLogoMetadata();
  ASSERT_TRUE(cached_metadata);
  EXPECT_TRUE(cached_metadata->can_show_after_expiration);
  EXPECT_EQ(test_clock_.Now() + base::TimeDelta::FromDays(30),
            cached_metadata->expiration_time);
}

TEST_F(LogoServiceImplTest, UseSoftExpiredCachedLogo) {
  SetServerResponse("", net::ERR_FAILED, net::HTTP_OK);
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_.Now());
  cached_logo.metadata.expiration_time =
      test_clock_.Now() - base::TimeDelta::FromSeconds(1);
  cached_logo.metadata.can_show_after_expiration = true;
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));

  StrictMock<MockLogoCallback> cached;
  StrictMock<MockLogoCallback> fresh;
  EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(cached_logo)));
  EXPECT_CALL(fresh, Run(LogoCallbackReason::FAILED, Eq(base::nullopt)));

  GetDecodedLogo(cached.Get(), fresh.Get());
}

TEST_F(LogoServiceImplTest, RerequestSoftExpiredCachedLogo) {
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_.Now());
  cached_logo.metadata.expiration_time =
      test_clock_.Now() - base::TimeDelta::FromDays(5);
  cached_logo.metadata.can_show_after_expiration = true;
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  Logo fresh_logo = GetSampleLogo2(DoodleURL(), test_clock_.Now());
  SetServerResponse(ServerResponse(fresh_logo));

  logo_cache_->ExpectSetCachedLogo(&fresh_logo);
  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));

  StrictMock<MockLogoCallback> cached;
  StrictMock<MockLogoCallback> fresh;
  EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(cached_logo)));
  EXPECT_CALL(fresh, Run(LogoCallbackReason::DETERMINED, Eq(fresh_logo)));

  GetDecodedLogo(cached.Get(), fresh.Get());
}

TEST_F(LogoServiceImplTest, DeleteAncientCachedLogo) {
  SetServerResponse("", net::ERR_FAILED, net::HTTP_OK);
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_.Now());
  cached_logo.metadata.expiration_time =
      test_clock_.Now() - base::TimeDelta::FromDays(200);
  cached_logo.metadata.can_show_after_expiration = true;
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(nullptr)).Times(AnyNumber());
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));

  StrictMock<MockLogoCallback> cached;
  StrictMock<MockLogoCallback> fresh;
  EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(base::nullopt)));
  EXPECT_CALL(fresh, Run(LogoCallbackReason::FAILED, Eq(base::nullopt)));

  GetDecodedLogo(cached.Get(), fresh.Get());
}

TEST_F(LogoServiceImplTest, DeleteExpiredCachedLogo) {
  SetServerResponse("", net::ERR_FAILED, net::HTTP_OK);
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_.Now());
  cached_logo.metadata.expiration_time =
      test_clock_.Now() - base::TimeDelta::FromSeconds(1);
  cached_logo.metadata.can_show_after_expiration = false;
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(nullptr)).Times(AnyNumber());
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));

  StrictMock<MockLogoCallback> cached;
  StrictMock<MockLogoCallback> fresh;
  EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(base::nullopt)));
  EXPECT_CALL(fresh, Run(LogoCallbackReason::FAILED, Eq(base::nullopt)));

  GetDecodedLogo(cached.Get(), fresh.Get());
}

TEST_F(LogoServiceImplTest, ClearLogoOnSignOut) {
  // Sign in and setup a logo response.
  signin_helper_.SignIn();
  // |SetCachedLogo(nullptr)| task might not have run.
  task_environment_.RunUntilIdle();
  Logo logo = GetSampleLogo(DoodleURL(), test_clock_.Now());
  SetServerResponse(ServerResponse(logo));

  // Request the logo so it gets fetched and cached.
  logo_cache_->ExpectSetCachedLogo(&logo);
  StrictMock<MockLogoCallback> cached;
  StrictMock<MockLogoCallback> fresh;
  EXPECT_CALL(cached, Run(LogoCallbackReason::DETERMINED, Eq(base::nullopt)));
  EXPECT_CALL(fresh, Run(LogoCallbackReason::DETERMINED, Eq(logo)));
  GetDecodedLogo(cached.Get(), fresh.Get());

  // Signing out should clear the cached logo immediately.
  logo_cache_->ExpectSetCachedLogo(nullptr);
  signin_helper_.SignOut();
}

// Tests that deal with multiple listeners.

void EnqueueCallbacks(LogoServiceImpl* logo_service,
                      std::vector<LogoCallback>* cached_callbacks,
                      std::vector<LogoCallback>* fresh_callbacks,
                      size_t start_index) {
  DCHECK_EQ(cached_callbacks->size(), fresh_callbacks->size());
  if (start_index >= cached_callbacks->size())
    return;

  LogoCallbacks callbacks;
  callbacks.on_cached_decoded_logo_available =
      std::move((*cached_callbacks)[start_index]);
  callbacks.on_fresh_decoded_logo_available =
      std::move((*fresh_callbacks)[start_index]);
  logo_service->GetLogo(std::move(callbacks));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EnqueueCallbacks, logo_service, cached_callbacks,
                     fresh_callbacks, start_index + 1));
}

TEST_F(LogoServiceImplTest, SupportOverlappingLogoRequests) {
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_.Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);
  ON_CALL(*logo_cache_, SetCachedLogo(_)).WillByDefault(Return());

  Logo fresh_logo = GetSampleLogo2(DoodleURL(), test_clock_.Now());
  std::string response = ServerResponse(fresh_logo);
  SetServerResponse(response);
  SetServerResponseWhenFingerprint(cached_logo.metadata.fingerprint, response);

  const int kNumListeners = 10;
  std::vector<std::unique_ptr<MockLogoCallback>> mocks;
  std::vector<LogoCallback> cached_callbacks;
  std::vector<LogoCallback> fresh_callbacks;
  for (int i = 0; i < kNumListeners; ++i) {
    mocks.push_back(std::make_unique<MockLogoCallback>());
    EXPECT_CALL(*mocks.back(),
                Run(LogoCallbackReason::DETERMINED, Eq(cached_logo)));
    cached_callbacks.push_back(mocks.back()->Get());

    mocks.push_back(std::make_unique<MockLogoCallback>());
    EXPECT_CALL(*mocks.back(),
                Run(LogoCallbackReason::DETERMINED, Eq(fresh_logo)));
    fresh_callbacks.push_back(mocks.back()->Get());
  }
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(AtMost(3));
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(3));

  EnqueueCallbacks(logo_service_.get(), &cached_callbacks, &fresh_callbacks, 0);

  task_environment_.RunUntilIdle();
}

TEST_F(LogoServiceImplTest, DeleteCallbacksWhenLogoURLChanged) {
  StrictMock<MockLogoCallback> first_cached;
  StrictMock<MockLogoCallback> first_fresh;
  EXPECT_CALL(first_cached,
              Run(LogoCallbackReason::CANCELED, Eq(base::nullopt)));
  EXPECT_CALL(first_fresh,
              Run(LogoCallbackReason::CANCELED, Eq(base::nullopt)));
  LogoCallbacks first_callbacks;
  first_callbacks.on_cached_decoded_logo_available = first_cached.Get();
  first_callbacks.on_fresh_decoded_logo_available = first_fresh.Get();
  logo_service_->GetLogo(std::move(first_callbacks));

  // Change default search engine; new DSE has a doodle URL.
  AddSearchEngine("cr", "Chromium", "https://www.chromium.org/?q={searchTerms}",
                  GURL("https://chromium.org/logo.json"),
                  /*make_default=*/true);

  Logo logo = GetSampleLogo(DoodleURL(), test_clock_.Now());
  SetServerResponse(ServerResponse(logo));

  StrictMock<MockLogoCallback> second_cached;
  StrictMock<MockLogoCallback> second_fresh;
  EXPECT_CALL(second_cached,
              Run(LogoCallbackReason::DETERMINED, Eq(base::nullopt)));
  EXPECT_CALL(second_fresh, Run(LogoCallbackReason::DETERMINED, Eq(logo)));
  LogoCallbacks second_callbacks;
  second_callbacks.on_cached_decoded_logo_available = second_cached.Get();
  second_callbacks.on_fresh_decoded_logo_available = second_fresh.Get();
  logo_service_->GetLogo(std::move(second_callbacks));

  task_environment_.RunUntilIdle();
}

}  // namespace

bool operator==(const Logo& a, const Logo& b) {
  return (a.image.width() == b.image.width()) &&
         (a.image.height() == b.image.height()) &&
         (a.dark_image.width() == b.dark_image.width()) &&
         (a.dark_image.height() == b.dark_image.height()) &&
         (a.metadata.on_click_url == b.metadata.on_click_url) &&
         (a.metadata.source_url == b.metadata.source_url) &&
         (a.metadata.animated_url == b.metadata.animated_url) &&
         (a.metadata.dark_animated_url == b.metadata.dark_animated_url) &&
         (a.metadata.alt_text == b.metadata.alt_text) &&
         (a.metadata.mime_type == b.metadata.mime_type) &&
         (a.metadata.dark_mime_type == b.metadata.dark_mime_type) &&
         (a.metadata.fingerprint == b.metadata.fingerprint) &&
         (a.metadata.can_show_after_expiration ==
          b.metadata.can_show_after_expiration);
}

bool operator==(const EncodedLogo& a, const EncodedLogo& b) {
  return DecodeLogo(a) == DecodeLogo(b);
}

void PrintTo(const Logo& logo, std::ostream* ostr) {
  *ostr << "image size: " << logo.image.width() << "x" << logo.image.height()
        << "\non_click_url: " << logo.metadata.on_click_url
        << "\nsource_url: " << logo.metadata.source_url
        << "\nanimated_url: " << logo.metadata.animated_url
        << "\nalt_text: " << logo.metadata.alt_text
        << "\nmime_type: " << logo.metadata.mime_type
        << "\nfingerprint: " << logo.metadata.fingerprint
        << "\ncan_show_after_expiration: "
        << logo.metadata.can_show_after_expiration;
}

void PrintTo(const EncodedLogo& logo, std::ostream* ostr) {
  PrintTo(DecodeLogo(logo), ostr);
}

}  // namespace search_provider_logos
