// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/js_injection/browser/js_to_browser_messaging.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "components/js_injection/browser/web_message.h"
#include "components/js_injection/browser/web_message_host.h"
#include "components/js_injection/browser/web_message_host_factory.h"
#include "components/js_injection/common/interfaces.mojom.h"
#include "components/origin_matcher/origin_matcher.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "url/gurl.h"

namespace js_injection {
namespace {

// Records messages delivered to the test listener.
struct MessageLog {
  void OnPostMessage(std::unique_ptr<WebMessage> message) {
    if (std::holds_alternative<std::u16string>(message->message)) {
      payloads.push_back(std::get<std::u16string>(message->message));
    }
    ++count;
  }

  // Waits until `count`, the total since the test started, reaches
  // `expected`. Fails if it is already past `expected` or overshoots it
  // before the wait returns.
  void WaitForCount(int expected) {
    ASSERT_TRUE(base::test::RunUntil([&] { return count >= expected; }));
    EXPECT_EQ(expected, count);
  }

  int count = 0;
  std::vector<std::u16string> payloads;
};

class CountingWebMessageHost : public WebMessageHost {
 public:
  explicit CountingWebMessageHost(MessageLog& log) : log_(log) {}
  ~CountingWebMessageHost() override = default;

  void OnPostMessage(std::unique_ptr<WebMessage> message) override {
    log_->OnPostMessage(std::move(message));
  }

 private:
  raw_ref<MessageLog> log_;
};

class CountingWebMessageHostFactory : public WebMessageHostFactory {
 public:
  explicit CountingWebMessageHostFactory(MessageLog& log) : log_(log) {}
  ~CountingWebMessageHostFactory() override = default;

  base::WeakPtr<WebMessageHostFactory> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }
  std::unique_ptr<WebMessageHost> CreateHost(
      const std::string& top_level_origin_string,
      const std::string& origin_string,
      bool is_main_frame,
      WebMessageReplyProxy* proxy) override {
    return std::make_unique<CountingWebMessageHost>(*log_);
  }

 private:
  raw_ref<MessageLog> log_;
  base::WeakPtrFactory<CountingWebMessageHostFactory> weak_factory_{this};
};

}  // namespace

// Drives real JsToBrowserMessaging objects through mojo, with real
// RenderFrameHosts. The renderer side of js_injection (JsCommunication,
// which installs the JS binding) is only created by WebView's renderer
// client, so a content-shell test cannot reach these objects from page
// JavaScript; driving the mojo endpoint directly exercises the same
// browser-side dispatch, including the frame lifecycle states.
class JsToBrowserMessagingBrowserTest : public content::ContentBrowserTest {
 public:
  JsToBrowserMessagingBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&JsToBrowserMessagingBrowserTest::web_contents,
                                base::Unretained(this))) {
    // Some bots, e.g. low-memory Android, disable BackForwardCache by default.
    feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }
  ~JsToBrowserMessagingBrowserTest() override = default;

 protected:
  content::WebContents* web_contents() { return shell()->web_contents(); }
  MessageLog& log() { return log_; }

  // Creates the messaging object the embedder would create for `rfh`, with
  // a fresh factory. `remote` is the test's end of the mojo endpoint.
  std::unique_ptr<JsToBrowserMessaging> CreateMessaging(
      content::RenderFrameHost* rfh,
      mojo::AssociatedRemote<mojom::JsToBrowserMessaging>* remote) {
    factory_ = std::make_unique<CountingWebMessageHostFactory>(log_);
    origin_matcher::OriginMatcher origin_matcher;
    CHECK(origin_matcher.AddRuleFromString("*"));
    auto messaging = std::make_unique<JsToBrowserMessaging>(
        rfh, remote->BindNewEndpointAndPassDedicatedReceiver(),
        mojo::PendingAssociatedRemote<mojom::BrowserToJsMessagingFactory>(),
        factory_->GetWeakPtr(), origin_matcher, u"testObject",
        /*world_id=*/0);
    // Mirrors the renderer's window-object-cleared notification, which
    // installs the reply proxy PostMessage() requires.
    messaging->SetBrowserToJsMessaging({});
    return messaging;
  }

  void PostMessage(mojo::AssociatedRemote<mojom::JsToBrowserMessaging>* remote,
                   std::u16string payload) {
    blink::WebMessagePayload message = std::move(payload);
    (*remote)->PostMessage(std::move(message),
                           std::vector<blink::MessagePortDescriptor>());
  }

  // Simulates RemoveWebMessageHostFactory() destroying the factory while
  // messaging objects that hold its WeakPtr survive.
  void DestroyFactory() { factory_.reset(); }

  // Flushes messages already sent on `remote`. The flush is ordered
  // after them on the same pipe, so a later count check cannot miss
  // a delivery that was already in flight.
  void FlushPendingMessages(
      mojo::AssociatedRemote<mojom::JsToBrowserMessaging>* remote) {
    remote->FlushForTesting();
  }

  content::test::PrerenderTestHelper prerender_helper_;

 private:
  // Initialized after `prerender_helper_`'s, so must be destroyed first.
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<CountingWebMessageHostFactory> factory_;
  MessageLog log_;
};

