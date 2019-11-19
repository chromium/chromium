// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ios/ukm_url_recorder.h"

#include "base/bind.h"
#include "base/optional.h"
#import "base/test/ios/wait_util.h"
#include "components/ukm/test_ukm_recorder.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path() == "/title1.html") {
    auto result = std::make_unique<net::test_server::BasicHttpResponse>();
    result->set_content_type("text/html");
    result->set_content("<html><head></head><body>NoTitle</body></html>");
    return std::move(result);
  }
  if (request.GetURL().path() == "/page_with_iframe.html") {
    auto result = std::make_unique<net::test_server::BasicHttpResponse>();
    result->set_content_type("text/html");
    result->set_content(
        "<html><head></head><body><iframe src=\"title1.html\"></body></html>");
    return std::move(result);
  }
  if (request.GetURL().path() == "/download") {
    auto result = std::make_unique<net::test_server::BasicHttpResponse>();
    result->set_content_type("application/vnd.test");
    result->set_content("TestDownloadContent");
    return std::move(result);
  }
  if (request.GetURL().path() == "/redirect") {
    auto result = std::make_unique<net::test_server::BasicHttpResponse>();
    result->set_code(net::HTTP_MOVED_PERMANENTLY);
    result->AddCustomHeader("Location", "/title1.html");
    return std::move(result);
  }
  return nullptr;
}

}  // namespace

class UkmUrlRecorderTest : public web::WebTestWithWebState {
 protected:
  UkmUrlRecorderTest() {
    server_.RegisterDefaultHandler(base::BindRepeating(&HandleRequest));
  }

  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    ASSERT_TRUE(server_.Start());
    ukm::InitializeSourceUrlRecorderForWebState(web_state());
  }

  bool LoadUrlAndWait(const GURL& url) {
    web::NavigationManager::WebLoadParams params(url);
    web_state()->GetNavigationManager()->LoadURLWithParams(params);
    return base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForPageLoadTimeout, ^{
          return !web_state()->IsLoading();
        });
  }

  testing::AssertionResult RecordedUrl(
      ukm::SourceId source_id,
      GURL expected_url,
      base::Optional<GURL> expected_initial_url) {
    auto* source = test_ukm_recorder_.GetSourceForSourceId(source_id);
    if (!source)
      return testing::AssertionFailure() << "No URL recorded";
    if (source->url() != expected_url)
      return testing::AssertionFailure()
             << "Url was " << source->url() << ", expected: " << expected_url;
    base::Optional<GURL> initial_url;
    if (source->urls().size() > 1u)
      initial_url = source->urls().front();
    if (expected_initial_url != initial_url) {
      return testing::AssertionFailure()
             << "Initial Url was " << initial_url.value_or(GURL())
             << ", expected: " << expected_initial_url.value_or(GURL());
    }
    return testing::AssertionSuccess();
  }

  testing::AssertionResult DidNotRecordUrl(GURL url) {
    const auto& sources = test_ukm_recorder_.GetSources();
    for (const auto& kv : sources) {
      if (kv.second->url() == url)
        return testing::AssertionFailure()
               << "Url " << url << " was recorded with SourceId: " << kv.first;
      if (kv.second->url() == url)
        return testing::AssertionFailure()
               << "Url " << url
               << " was recorded as an initial URL with SourceId: " << kv.first;
    }
    return testing::AssertionSuccess();
  }

 protected:
  net::EmbeddedTestServer server_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

// Tests that URLs get recorded for pages visited.
TEST_F(UkmUrlRecorderTest, Basic) {
  GURL url = server_.GetURL("/title1.html");
  EXPECT_TRUE(LoadUrlAndWait(url));
  ukm::SourceId source_id = ukm::GetSourceIdForWebStateDocument(web_state());
  EXPECT_TRUE(RecordedUrl(source_id, url, base::nullopt));
}

// Tests that subframe URLs do not get recorded.
TEST_F(UkmUrlRecorderTest, IgnoreUrlInSubframe) {
  GURL main_url = server_.GetURL("/page_with_iframe.html");
  GURL subframe_url = server_.GetURL("/title1.html");
  EXPECT_TRUE(LoadUrlAndWait(main_url));
  ukm::SourceId source_id = ukm::GetSourceIdForWebStateDocument(web_state());
  EXPECT_TRUE(RecordedUrl(source_id, main_url, base::nullopt));
  EXPECT_TRUE(DidNotRecordUrl(subframe_url));
}

// Tests that download URLs do not get recorded.
TEST_F(UkmUrlRecorderTest, IgnoreDownloadUrl) {
  GURL url = server_.GetURL("/download");
  EXPECT_TRUE(LoadUrlAndWait(url));
  EXPECT_TRUE(DidNotRecordUrl(url));
}

// Tests that redirected URLs record initial and final URL.
TEST_F(UkmUrlRecorderTest, InitialUrl) {
  GURL redirect_url = server_.GetURL("/redirect");
  GURL target_url = server_.GetURL("/title1.html");
  EXPECT_TRUE(LoadUrlAndWait(redirect_url));
  ukm::SourceId source_id = ukm::GetSourceIdForWebStateDocument(web_state());
  EXPECT_TRUE(RecordedUrl(source_id, target_url, redirect_url));
}
