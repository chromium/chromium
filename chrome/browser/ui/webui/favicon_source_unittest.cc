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
#include "ui/native_theme/mock_os_settings_provider.h"
#include "ui/native_theme/native_theme.h"
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

  MOCK_METHOD4(GetRawFaviconForPageURL,
               void(const GURL& page_url,
                    int desired_size_in_pixel,
                    bool fallback_to_host,
                    favicon_base::FaviconRawBitmapCallback callback));

  MOCK_METHOD2(GetFaviconImageForPageURL,
               void(const GURL& page_url,
                    favicon_base::FaviconImageCallback callback));
};

class TestFaviconSource : public FaviconSource {
 public:
  using FaviconSource::FaviconSource;

  MOCK_METHOD(base::RefCountedMemory*, LoadIconBytes, (float, int));
};

class FaviconSourceTestBase : public testing::Test {
 public:
  explicit FaviconSourceTestBase(chrome::FaviconUrlFormat format,
                                 bool serve_untrusted = false)
      : source_(&profile_, format, serve_untrusted) {
    Init();
  }

  void Init() {
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
    test_web_contents_getter_ =
        base::BindLambdaForTesting([&] { return test_web_contents(); });

    // On call, dependencies will return empty favicon by default.
    ON_CALL(mock_favicon_service(), GetRawFaviconForPageURL(_, _, _, _, _, _))
        .WillByDefault([](auto, auto, auto, auto,
                          favicon_base::FaviconRawBitmapCallback callback,
                          auto) {
          std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
          return kDummyTaskId;
        });
    ON_CALL(mock_history_ui_favicon_request_handler(), GetRawFaviconForPageURL)
        .WillByDefault([](auto, auto, auto,
                          favicon_base::FaviconRawBitmapCallback callback) {
          std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
        });

    // Mock default icon loading.
    ON_CALL(source(), LoadIconBytes(_, _))
        .WillByDefault(Return(dummy_icon_bytes_.get()));
  }

 protected:
  ui::MockOsSettingsProvider& os_settings_provider() {
    return os_settings_provider_;
  }

  content::WebContents* test_web_contents() { return test_web_contents_.get(); }

  const WebContentsGetter& test_web_contents_getter() const {
    return test_web_contents_getter_;
  }

  NiceMock<favicon::MockFaviconService>& mock_favicon_service() {
    return *mock_favicon_service_;
  }

  NiceMock<MockHistoryUiFaviconRequestHandler>&
  mock_history_ui_favicon_request_handler() {
    return *mock_history_ui_favicon_request_handler_;
  }

  NiceMock<TestFaviconSource>& source() { return source_; }

 private:
  ui::MockOsSettingsProvider os_settings_provider_;
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  WebContentsGetter test_web_contents_getter_;
  raw_ptr<NiceMock<favicon::MockFaviconService>> mock_favicon_service_;
  raw_ptr<NiceMock<MockHistoryUiFaviconRequestHandler>>
      mock_history_ui_favicon_request_handler_;
  NiceMock<TestFaviconSource> source_;
  scoped_refptr<base::RefCountedBytes> dummy_icon_bytes_;
};

class FaviconSourceTestWithLegacyFormat : public FaviconSourceTestBase {
 public:
  FaviconSourceTestWithLegacyFormat()
      : FaviconSourceTestBase(chrome::FaviconUrlFormat::kFaviconLegacy) {}
};

TEST_F(FaviconSourceTestWithLegacyFormat, DarkDefault) {
  os_settings_provider().SetPreferredColorScheme(
      ui::NativeTheme::PreferredColorScheme::kDark);
  EXPECT_CALL(source(), LoadIconBytes(_, IDR_DEFAULT_FAVICON_DARK));
  source().StartDataRequest(GURL(kDummyPrefix), test_web_contents_getter(),
                            base::DoNothing());
}

TEST_F(FaviconSourceTestWithLegacyFormat, LightDefault) {
  EXPECT_CALL(source(), LoadIconBytes(_, IDR_DEFAULT_FAVICON));
  source().StartDataRequest(GURL(kDummyPrefix), test_web_contents_getter(),
                            base::DoNothing());
}

TEST_F(FaviconSourceTestWithLegacyFormat,
       ShouldNotQueryHistoryUiFaviconRequestHandler) {
  content::WebContentsTester::For(test_web_contents())
      ->SetLastCommittedURL(GURL(chrome::kChromeUIHistoryURL));

  EXPECT_CALL(mock_history_ui_favicon_request_handler(),
              GetRawFaviconForPageURL)
      .Times(0);

  source().StartDataRequest(
      GURL(base::StrCat({kDummyPrefix, "size/16@1x/https://www.google.com"})),
      test_web_contents_getter(), base::DoNothing());
}