// A message posted after the factory is removed must be dropped, not
// delivered to the host created before removal. The frame stays in the
// BackForwardCache across removal, which only happens if the removal-time
// flush is skipped (kill switch off) or bypassed.
IN_PROC_BROWSER_TEST_F(JsToBrowserMessagingBrowserTest,
                       PostMessageAfterRemovalFromBackForwardCacheDropped) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(content::NavigateToURL(shell(), url_a));
  content::RenderFrameHostWrapper rfh_a(web_contents()->GetPrimaryMainFrame());

  mojo::AssociatedRemote<mojom::JsToBrowserMessaging> remote;
  std::unique_ptr<JsToBrowserMessaging> messaging =
      CreateMessaging(rfh_a.get(), &remote);
  PostMessage(&remote, u"first");
  log().WaitForCount(1);
  ASSERT_EQ(std::vector<std::u16string>({u"first"}), log().payloads);

  GURL url_b = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(content::NavigateToURL(shell(), url_b));
  ASSERT_FALSE(rfh_a.IsDestroyed());
  ASSERT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  DestroyFactory();

  ASSERT_TRUE(content::HistoryGoBack(web_contents()));
  ASSERT_EQ(rfh_a.get(), web_contents()->GetPrimaryMainFrame());

  PostMessage(&remote, u"second");
  FlushPendingMessages(&remote);
  EXPECT_EQ(1, log().count);
  EXPECT_EQ(std::vector<std::u16string>({u"first"}), log().payloads);
}

// Messages queued while prerendering are flushed when the page activates.
// Without removal they must be delivered; this is the control case for
// the removal test below.
IN_PROC_BROWSER_TEST_F(JsToBrowserMessagingBrowserTest,
                       QueuedMessageDeliveredOnPrerenderActivation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(content::NavigateToURL(shell(), url_a));

  GURL url_b = embedded_test_server()->GetURL("/title2.html");
  content::PrerenderHostId host_id = prerender_helper_.AddPrerender(url_b);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper_.GetPrerenderedMainFrameHost(host_id);
  ASSERT_TRUE(prerender_rfh);
  EXPECT_EQ(prerender_rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kPrerendering);

  mojo::AssociatedRemote<mojom::JsToBrowserMessaging> remote;
  std::unique_ptr<JsToBrowserMessaging> messaging =
      CreateMessaging(prerender_rfh, &remote);
  PostMessage(&remote, u"queued");
  FlushPendingMessages(&remote);
  EXPECT_EQ(0, log().count);

  prerender_helper_.NavigatePrimaryPage(url_b);
  messaging->OnRenderFrameHostActivated();
  log().WaitForCount(1);
  EXPECT_EQ(std::vector<std::u16string>({u"queued"}), log().payloads);
}

// Messages queued while prerendering must be dropped when the page is
// activated after the factory is removed, and a message posted after
// activation must be dropped too.
IN_PROC_BROWSER_TEST_F(JsToBrowserMessagingBrowserTest,
                       QueuedMessageAfterRemovalOnPrerenderActivationDropped) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(content::NavigateToURL(shell(), url_a));

  GURL url_b = embedded_test_server()->GetURL("/title2.html");
  content::PrerenderHostId host_id = prerender_helper_.AddPrerender(url_b);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper_.GetPrerenderedMainFrameHost(host_id);
  ASSERT_TRUE(prerender_rfh);
  EXPECT_EQ(prerender_rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kPrerendering);

  mojo::AssociatedRemote<mojom::JsToBrowserMessaging> remote;
  std::unique_ptr<JsToBrowserMessaging> messaging =
      CreateMessaging(prerender_rfh, &remote);
  PostMessage(&remote, u"queued");
  FlushPendingMessages(&remote);
  EXPECT_EQ(0, log().count);

  DestroyFactory();

  prerender_helper_.NavigatePrimaryPage(url_b);
  messaging->OnRenderFrameHostActivated();
  EXPECT_EQ(0, log().count);

  PostMessage(&remote, u"after");
  FlushPendingMessages(&remote);
  EXPECT_EQ(0, log().count);
  EXPECT_TRUE(log().payloads.empty());
}

}  // namespace js_injection
