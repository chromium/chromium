// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/favicon_source.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/history_ui_favicon_request_handler_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/favicon/core/history_ui_favicon_request_handler.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/test_native_theme.h"
#include "ui/resources/grit/ui_resources.h"

using GotDataCallback = content::URLDataSource::GotDataCallback;
using WebContentsGetter = content::WebContents::Getter;
using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnArg;

namespace {

const int kDummyTaskId = 1;
const char kDummyPrefix[] = "chrome://any-host/";

}  // namespace

class MockHistoryUiFaviconRequestHandler
    : public favicon::HistoryUiFaviconRequestHandler {
 public:
  MockHistoryUiFaviconRequestHandler() = default;
  ~MockHistoryUiFaviconRequestHandler() override = default;

  MOCK_METHOD4(
      GetRawFaviconForPageURL,
      void(const GURL& page_url,
           int desired_size_in_pixel,
           favicon_base::FaviconRawBitmapCallback callback,
           favicon::HistoryUiFaviconRequestOrigin request_origin_for_uma));

  MOCK_METHOD3(
      GetFaviconImageForPageURL,
      void(const GURL& page_url,
           favicon_base::FaviconImageCallback callback,
           favicon::HistoryUiFaviconRequestOrigin request_origin_for_uma));
};

class TestFaviconSource : public FaviconSource {
 public:
  TestFaviconSource(chrome::FaviconUrlFormat format,
                    Profile* profile,
                    ui::NativeTheme* theme)
      : FaviconSource(profile, format), theme_(theme) {}

  ~TestFaviconSource() override {}

  MOCK_METHOD(base::RefCountedMemory*, LoadIconBytes, (float, int));

 protected:
  // FaviconSource:
  ui::NativeTheme* GetNativeTheme(
      const content::WebContents::Getter& wc_getter) override {
    return theme_;
  }

 private:
  const raw_ptr<ui::NativeTheme> theme_;
};

class FaviconSourceTestBase : public testing::Test {
 public:
  explicit FaviconSourceTestBase(chrome::FaviconUrlFormat format)
      : source_(format, &profile_, &theme_) {
    // Setup testing factories for main dependencies.
    mock_history_ui_favicon_request_handler_ =
        static_cast<NiceMock<MockHistoryUiFaviconRequestHandler>*>(
            HistoryUiFaviconRequestHandlerFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    &profile_, base::BindOnce([](content::BrowserContext*) {
                      return base::WrapUnique<KeyedService>(
                          new NiceMock<MockHistoryUiFaviconRequestHandler>());
                    })));
    mock_favicon_service_ = static_cast<NiceMock<favicon::MockFaviconService>*>(
        FaviconServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_, base::BindOnce([](content::BrowserContext*) {
              return static_cast<std::unique_ptr<KeyedService>>(
                  std::make_unique<NiceMock<favicon::MockFaviconService>>());
            })));

    // Setup TestWebContents.
    test_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    test_web_contents_getter_ = base::BindLambdaForTesting(
        [&] { return (content::WebContents*)test_web_contents_.get(); });

    // On call, dependencies will return empty favicon by default.
    ON_CALL(*mock_favicon_service_, GetRawFaviconForPageURL(_, _, _, _, _, _))
        .WillByDefault([](auto, auto, auto, auto,
                          favicon_base::FaviconRawBitmapCallback callback,
                          auto) {
          std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
          return kDummyTaskId;
        });
    ON_CALL(*mock_history_ui_favicon_request_handler_,
            GetRawFaviconForPageURL(_, _, _, _))
        .WillByDefault([](auto, auto,
                          favicon_base::FaviconRawBitmapCallback callback,
                          auto) {
          std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
        });

    // Mock default icon loading.
    ON_CALL(*source(), LoadIconBytes(_, _))
        .WillByDefault(Return(kDummyIconBytes.get()));
  }

  void SetDarkMode(bool dark_mode) { theme_.SetDarkMode(dark_mode); }

  NiceMock<TestFaviconSource>* source() { return &source_; }

 protected:
  const scoped_refptr<base::RefCountedBytes> kDummyIconBytes;
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  ui::TestNativeTheme theme_;
  TestingProfile profile_;
  raw_ptr<NiceMock<MockHistoryUiFaviconRequestHandler>>
      mock_history_ui_favicon_request_handler_;
  raw_ptr<NiceMock<favicon::MockFaviconService>> mock_favicon_service_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  WebContentsGetter test_web_contents_getter_;
  NiceMock<TestFaviconSource> source_;
};