TEST_F(FaviconSourceTestWithLegacyFormat, ShouldNotQueryIfDesiredSizeTooLarge) {
  EXPECT_CALL(mock_history_ui_favicon_request_handler(),
              GetRawFaviconForPageURL)
      .Times(0);
  EXPECT_CALL(mock_favicon_service(), GetRawFavicon).Times(0);
  EXPECT_CALL(mock_favicon_service(), GetRawFaviconForPageURL).Times(0);

  // 1000x scale factor runs into the max cap.
  source().StartDataRequest(
      GURL(
          base::StrCat({kDummyPrefix, "size/16@1000x/https://www.google.com"})),
      test_web_contents_getter(), base::DoNothing());
}

TEST_F(FaviconSourceTestWithLegacyFormat, ShouldNotQueryIfInvalidScaleFactor) {
  EXPECT_CALL(mock_history_ui_favicon_request_handler(),
              GetRawFaviconForPageURL)
      .Times(0);
  EXPECT_CALL(mock_favicon_service(), GetRawFavicon).Times(0);
  EXPECT_CALL(mock_favicon_service(), GetRawFaviconForPageURL).Times(0);

  // A negative scale factor cannot be parsed.
  source().StartDataRequest(
      GURL(base::StrCat({kDummyPrefix, "size/16@-2x/https://www.google.com"})),
      test_web_contents_getter(), base::DoNothing());
}

