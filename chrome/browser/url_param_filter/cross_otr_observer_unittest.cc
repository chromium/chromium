// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/url_param_filter/cross_otr_observer.h"

#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

using ::testing::NiceMock;

namespace url_param_filter {

constexpr char kResponseCodeMetricName[] =
    "Navigation.CrossOtr.ContextMenu.ResponseCodeExperimental";
constexpr char kCrossOtrRefreshCountMetricName[] =
    "Navigation.CrossOtr.ContextMenu.RefreshCountExperimental";

class CrossOtrObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  CrossOtrObserverTest() = default;

  std::unique_ptr<content::MockNavigationHandle> CreateMockNavigationHandle() {
    return std::make_unique<NiceMock<content::MockNavigationHandle>>(
        GURL("https://example.com/"), main_rfh());
  }
};

TEST_F(CrossOtrObserverTest, NotContextMenuInitiated) {
  NavigateParams params(profile(), GURL("https://www.foo.com"),
                        ui::PAGE_TRANSITION_LINK);

  params.privacy_sensitivity = NavigateParams::PrivacySensitivity::DEFAULT;
  params.started_from_context_menu = false;

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  CrossOtrObserver::MaybeCreateForWebContents(web_contents.get(), params);

  ASSERT_EQ(CrossOtrObserver::FromWebContents(web_contents.get()), nullptr);
}
TEST_F(CrossOtrObserverTest, DefaultSensitivity) {
  NavigateParams params(profile(), GURL("https://www.foo.com"),
                        ui::PAGE_TRANSITION_LINK);

  params.started_from_context_menu = false;
  params.privacy_sensitivity = NavigateParams::PrivacySensitivity::DEFAULT;

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  CrossOtrObserver::MaybeCreateForWebContents(web_contents.get(), params);

  ASSERT_EQ(CrossOtrObserver::FromWebContents(web_contents.get()), nullptr);
}
TEST_F(CrossOtrObserverTest, BookmarkLink) {
  NavigateParams params(profile(), GURL("https://www.foo.com"),
                        ui::PAGE_TRANSITION_AUTO_BOOKMARK);

  params.started_from_context_menu = true;
  params.privacy_sensitivity = NavigateParams::PrivacySensitivity::CROSS_OTR;

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  CrossOtrObserver::MaybeCreateForWebContents(web_contents.get(), params);

  ASSERT_EQ(CrossOtrObserver::FromWebContents(web_contents.get()), nullptr);
}
TEST_F(CrossOtrObserverTest, CreateKey) {
  NavigateParams params(profile(), GURL("https://www.foo.com"),
                        ui::PAGE_TRANSITION_LINK);

  params.started_from_context_menu = true;
  params.privacy_sensitivity = NavigateParams::PrivacySensitivity::CROSS_OTR;
  content::WebContents* contents = web_contents();
  CrossOtrObserver::MaybeCreateForWebContents(contents, params);

  ASSERT_NE(CrossOtrObserver::FromWebContents(contents), nullptr);
}
TEST_F(CrossOtrObserverTest, DuplicateCreateKey) {
  NavigateParams params(profile(), GURL("https://www.foo.com"),
                        ui::PAGE_TRANSITION_LINK);

  params.started_from_context_menu = true;
  params.privacy_sensitivity = NavigateParams::PrivacySensitivity::CROSS_OTR;
  content::WebContents* contents = web_contents();
  CrossOtrObserver::MaybeCreateForWebContents(contents, params);
  CrossOtrObserver::MaybeCreateForWebContents(contents, params);

  ASSERT_NE(CrossOtrObserver::FromWebContents(contents), nullptr);
}
TEST_F(CrossOtrObserverTest, HandleRedirects) {
  base::HistogramTester histogram_tester;
  NavigateParams params(profile(), GURL("https://www.foo.com"),
                        ui::PAGE_TRANSITION_LINK);

  params.started_from_context_menu = true;
  params.privacy_sensitivity = NavigateParams::PrivacySensitivity::CROSS_OTR;
  content::WebContents* contents = web_contents();
  CrossOtrObserver::MaybeCreateForWebContents(contents, params);
  CrossOtrObserver* observer = CrossOtrObserver::FromWebContents(contents);
  ASSERT_NE(observer, nullptr);
  std::unique_ptr<content::MockNavigationHandle> handle =
      std::make_unique<NiceMock<content::MockNavigationHandle>>(contents);

  scoped_refptr<net::HttpResponseHeaders> response =
      base::MakeRefCounted<net::HttpResponseHeaders>(
          "HTTP/1.1 302 Moved Temporarily");
  handle->set_response_headers(response);
  observer->DidRedirectNavigation(handle.get());
  histogram_tester.ExpectTotalCount(kResponseCodeMetricName, 1);
  histogram_tester.ExpectUniqueSample(
      kResponseCodeMetricName, net::HttpUtil::MapStatusCodeForHistogram(302),
      1);
}
TEST_F(CrossOtrObserverTest, FinishedNavigation) {
  base::HistogramTester histogram_tester;
  NavigateParams params(profile(), GURL("https://www.foo.com"),
                        ui::PAGE_TRANSITION_LINK);

  params.started_from_context_menu = true;
  params.privacy_sensitivity = NavigateParams::PrivacySensitivity::CROSS_OTR;
  content::WebContents* contents = web_contents();
  CrossOtrObserver::MaybeCreateForWebContents(contents, params);
  CrossOtrObserver* observer = CrossOtrObserver::FromWebContents(contents);
  ASSERT_NE(observer, nullptr);
  std::unique_ptr<content::MockNavigationHandle> handle =
      std::make_unique<NiceMock<content::MockNavigationHandle>>(contents);

  scoped_refptr<net::HttpResponseHeaders> response =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  handle->set_response_headers(response);
  observer->DidFinishNavigation(handle.get());
  histogram_tester.ExpectTotalCount(kResponseCodeMetricName, 1);
  histogram_tester.ExpectUniqueSample(
      kResponseCodeMetricName, net::HttpUtil::MapStatusCodeForHistogram(200),
      1);
}
TEST_F(CrossOtrObserverTest, BadRedirectResponse) {
  base::HistogramTester histogram_tester;
  NavigateParams params(profile(), GURL("https://www.foo.com"),
                        ui::PAGE_TRANSITION_LINK);

  params.started_from_context_menu = true;
  params.privacy_sensitivity = NavigateParams::PrivacySensitivity::CROSS_OTR;
  content::WebContents* contents = web_contents();
  CrossOtrObserver::MaybeCreateForWebContents(contents, params);
  CrossOtrObserver* observer = CrossOtrObserver::FromWebContents(contents);
  ASSERT_NE(observer, nullptr);
  std::unique_ptr<content::MockNavigationHandle> handle =
      std::make_unique<NiceMock<content::MockNavigationHandle>>(contents);

  handle->set_response_headers(nullptr);
  observer->DidRedirectNavigation(handle.get());
  histogram_tester.ExpectTotalCount(kResponseCodeMetricName, 0);
}
TEST_F(CrossOtrObserverTest, BadNavigationResponse) {
  base::HistogramTester histogram_tester;
  NavigateParams params(profile(), GURL("https://www.foo.com"),
                        ui::PAGE_TRANSITION_LINK);

  params.started_from_context_menu = true;
  params.privacy_sensitivity = NavigateParams::PrivacySensitivity::CROSS_OTR;
  content::WebContents* contents = web_contents();
  CrossOtrObserver::MaybeCreateForWebContents(contents, params);
  CrossOtrObserver* observer = CrossOtrObserver::FromWebContents(contents);
  ASSERT_NE(observer, nullptr);
  std::unique_ptr<content::MockNavigationHandle> handle =
      std::make_unique<NiceMock<content::MockNavigationHandle>>(contents);

  handle->set_response_headers(nullptr);
  observer->DidFinishNavigation(handle.get());
  histogram_tester.ExpectTotalCount(kResponseCodeMetricName, 0);

  // The observer should not cease observation after first load, regardless of
  // whether the headers include a response code. We still want to see
  // the refresh count.
  ASSERT_NE(CrossOtrObserver::FromWebContents(contents), nullptr);
}
TEST_F(CrossOtrObserverTest, RefreshedAfterNavigation) {
  base::HistogramTester histogram_tester;
  NavigateParams params(profile(), GURL("https://www.foo.com"),
                        ui::PAGE_TRANSITION_LINK);

  params.started_from_context_menu = true;
  params.privacy_sensitivity = NavigateParams::PrivacySensitivity::CROSS_OTR;
  content::WebContents* contents = web_contents();
  CrossOtrObserver::MaybeCreateForWebContents(contents, params);
  CrossOtrObserver* observer = CrossOtrObserver::FromWebContents(contents);
  ASSERT_NE(observer, nullptr);
  std::unique_ptr<content::MockNavigationHandle> handle =
      std::make_unique<NiceMock<content::MockNavigationHandle>>(contents);

  scoped_refptr<net::HttpResponseHeaders> response =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  handle->set_response_headers(response);
  observer->DidFinishNavigation(handle.get());

  handle->set_reload_type(content::ReloadType::NORMAL);
  observer->DidFinishNavigation(handle.get());
  observer->WebContentsDestroyed();

  histogram_tester.ExpectTotalCount(kCrossOtrRefreshCountMetricName, 1);
  ASSERT_EQ(histogram_tester.GetTotalSum(kCrossOtrRefreshCountMetricName), 1);
  histogram_tester.ExpectTotalCount(kResponseCodeMetricName, 1);
  histogram_tester.ExpectUniqueSample(
      kResponseCodeMetricName, net::HttpUtil::MapStatusCodeForHistogram(200),
      1);
}
TEST_F(CrossOtrObserverTest, UncommittedNavigationWithRefresh) {
  base::HistogramTester histogram_tester;
  NavigateParams params(profile(), GURL("https://www.foo.com"),
                        ui::PAGE_TRANSITION_LINK);

  params.started_from_context_menu = true;
  params.privacy_sensitivity = NavigateParams::PrivacySensitivity::CROSS_OTR;
  content::WebContents* contents = web_contents();
  CrossOtrObserver::MaybeCreateForWebContents(contents, params);
  CrossOtrObserver* observer = CrossOtrObserver::FromWebContents(contents);
  ASSERT_NE(observer, nullptr);
  std::unique_ptr<content::MockNavigationHandle> handle =
      std::make_unique<NiceMock<content::MockNavigationHandle>>(contents);

  scoped_refptr<net::HttpResponseHeaders> response =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  handle->set_response_headers(response);
  observer->DidFinishNavigation(handle.get());

  // Finish a non-reload navigation, but one that isn't committed (so no actual
  // navigation away from the monitored page)
  handle->set_reload_type(content::ReloadType::NONE);
  handle->set_is_in_primary_main_frame(true);
  handle->set_is_same_document(false);
  handle->set_has_committed(false);

  observer->DidFinishNavigation(handle.get());

  // After that uncommitted navigation, trigger a redirect, then destroy.
  handle->set_reload_type(content::ReloadType::NORMAL);
  observer->DidFinishNavigation(handle.get());
  observer->WebContentsDestroyed();

  // We had 1 relevant refresh.
  histogram_tester.ExpectTotalCount(kCrossOtrRefreshCountMetricName, 1);
  ASSERT_EQ(histogram_tester.GetTotalSum(kCrossOtrRefreshCountMetricName), 1);
  histogram_tester.ExpectTotalCount(kResponseCodeMetricName, 1);
  histogram_tester.ExpectUniqueSample(
      kResponseCodeMetricName, net::HttpUtil::MapStatusCodeForHistogram(200),
      1);
}
TEST_F(CrossOtrObserverTest, MultipleRefreshesAfterNavigation) {
  base::HistogramTester histogram_tester;
  NavigateParams params(profile(), GURL("https://www.foo.com"),
                        ui::PAGE_TRANSITION_LINK);

  params.started_from_context_menu = true;
  params.privacy_sensitivity = NavigateParams::PrivacySensitivity::CROSS_OTR;
  content::WebContents* contents = web_contents();
  CrossOtrObserver::MaybeCreateForWebContents(contents, params);
  CrossOtrObserver* observer = CrossOtrObserver::FromWebContents(contents);
  ASSERT_NE(observer, nullptr);
  std::unique_ptr<content::MockNavigationHandle> handle =
      std::make_unique<NiceMock<content::MockNavigationHandle>>(contents);

  scoped_refptr<net::HttpResponseHeaders> response =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  handle->set_response_headers(response);
  observer->DidFinishNavigation(handle.get());

  // Reload twice and ensure the count is persisted.
  handle->set_reload_type(content::ReloadType::NORMAL);
  observer->DidFinishNavigation(handle.get());
  observer->DidFinishNavigation(handle.get());

  // Navigating away means no more observer.
  handle->set_reload_type(content::ReloadType::NONE);
  handle->set_is_in_primary_main_frame(true);
  handle->set_is_same_document(false);
  handle->set_has_committed(true);
  observer->DidFinishNavigation(handle.get());

  ASSERT_EQ(CrossOtrObserver::FromWebContents(contents), nullptr);

  histogram_tester.ExpectTotalCount(kCrossOtrRefreshCountMetricName, 1);
  ASSERT_EQ(histogram_tester.GetTotalSum(kCrossOtrRefreshCountMetricName), 2);
  histogram_tester.ExpectTotalCount(kResponseCodeMetricName, 1);
  histogram_tester.ExpectUniqueSample(
      kResponseCodeMetricName, net::HttpUtil::MapStatusCodeForHistogram(200),
      1);
}
TEST_F(CrossOtrObserverTest, RedirectsAfterNavigation) {
  base::HistogramTester histogram_tester;
  NavigateParams params(profile(), GURL("https://www.foo.com"),
                        ui::PAGE_TRANSITION_LINK);

  params.started_from_context_menu = true;
  params.privacy_sensitivity = NavigateParams::PrivacySensitivity::CROSS_OTR;
  content::WebContents* contents = web_contents();
  CrossOtrObserver::MaybeCreateForWebContents(contents, params);
  CrossOtrObserver* observer = CrossOtrObserver::FromWebContents(contents);
  ASSERT_NE(observer, nullptr);
  std::unique_ptr<content::MockNavigationHandle> handle =
      std::make_unique<NiceMock<content::MockNavigationHandle>>(contents);

  scoped_refptr<net::HttpResponseHeaders> response =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  handle->set_response_headers(response);
  observer->DidFinishNavigation(handle.get());

  // Redirects observed after the first navigation has committed should not
  // write responses.
  response = base::MakeRefCounted<net::HttpResponseHeaders>(
      "HTTP/1.1 302 Moved Temporarily");
  handle->set_response_headers(response);
  observer->DidRedirectNavigation(handle.get());

  histogram_tester.ExpectTotalCount(kResponseCodeMetricName, 1);
  histogram_tester.ExpectUniqueSample(
      kResponseCodeMetricName, net::HttpUtil::MapStatusCodeForHistogram(200),
      1);
}
}  // namespace url_param_filter