class FaviconSourceTestWithLegacyFormat : public FaviconSourceTestBase {
 public:
  FaviconSourceTestWithLegacyFormat()
      : FaviconSourceTestBase(chrome::FaviconUrlFormat::kFaviconLegacy) {}
};

class FaviconSourceTestWithFavicon2Format : public FaviconSourceTestBase {
 public:
  FaviconSourceTestWithFavicon2Format()
      : FaviconSourceTestBase(chrome::FaviconUrlFormat::kFavicon2) {}
};

TEST_F(FaviconSourceTestWithLegacyFormat, DarkDefault) {
  SetDarkMode(true);
  EXPECT_CALL(*source(), LoadIconBytes(_, IDR_DEFAULT_FAVICON_DARK));
  source()->StartDataRequest(GURL(kDummyPrefix), test_web_contents_getter_,
                             base::DoNothing());
}

TEST_F(FaviconSourceTestWithLegacyFormat, LightDefault) {
  SetDarkMode(false);
  EXPECT_CALL(*source(), LoadIconBytes(_, IDR_DEFAULT_FAVICON));
  source()->StartDataRequest(GURL(kDummyPrefix), test_web_contents_getter_,
                             base::DoNothing());
}

TEST_F(FaviconSourceTestWithLegacyFormat,
       ShouldNotQueryHistoryUiFaviconRequestHandler) {
  content::WebContentsTester::For(test_web_contents_.get())
      ->SetLastCommittedURL(GURL(chrome::kChromeUIHistoryURL));

  EXPECT_CALL(*mock_history_ui_favicon_request_handler_,
              GetRawFaviconForPageURL)
      .Times(0);

  source()->StartDataRequest(
      GURL(base::StrCat({kDummyPrefix, "size/16@1x/https://www.google.com"})),
      test_web_contents_getter_, base::DoNothing());
}

TEST_F(FaviconSourceTestWithLegacyFormat, ShouldNotQueryIfDesiredSizeTooLarge) {
  EXPECT_CALL(*mock_history_ui_favicon_request_handler_,
              GetRawFaviconForPageURL)
      .Times(0);
  EXPECT_CALL(*mock_favicon_service_, GetRawFavicon).Times(0);
  EXPECT_CALL(*mock_favicon_service_, GetRawFaviconForPageURL).Times(0);

  // 1000x scale factor runs into the max cap.
  source()->StartDataRequest(
      GURL(
          base::StrCat({kDummyPrefix, "size/16@1000x/https://www.google.com"})),
      test_web_contents_getter_, base::DoNothing());
}

TEST_F(FaviconSourceTestWithLegacyFormat, ShouldNotQueryIfInvalidScaleFactor) {
  EXPECT_CALL(*mock_history_ui_favicon_request_handler_,
              GetRawFaviconForPageURL)
      .Times(0);
  EXPECT_CALL(*mock_favicon_service_, GetRawFavicon).Times(0);
  EXPECT_CALL(*mock_favicon_service_, GetRawFaviconForPageURL).Times(0);

  // A negative scale factor cannot be parsed.
  source()->StartDataRequest(
      GURL(base::StrCat({kDummyPrefix, "size/16@-2x/https://www.google.com"})),
      test_web_contents_getter_, base::DoNothing());
}

TEST_F(FaviconSourceTestWithFavicon2Format,
       ShouldNotRecordFaviconResourceHistogram) {
  base::HistogramTester tester;
  source()->StartDataRequest(
      GURL(base::StrCat({kDummyPrefix, "size/16@1x/https://www.google.com"})),
      test_web_contents_getter_, base::DoNothing());
  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation(
          "Extensions.FaviconResourceUsed"));
  EXPECT_TRUE(samples);
  EXPECT_EQ(0, samples->TotalCount());
}

TEST_F(FaviconSourceTestWithFavicon2Format, DarkDefault) {
  SetDarkMode(true);
  EXPECT_CALL(*source(), LoadIconBytes(_, IDR_DEFAULT_FAVICON_DARK));
  source()->StartDataRequest(GURL(kDummyPrefix), test_web_contents_getter_,
                             base::DoNothing());
}

TEST_F(FaviconSourceTestWithFavicon2Format, LightDefault) {
  SetDarkMode(false);
  EXPECT_CALL(*source(), LoadIconBytes(_, IDR_DEFAULT_FAVICON));
  source()->StartDataRequest(GURL(kDummyPrefix), test_web_contents_getter_,
                             base::DoNothing());
}

