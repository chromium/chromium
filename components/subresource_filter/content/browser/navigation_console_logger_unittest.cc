// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/navigation_console_logger.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

const std::vector<std::string>& GetConsoleMessages(
    content::RenderFrameHost* rfh) {
  return content::RenderFrameHostTester::For(rfh)->GetConsoleMessages();
}

}  // namespace

using NavigationConsoleLoggerTest = content::RenderViewHostTestHarness;

using NavigationCallback =
    base::RepeatingCallback<void(content::NavigationHandle*)>;
class NavigationFinishCaller : public content::WebContentsObserver {
 public:
  NavigationFinishCaller(content::WebContents* contents,
                         const NavigationCallback& callback)
      : content::WebContentsObserver(contents), callback_(callback) {}
  ~NavigationFinishCaller() override = default;

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override {
    callback_.Run(handle);
  }

 private:
  NavigationCallback callback_;
};

TEST_F(NavigationConsoleLoggerTest, NavigationFails_NoLog) {
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://example.test/"), main_rfh());
  navigation->Start();
  NavigationConsoleLogger::LogMessageOnCommit(
      navigation->GetNavigationHandle(),
      blink::mojom::ConsoleMessageLevel::kWarning, "foo");
  navigation->Fail(net::ERR_ABORTED);

  EXPECT_TRUE(GetConsoleMessages(main_rfh()).empty());
}

TEST_F(NavigationConsoleLoggerTest, NavigationCommitsToErrorPage_NoLog) {
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://example.test/"), main_rfh());
  navigation->Start();
  NavigationConsoleLogger::LogMessageOnCommit(
      navigation->GetNavigationHandle(),
      blink::mojom::ConsoleMessageLevel::kWarning, "foo");
  navigation->Fail(net::ERR_TIMED_OUT);

  EXPECT_TRUE(GetConsoleMessages(main_rfh()).empty());
}

TEST_F(NavigationConsoleLoggerTest, NavigationCommitsSuccessfully_Logs) {
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://example.test/"), main_rfh());
  navigation->Start();
  NavigationConsoleLogger::LogMessageOnCommit(
      navigation->GetNavigationHandle(),
      blink::mojom::ConsoleMessageLevel::kWarning, "foo");

  EXPECT_TRUE(GetConsoleMessages(main_rfh()).empty());
  navigation->Commit();

  EXPECT_TRUE(base::Contains(GetConsoleMessages(main_rfh()), "foo"));
}

TEST_F(NavigationConsoleLoggerTest, NavigationAlreadyCommit_Logs) {
  auto on_finish = [](content::NavigationHandle* handle) {
    NavigationConsoleLogger::LogMessageOnCommit(
        handle, blink::mojom::ConsoleMessageLevel::kWarning, "foo");
  };
  NavigationFinishCaller caller(web_contents(), base::BindRepeating(on_finish));
  NavigateAndCommit(GURL("http://example.test/"));
  EXPECT_TRUE(base::Contains(GetConsoleMessages(main_rfh()), "foo"));
}

TEST_F(NavigationConsoleLoggerTest, NavigationAlreadyFailed_NoLog) {
  auto on_finish = [](content::NavigationHandle* handle) {
    NavigationConsoleLogger::LogMessageOnCommit(
        handle, blink::mojom::ConsoleMessageLevel::kWarning, "foo");
  };
  NavigationFinishCaller caller(web_contents(), base::BindRepeating(on_finish));
  content::NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), GURL("http://example.test/"), net::ERR_TIMED_OUT);
  EXPECT_TRUE(GetConsoleMessages(main_rfh()).empty());
}

TEST_F(NavigationConsoleLoggerTest, MultipleNavigations_OneLog) {
  {
    auto navigation = content::NavigationSimulator::CreateRendererInitiated(
        GURL("http://example.test/"), main_rfh());
    navigation->Start();
    NavigationConsoleLogger::LogMessageOnCommit(
        navigation->GetNavigationHandle(),
        blink::mojom::ConsoleMessageLevel::kWarning, "foo");
    navigation->Commit();
  }
  NavigateAndCommit(GURL("http://example.test/"));
  EXPECT_EQ(1u, GetConsoleMessages(main_rfh()).size());
}

TEST_F(NavigationConsoleLoggerTest, MultipleMessages) {
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://example.test/"), main_rfh());
  navigation->Start();
  NavigationConsoleLogger::LogMessageOnCommit(
      navigation->GetNavigationHandle(),
      blink::mojom::ConsoleMessageLevel::kWarning, "foo");
  NavigationConsoleLogger::LogMessageOnCommit(
      navigation->GetNavigationHandle(),
      blink::mojom::ConsoleMessageLevel::kWarning, "bar");

  EXPECT_TRUE(GetConsoleMessages(main_rfh()).empty());
  navigation->Commit();

  EXPECT_EQ(2u, GetConsoleMessages(main_rfh()).size());
}

TEST_F(NavigationConsoleLoggerTest, SyncNavigationDuringNavigation) {
  NavigateAndCommit(GURL("http://example.test/"));

  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://example.test/path"), main_rfh());
  navigation->Start();
  NavigationConsoleLogger::LogMessageOnCommit(
      navigation->GetNavigationHandle(),
      blink::mojom::ConsoleMessageLevel::kWarning, "foo");

  content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://example.test/#hash"), main_rfh())
      ->CommitSameDocument();
  EXPECT_EQ(0u, GetConsoleMessages(main_rfh()).size());

  navigation->Commit();
  EXPECT_EQ(1u, GetConsoleMessages(main_rfh()).size());
}

}  // namespace subresource_filter