class FaviconSourceTestWithFavicon2Format
    : public FaviconSourceTestBase,
      public testing::WithParamInterface<bool> {
 public:
  FaviconSourceTestWithFavicon2Format()
      : FaviconSourceTestBase(chrome::FaviconUrlFormat::kFavicon2,
                              /*serve_untrusted=*/GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(,
                         FaviconSourceTestWithFavicon2Format,
                         /*serve_untrusted=*/testing::Bool());

TEST_P(FaviconSourceTestWithFavicon2Format,
       ShouldNotRecordFaviconResourceHistogram) {
  base::HistogramTester tester;
  source().StartDataRequest(
      GURL(base::StrCat({kDummyPrefix, "size/16@1x/https://www.google.com"})),
      test_web_contents_getter(), base::DoNothing());
  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation(
          "Extensions.FaviconResourceUsed"));
  EXPECT_TRUE(samples);
  EXPECT_EQ(0, samples->TotalCount());
}

TEST_P(FaviconSourceTestWithFavicon2Format, DarkDefault) {
  os_settings_provider().SetPreferredColorScheme(
      ui::NativeTheme::PreferredColorScheme::kDark);
  EXPECT_CALL(source(), LoadIconBytes(_, IDR_DEFAULT_FAVICON_DARK));
  source().StartDataRequest(GURL(kDummyPrefix), test_web_contents_getter(),
                            base::DoNothing());
}

TEST_P(FaviconSourceTestWithFavicon2Format, LightDefault) {
  EXPECT_CALL(source(), LoadIconBytes(_, IDR_DEFAULT_FAVICON));
  source().StartDataRequest(GURL(kDummyPrefix), test_web_contents_getter(),
                            base::DoNothing());
}

TEST_P(FaviconSourceTestWithFavicon2Format, LightOverride) {
  os_settings_provider().SetPreferredColorScheme(
      ui::NativeTheme::PreferredColorScheme::kDark);
  EXPECT_CALL(source(), LoadIconBytes(_, IDR_DEFAULT_FAVICON));
  source().StartDataRequest(
      GURL(base::StrCat({kDummyPrefix,
                         "?pageUrl=https%3A%2F%2Fwww.google.com"
                         "&forceLightMode"})),
      test_web_contents_getter(), base::DoNothing());
}

TEST_P(FaviconSourceTestWithFavicon2Format, ForceEmptyDefaultFaviconIsTrue) {
  base::RunLoop run_loop;
  EXPECT_CALL(source(), LoadIconBytes(_, _)).Times(0);
  source().StartDataRequest(
      GURL(base::StrCat({kDummyPrefix,
                         "?pageUrl=https%3A%2F%2Fwww.google.com&"
                         "forceEmptyDefaultFavicon=1"})),
      test_web_contents_getter(),
      base::BindLambdaForTesting(
          [&](scoped_refptr<base::RefCountedMemory> data) {
            EXPECT_EQ(nullptr, data);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_P(FaviconSourceTestWithFavicon2Format, ForceEmptyDefaultFaviconIsFalse) {
  EXPECT_CALL(source(), LoadIconBytes(_, IDR_DEFAULT_FAVICON));
  source().StartDataRequest(
      GURL(base::StrCat({kDummyPrefix,
                         "?pageUrl=https%3A%2F%2Fwww.google.com&"
                         "forceEmptyDefaultFavicon=0"})),
      test_web_contents_getter(), base::DoNothing());
}

TEST_P(FaviconSourceTestWithFavicon2Format, ForceEmptyDefaultFaviconIsOmitted) {
  EXPECT_CALL(source(), LoadIconBytes(_, IDR_DEFAULT_FAVICON));
  source().StartDataRequest(
      GURL(base::StrCat(
          {kDummyPrefix, "?pageUrl=https%3A%2F%2Fwww.google.com"})),
      test_web_contents_getter(), base::DoNothing());
}

TEST_P(FaviconSourceTestWithFavicon2Format,
       ShouldNotQueryHistoryUiFaviconRequestHandlerIfNotAllowed) {
  content::WebContentsTester::For(test_web_contents())
      ->SetLastCommittedURL(GURL(chrome::kChromeUIHistoryURL));

  EXPECT_CALL(mock_history_ui_favicon_request_handler(),
              GetRawFaviconForPageURL)
      .Times(0);

  source().StartDataRequest(
      GURL(base::StrCat(
          {kDummyPrefix,
           "?size=16&scaleFactor=1x&pageUrl=https%3A%2F%2Fwww.google."
           "com&allowGoogleServerFallback=0"})),
      test_web_contents_getter(), base::DoNothing());
}

TEST_P(FaviconSourceTestWithFavicon2Format,
       ShouldNotQueryHistoryUiFaviconRequestHandlerIfHasNotHistoryUiOrigin) {
  content::WebContentsTester::For(test_web_contents())
      ->SetLastCommittedURL(GURL("chrome://non-history-url"));

  EXPECT_CALL(mock_history_ui_favicon_request_handler(),
              GetRawFaviconForPageURL)
      .Times(0);

  source().StartDataRequest(
      GURL(base::StrCat(
          {kDummyPrefix,
           "?size=16&scaleFactor=1x&pageUrl=https%3A%2F%2Fwww.google."
           "com&allowGoogleServerFallback=1"})),
      test_web_contents_getter(), base::DoNothing());
}

TEST_P(
    FaviconSourceTestWithFavicon2Format,
    ShouldQueryHistoryUiFaviconRequestHandlerIfHasHistoryUiOriginAndAllowed) {
  content::WebContentsTester::For(test_web_contents())
      ->SetLastCommittedURL(GURL(chrome::kChromeUIHistoryURL));

  EXPECT_CALL(mock_history_ui_favicon_request_handler(),
              GetRawFaviconForPageURL(GURL("https://www.google.com"), _, _, _))
      .Times(1);

  source().StartDataRequest(
      GURL(base::StrCat(
          {kDummyPrefix,
           "?size=16&scaleFactor=1x&pageUrl=https%3A%2F%2Fwww.google."
           "com&allowGoogleServerFallback=1"})),
      test_web_contents_getter(), base::DoNothing());
}

TEST_P(
    FaviconSourceTestWithFavicon2Format,
    ShouldQueryHistoryUiFaviconRequestHandlerIfHasDataSharingOriginAndAllowed) {
  content::WebContentsTester::For(test_web_contents())
      ->SetLastCommittedURL(GURL(chrome::kChromeUIUntrustedDataSharingURL));

  EXPECT_CALL(mock_history_ui_favicon_request_handler(),
              GetRawFaviconForPageURL(GURL("https://www.google.com"), _, _, _))
      .Times(1);

  source().StartDataRequest(
      GURL(base::StrCat(
          {kDummyPrefix,
           "?size=16&scaleFactor=1x&pageUrl=https%3A%2F%2Fwww.google."
           "com&allowGoogleServerFallback=1"})),
      test_web_contents_getter(), base::DoNothing());
}

TEST_P(FaviconSourceTestWithFavicon2Format,
       ShouldNotQueryIfDesiredSizeTooLarge) {
  EXPECT_CALL(mock_history_ui_favicon_request_handler(),
              GetRawFaviconForPageURL)
      .Times(0);
  EXPECT_CALL(mock_favicon_service(), GetRawFavicon).Times(0);
  EXPECT_CALL(mock_favicon_service(), GetRawFaviconForPageURL).Times(0);

  // 1000x scale factor runs into the max cap.
  source().StartDataRequest(
      GURL(base::StrCat(
          {kDummyPrefix,
           "?size=16&scaleFactor=1000x&pageUrl=https%3A%2F%2Fwww.google.com"})),
      test_web_contents_getter(), base::DoNothing());
}

TEST_P(FaviconSourceTestWithFavicon2Format,
       ShouldNotQueryIfInvalidScaleFactor) {
  EXPECT_CALL(mock_history_ui_favicon_request_handler(),
              GetRawFaviconForPageURL)
      .Times(0);
  EXPECT_CALL(mock_favicon_service(), GetRawFavicon).Times(0);
  EXPECT_CALL(mock_favicon_service(), GetRawFaviconForPageURL).Times(0);

  // A negative scale factor cannot be parsed.
  source().StartDataRequest(
      GURL(base::StrCat(
          {kDummyPrefix,
           "?size=16&scaleFactor=-2x&pageUrl=https%3A%2F%2Fwww.google.com"})),
      test_web_contents_getter(), base::DoNothing());
}

TEST_P(FaviconSourceTestWithFavicon2Format, ValidateGetSource) {
  bool serve_untrusted = GetParam();
  EXPECT_EQ(serve_untrusted ? chrome::kChromeUIUntrustedFavicon2URL
                            : chrome::kChromeUIFavicon2Host,
            source().GetSource());
}