TEST_F(FaviconSourceTestWithFavicon2Format, LightOverride) {
  SetDarkMode(true);
  EXPECT_CALL(*source(), LoadIconBytes(_, IDR_DEFAULT_FAVICON));
  source()->StartDataRequest(
      GURL(base::StrCat({kDummyPrefix,
                         "?pageUrl=https%3A%2F%2Fwww.google.com"
                         "&forceLightMode"})),
      test_web_contents_getter_, base::DoNothing());
}

TEST_F(FaviconSourceTestWithFavicon2Format,
       ShouldNotQueryHistoryUiFaviconRequestHandlerIfNotAllowed) {
  content::WebContentsTester::For(test_web_contents_.get())
      ->SetLastCommittedURL(GURL(chrome::kChromeUIHistoryURL));

  EXPECT_CALL(*mock_history_ui_favicon_request_handler_,
              GetRawFaviconForPageURL)
      .Times(0);

  source()->StartDataRequest(
      GURL(base::StrCat(
          {kDummyPrefix,
           "?size=16&scaleFactor=1x&pageUrl=https%3A%2F%2Fwww.google."
           "com&allowGoogleServerFallback=0"})),
      test_web_contents_getter_, base::DoNothing());
}

TEST_F(FaviconSourceTestWithFavicon2Format,
       ShouldNotQueryHistoryUiFaviconRequestHandlerIfHasNotHistoryUiOrigin) {
  content::WebContentsTester::For(test_web_contents_.get())
      ->SetLastCommittedURL(GURL("chrome://non-history-url"));

  EXPECT_CALL(*mock_history_ui_favicon_request_handler_,
              GetRawFaviconForPageURL)
      .Times(0);

  source()->StartDataRequest(
      GURL(base::StrCat(
          {kDummyPrefix,
           "?size=16&scaleFactor=1x&pageUrl=https%3A%2F%2Fwww.google."
           "com&allowGoogleServerFallback=1"})),
      test_web_contents_getter_, base::DoNothing());
}

TEST_F(
    FaviconSourceTestWithFavicon2Format,
    ShouldQueryHistoryUiFaviconRequestHandlerIfHasHistoryUiOriginAndAllowed) {
  content::WebContentsTester::For(test_web_contents_.get())
      ->SetLastCommittedURL(GURL(chrome::kChromeUIHistoryURL));

  EXPECT_CALL(*mock_history_ui_favicon_request_handler_,
              GetRawFaviconForPageURL(GURL("https://www.google.com"), _, _, _))
      .Times(1);

  source()->StartDataRequest(
      GURL(base::StrCat(
          {kDummyPrefix,
           "?size=16&scaleFactor=1x&pageUrl=https%3A%2F%2Fwww.google."
           "com&allowGoogleServerFallback=1"})),
      test_web_contents_getter_, base::DoNothing());
}

TEST_F(FaviconSourceTestWithFavicon2Format,
       ShouldNotQueryIfDesiredSizeTooLarge) {
  EXPECT_CALL(*mock_history_ui_favicon_request_handler_,
              GetRawFaviconForPageURL)
      .Times(0);
  EXPECT_CALL(*mock_favicon_service_, GetRawFavicon).Times(0);
  EXPECT_CALL(*mock_favicon_service_, GetRawFaviconForPageURL).Times(0);

  // 1000x scale factor runs into the max cap.
  source()->StartDataRequest(
      GURL(base::StrCat(
          {kDummyPrefix,
           "?size=16&scaleFactor=1000x&pageUrl=https%3A%2F%2Fwww.google.com"})),
      test_web_contents_getter_, base::DoNothing());
}

TEST_F(FaviconSourceTestWithFavicon2Format,
       ShouldNotQueryIfInvalidScaleFactor) {
  EXPECT_CALL(*mock_history_ui_favicon_request_handler_,
              GetRawFaviconForPageURL)
      .Times(0);
  EXPECT_CALL(*mock_favicon_service_, GetRawFavicon).Times(0);
  EXPECT_CALL(*mock_favicon_service_, GetRawFaviconForPageURL).Times(0);

  // A negative scale factor cannot be parsed.
  source()->StartDataRequest(
      GURL(base::StrCat(
          {kDummyPrefix,
           "?size=16&scaleFactor=-2x&pageUrl=https%3A%2F%2Fwww.google.com"})),
      test_web_contents_getter_, base::DoNothing());
}
