// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/viewer.h"

#include <memory>

#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/distiller_ui_handle.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace dom_distiller {

namespace {

const GURL GetDistillerViewUrlFromUrl(const std::string& url) {
  return url_utils::GetDistillerViewUrlFromUrl(kDomDistillerScheme, GURL(url),
                                               "Title");
}

const GURL GetDistillerViewUrlFromEntryId(const std::string& id) {
  return url_utils::GetDistillerViewUrlFromEntryId(kDomDistillerScheme, id);
}

}  // namespace

class FakeViewRequestDelegate : public ViewRequestDelegate {
 public:
  ~FakeViewRequestDelegate() override = default;
  MOCK_METHOD1(OnArticleReady, void(const DistilledArticleProto* proto));
  MOCK_METHOD1(OnArticleUpdated,
               void(ArticleDistillationUpdate article_update));
};

class TestDomDistillerService : public DomDistillerServiceInterface {
 public:
  TestDomDistillerService() = default;
  ~TestDomDistillerService() override = default;

  MOCK_METHOD0(ViewUrlImpl, ViewerHandle*());
  std::unique_ptr<ViewerHandle> ViewUrl(
      ViewRequestDelegate*,
      std::unique_ptr<DistillerPage> distiller_page,
      const GURL&) override {
    return std::unique_ptr<ViewerHandle>(ViewUrlImpl());
  }
  std::unique_ptr<DistillerPage> CreateDefaultDistillerPage(
      const gfx::Size& render_view_size) override {
    return nullptr;
  }
  std::unique_ptr<DistillerPage> CreateDefaultDistillerPageWithHandle(
      std::unique_ptr<SourcePageHandle> handle) override {
    return nullptr;
  }
  DistilledPagePrefs* GetDistilledPagePrefs() override;
  DistillerUIHandle* GetDistillerUIHandle() override;
};

class DomDistillerViewerTest : public testing::Test {
 public:
  void SetUp() override {
    service_ = std::make_unique<TestDomDistillerService>();
  }

 protected:
  std::unique_ptr<ViewerHandle> CreateViewRequest(
      const GURL& url,
      ViewRequestDelegate* view_request_delegate) {
    return viewer::CreateViewRequest(service_.get(), url, view_request_delegate,
                                     gfx::Size());
  }

  std::unique_ptr<TestDomDistillerService> service_;
};

TEST_F(DomDistillerViewerTest, TestCreatingViewUrlRequest) {
  std::unique_ptr<FakeViewRequestDelegate> view_request_delegate(
      new FakeViewRequestDelegate());
  ViewerHandle* viewer_handle(new ViewerHandle(ViewerHandle::CancelCallback()));
  EXPECT_CALL(*service_, ViewUrlImpl())
      .WillOnce(testing::Return(viewer_handle));
  CreateViewRequest(GetDistillerViewUrlFromUrl("http://www.example.com/"),
                    view_request_delegate.get());
}

TEST_F(DomDistillerViewerTest, TestCreatingInvalidViewRequest) {
  std::unique_ptr<FakeViewRequestDelegate> view_request_delegate(
      new FakeViewRequestDelegate());
  EXPECT_CALL(*service_, ViewUrlImpl()).Times(0);
  // Specify none of the required query parameters.
  CreateViewRequest(GURL(std::string(kDomDistillerScheme) + "://host?foo=bar"),
                    view_request_delegate.get());
  // Specify both of the required query parameters.
  CreateViewRequest(net::AppendOrReplaceQueryParameter(
                        GetDistillerViewUrlFromUrl("http://www.example.com/"),
                        kEntryIdKey, "abc-def"),
                    view_request_delegate.get());
  // Specify an internal Chrome page.
  CreateViewRequest(GetDistillerViewUrlFromUrl("chrome://settings/"),
                    view_request_delegate.get());
  // Specify a recursive URL.
  CreateViewRequest(GetDistillerViewUrlFromUrl(
                        GetDistillerViewUrlFromEntryId("abc-def").spec()),
                    view_request_delegate.get());
  // Specify a non-distilled URL.
  CreateViewRequest(GURL("https://example.com"), view_request_delegate.get());
  // Specify an empty URL.
  CreateViewRequest(GURL(), view_request_delegate.get());
}

DistilledPagePrefs* TestDomDistillerService::GetDistilledPagePrefs() {
  return nullptr;
}

DistillerUIHandle* TestDomDistillerService::GetDistillerUIHandle() {
  return nullptr;
}

TEST_F(DomDistillerViewerTest, TestGetDistilledPageThemeJsOutput) {
  std::string kDarkJs = "useTheme('dark');";
  std::string kSepiaJs = "useTheme('sepia');";
  std::string kLightJs = "useTheme('light');";
  EXPECT_EQ(
      kDarkJs.compare(viewer::GetDistilledPageThemeJs(mojom::Theme::kDark)), 0);
  EXPECT_EQ(
      kLightJs.compare(viewer::GetDistilledPageThemeJs(mojom::Theme::kLight)),
      0);
  EXPECT_EQ(
      kSepiaJs.compare(viewer::GetDistilledPageThemeJs(mojom::Theme::kSepia)),
      0);
}

TEST_F(DomDistillerViewerTest, TestGetDistilledPageFontFamilyJsOutput) {
  std::string kSerifJsFontFamily = "useFontFamily('serif');";
  std::string kMonospaceJsFontFamily = "useFontFamily('monospace');";
  std::string kSansSerifJsFontFamily = "useFontFamily('sans-serif');";
  EXPECT_EQ(kSerifJsFontFamily.compare(viewer::GetDistilledPageFontFamilyJs(
                mojom::FontFamily::kSerif)),
            0);
  EXPECT_EQ(kMonospaceJsFontFamily.compare(viewer::GetDistilledPageFontFamilyJs(
                mojom::FontFamily::kMonospace)),
            0);
  EXPECT_EQ(kSansSerifJsFontFamily.compare(viewer::GetDistilledPageFontFamilyJs(
                mojom::FontFamily::kSansSerif)),
            0);
}

TEST_F(DomDistillerViewerTest, TestGetDistilledPageFontScalingJsOutput) {
  std::string kJsFontScaling = "useFontScaling(5);";
  EXPECT_EQ(kJsFontScaling.compare(viewer::GetDistilledPageFontScalingJs(5)),
            0);
}

}  // namespace dom_distiller
