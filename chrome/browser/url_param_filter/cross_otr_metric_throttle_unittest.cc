// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/url_param_filter/cross_otr_metric_throttle.h"

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

using content::MockNavigationHandle;
using ::testing::NiceMock;

namespace url_param_filter {

const char kMetricName[] =
    "Navigation.CrossOtr.ContextMenu.ResponseCodeExperimental";

// A test class that covers valid and invalid cases, with the end-to-end flow
// additionally asserted in browser tests.
class CrossOtrMetricNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 public:
  CrossOtrMetricNavigationThrottleTest() = default;

  std::unique_ptr<MockNavigationHandle> CreateMockNavigationHandle() {
    return std::make_unique<NiceMock<MockNavigationHandle>>(
        GURL("https://example.com/"), main_rfh());
  }
};

TEST_F(CrossOtrMetricNavigationThrottleTest, NotContextMenuInitiated) {
  std::unique_ptr<MockNavigationHandle> handle = CreateMockNavigationHandle();
  // The navigation is not context menu initiated, so do not create a throttle.
  ASSERT_EQ(nullptr, CrossOtrMetricNavigationThrottle::MaybeCreateThrottleFor(
                         handle.get()));
}

TEST_F(CrossOtrMetricNavigationThrottleTest, ContextMenuInitiatedNoInitiator) {
  std::unique_ptr<MockNavigationHandle> handle = CreateMockNavigationHandle();
  handle->set_was_started_from_context_menu(true);
  handle->set_initiator_frame_token(nullptr);
  // No initiator means no throttle should be created.
  ASSERT_EQ(nullptr, CrossOtrMetricNavigationThrottle::MaybeCreateThrottleFor(
                         handle.get()));
}

TEST_F(CrossOtrMetricNavigationThrottleTest, ContextMenuInitiatedBadInitiator) {
  std::unique_ptr<MockNavigationHandle> handle = CreateMockNavigationHandle();
  handle->set_was_started_from_context_menu(true);
  blink::LocalFrameToken default_token;
  handle->set_initiator_frame_token(&default_token);
  // An initiator that is invalid should not result in creation of a throttle.
  ASSERT_EQ(nullptr, CrossOtrMetricNavigationThrottle::MaybeCreateThrottleFor(
                         handle.get()));
}

TEST_F(CrossOtrMetricNavigationThrottleTest, ContextMenuNotIncognito) {
  // Create a mock render process host that is not Off the Record.
  std::unique_ptr<content::MockRenderProcessHost> render_process_host =
      std::make_unique<content::MockRenderProcessHost>(profile());

  std::unique_ptr<MockNavigationHandle> handle = CreateMockNavigationHandle();
  handle->set_was_started_from_context_menu(true);
  blink::LocalFrameToken default_token;
  handle->set_initiator_frame_token(&default_token);
  handle->set_initiator_process_id(render_process_host->GetID());
  // The initiator is valid and it was started from the context menu, but the
  // navigation handle's target WebContents is not Off the Record. Therefore, we
  // must not create a throttle.
  ASSERT_EQ(nullptr, CrossOtrMetricNavigationThrottle::MaybeCreateThrottleFor(
                         handle.get()));
}

TEST_F(CrossOtrMetricNavigationThrottleTest, ContextMenuInitiatorIncognito) {
  // Create a mock render process host that is Off the Record.
  std::unique_ptr<content::MockRenderProcessHost> render_process_host =
      std::make_unique<content::MockRenderProcessHost>(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  std::unique_ptr<MockNavigationHandle> handle = CreateMockNavigationHandle();
  handle->set_was_started_from_context_menu(true);
  blink::LocalFrameToken default_token;
  handle->set_initiator_frame_token(&default_token);
  handle->set_initiator_process_id(render_process_host->GetID());
  // The initiator is valid and it was started from the context menu, but the
  // initiator is already Off the Record. Therefore, we must not create a
  // throttle.
  ASSERT_EQ(nullptr, CrossOtrMetricNavigationThrottle::MaybeCreateThrottleFor(
                         handle.get()));
}

TEST_F(CrossOtrMetricNavigationThrottleTest, CreateThrottle) {
  // Create a mock render process host that is not Off the Record.
  std::unique_ptr<content::MockRenderProcessHost> render_process_host =
      std::make_unique<content::MockRenderProcessHost>(profile());

  // Create the target webcontents with browsing context that indicates it's Off
  // the Record.
  std::unique_ptr<content::WebContents> incognito_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true), nullptr);

  std::unique_ptr<MockNavigationHandle> handle =
      std::make_unique<NiceMock<MockNavigationHandle>>(
          incognito_web_contents.get());
  handle->set_was_started_from_context_menu(true);
  blink::LocalFrameToken default_token;
  // Set up the mock handle with a valid initiator and ensure it is our normal
  // browsing instance.
  handle->set_initiator_frame_token(&default_token);
  handle->set_initiator_process_id(render_process_host->GetID());
  // The navigation handle's web contents are incognito mode; its initiator is
  // not, and it was started from the context menu. Therefore, we are in the
  // context menu "Open Link in Incognito Window" case; a throttle must be
  // created.
  ASSERT_NE(nullptr, CrossOtrMetricNavigationThrottle::MaybeCreateThrottleFor(
                         handle.get()));
}

TEST_F(CrossOtrMetricNavigationThrottleTest, CreateThrottleBadResponse) {
  base::HistogramTester histogram_tester;
  // Create a mock render process host that is not Off the Record.
  std::unique_ptr<content::MockRenderProcessHost> render_process_host =
      std::make_unique<content::MockRenderProcessHost>(profile());

  // Create the target webcontents with browsing context that indicates it's Off
  // the Record.
  std::unique_ptr<content::WebContents> incognito_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true), nullptr);

  std::unique_ptr<MockNavigationHandle> handle =
      std::make_unique<NiceMock<MockNavigationHandle>>(
          incognito_web_contents.get());
  handle->set_was_started_from_context_menu(true);
  blink::LocalFrameToken default_token;
  // Set up the mock handle with a valid initiator and ensure it is our normal
  // browsing instance.
  handle->set_initiator_frame_token(&default_token);
  handle->set_initiator_process_id(render_process_host->GetID());

  // Set null response headers and ensure we don't error out when processing a
  // response.
  handle->set_response_headers(nullptr);

  std::unique_ptr<content::NavigationThrottle> throttle =
      CrossOtrMetricNavigationThrottle::MaybeCreateThrottleFor(handle.get());
  ASSERT_NE(nullptr, throttle);

  throttle->WillProcessResponse();
  // We have no response headers, which means no response code to log a metric
  // for.
  histogram_tester.ExpectTotalCount(kMetricName, 0);
}

TEST_F(CrossOtrMetricNavigationThrottleTest, CreateThrottleMetricLogged) {
  base::HistogramTester histogram_tester;
  // Create a mock render process host that is not Off the Record.
  std::unique_ptr<content::MockRenderProcessHost> render_process_host =
      std::make_unique<content::MockRenderProcessHost>(profile());

  // Create the target webcontents with browsing context that indicates it's Off
  // the Record.
  std::unique_ptr<content::WebContents> incognito_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true), nullptr);

  std::unique_ptr<MockNavigationHandle> handle =
      std::make_unique<NiceMock<MockNavigationHandle>>(
          incognito_web_contents.get());
  handle->set_was_started_from_context_menu(true);
  blink::LocalFrameToken default_token;
  // Set up the mock handle with a valid initiator and ensure it is our normal
  // browsing instance.
  handle->set_initiator_frame_token(&default_token);
  handle->set_initiator_process_id(render_process_host->GetID());

  scoped_refptr<net::HttpResponseHeaders> response =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  handle->set_response_headers(response);

  std::unique_ptr<content::NavigationThrottle> throttle =
      CrossOtrMetricNavigationThrottle::MaybeCreateThrottleFor(handle.get());
  ASSERT_NE(nullptr, throttle);

  throttle->WillProcessResponse();
  histogram_tester.ExpectTotalCount(kMetricName, 1);
  histogram_tester.ExpectUniqueSample(
      kMetricName, net::HttpUtil::MapStatusCodeForHistogram(200), 1);
}

}  // namespace url_param_filter
