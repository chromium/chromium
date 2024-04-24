// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "components/js_injection/browser/js_communication_host.h"
#include "components/js_injection/browser/navigation_web_message_sender.h"
#include "components/js_injection/browser/web_message.h"
#include "components/js_injection/browser/web_message_host.h"
#include "components/js_injection/browser/web_message_host_factory.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace js_injection {

using HostToken = base::UnguessableToken;
// Listens to navigation messages and queues them per-WebMessageHost. The queued
// messages can be read in sequence for each host. It's useful to queue the
// messages per-host, since some messages can arrive interleaved with messages
// intended for other hosts, e.g. PAGE_DELETED messages can fire a bit later
// than the next page's NAVIGATION_COMPLETED message, if the RenderFrameHost
// changes and the deletion of the previous RenderFrameHost happens after
// the new RenderFrameHost's navigation committed. However, it's guaranteed
// that the order within the same host stays consistent (NAVIGATION_COMPLETED &
// PAGE_LOAD_END won't fire after PAGE_DELETED) and only PAGE_DELETED can be
// fired for the old page after the new page's NAVIGATION_COMPLETED.
class NavigationMessageListener {
 public:
  NavigationMessageListener() = default;
  ~NavigationMessageListener() = default;

  // Returns the index for the next message for `host` in its message queue.
  size_t GetNextMessageIndexForHost(const HostToken& host) {
    CHECK(message_queues_.contains(host));
    return message_queues_[host].next_message_index;
  }

  // Returns true if there's at least 1 new queued message for `host`.
  bool HasNextMessageForHost(const HostToken& host) {
    return message_queues_[host].messages.size() >
           GetNextMessageIndexForHost(host);
  }

  // Returns true if there's at least 1 new queued message for any host.
  bool HasNextMessageForAnyHost() {
    for (const auto& host_message_pair : message_queues_) {
      if (HasNextMessageForHost(host_message_pair.first)) {
        return true;
      }
    }
    return false;
  }

  // Returns the first message in the queue for `host`.
  WebMessage* NextMessageForHost(const HostToken& host) {
    CHECK(HasNextMessageForHost(host));
    size_t message_index = GetNextMessageIndexForHost(host);
    message_queues_[host].next_message_index = message_index + 1;
    return message_queues_[host].messages.at(message_index).get();
  }

  // Waits until there's a message waiting for `host`.
  void WaitForNextMessageForHost(const HostToken& host) {
    if (HasNextMessageForHost(host)) {
      return;
    }
    message_queues_[host].message_waiter = std::make_unique<base::RunLoop>();
    message_queues_[host].message_waiter->Run();
    CHECK(HasNextMessageForHost(host));
  }

  // Waits until there's a message waiting in any host's queue. Useful when
  // navigating to a new page and the WebMessageHost is not known yet.
  void WaitForNextMessageForAnyHost() {
    if (HasNextMessageForAnyHost()) {
      return;
    }

    any_message_waiter_ = std::make_unique<base::RunLoop>();
    any_message_waiter_->Run();
  }

  // The message `message` has been posted to the FakeWebMessageHost associated
  // with `host`. Add the message to that host's queue.
  void OnPostMessage(const HostToken& host,
                     std::unique_ptr<WebMessage> message) {
    message_queues_[host].messages.push_back(std::move(message));
    // We received a new message for `host`, so we can unblock calls to
    // `WaitForNextMessage*()` if needed.
    if (message_queues_[host].message_waiter.get()) {
      message_queues_[host].message_waiter->Quit();
    }
    if (any_message_waiter_.get()) {
      any_message_waiter_->Quit();
    }
  }

 private:
  struct MessageQueue {
    std::unique_ptr<base::RunLoop> message_waiter;
    std::vector<std::unique_ptr<WebMessage>> messages;
    int next_message_index = 0;
  };
  std::map<HostToken, MessageQueue> message_queues_;

  std::unique_ptr<base::RunLoop> any_message_waiter_;
};

class FakeWebMessageHost : public WebMessageHost {
 public:
  explicit FakeWebMessageHost(NavigationMessageListener* listener)
      : listener_(listener) {}
  ~FakeWebMessageHost() override = default;

  // WebMessageHost overrides:
  void OnPostMessage(std::unique_ptr<WebMessage> message) override {
    listener_->OnPostMessage(token(), std::move(message));
  }

  base::UnguessableToken token() const { return token_; }

 private:
  base::UnguessableToken token_ = base::UnguessableToken::Create();
  raw_ptr<NavigationMessageListener> listener_;
};

class FakeWebMessageHostFactory : public WebMessageHostFactory {
 public:
  explicit FakeWebMessageHostFactory(NavigationMessageListener* listener)
      : listener_(listener) {}
  ~FakeWebMessageHostFactory() override = default;

  // WebMessageHostFactory overrides:
  std::unique_ptr<WebMessageHost> CreateHost(
      const std::string& top_level_origin_string,
      const std::string& origin_string,
      bool is_main_frame,
      WebMessageReplyProxy* proxy) override {
    return std::make_unique<FakeWebMessageHost>(listener_);
  }

 private:
  raw_ptr<NavigationMessageListener> listener_;
};

class NavigationListenerBrowserTest : public content::ContentBrowserTest,
                                      public testing::WithParamInterface<bool> {
 public:
  NavigationListenerBrowserTest() {
    features_.InitAndEnableFeature(features::kEnableNavigationListener);
  }
  ~NavigationListenerBrowserTest() override = default;

 protected:
  // content::ContentBrowserTest overrides:
  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
  }
  void TearDownOnMainThread() override {
    content::ContentBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* web_contents() { return shell()->web_contents(); }

  NavigationMessageListener& listener() { return listener_; }

  bool UseBackForwardCacheDisablerListener() const { return GetParam(); }

  bool IsBackForwardCacheDisabled() const {
    return UseBackForwardCacheDisablerListener() ||
           !base::FeatureList::IsEnabled(features::kBackForwardCache);
  }

  void SetupNavigationListener() {
    if (!js_communication_host_.get()) {
      js_communication_host_ =
          std::make_unique<JsCommunicationHost>(web_contents());
    }
    js_communication_host_->AddWebMessageHostFactory(
        std::make_unique<FakeWebMessageHostFactory>(&listener()),
        UseBackForwardCacheDisablerListener()
            ? NavigationWebMessageSender::
                  kNavigationListenerDisableBFCacheObjectName
            : NavigationWebMessageSender::
                  kNavigationListenerAllowBFCacheObjectName,
        {"*"});
  }

  HostToken GetCurrentHostToken() {
    NavigationWebMessageSender* sender = NavigationWebMessageSender::GetForPage(
        web_contents()->GetPrimaryPage());
    return static_cast<FakeWebMessageHost*>(
               sender->GetWebMessageHostForTesting())
        ->token();
  }

  HostToken NavigateToFirstPage() {
    NavigationWebMessageSender* sender0 =
        NavigationWebMessageSender::GetForPage(
            web_contents()->GetPrimaryPage());
    EXPECT_TRUE(sender0);
    HostToken host0 =
        static_cast<FakeWebMessageHost*>(sender0->GetWebMessageHostForTesting())
            ->token();
    CheckNavigationMessage(listener().NextMessageForHost(host0),
                           NavigationWebMessageSender::kOptedInMessage);
    GURL navigation_url = embedded_test_server()->GetURL("/title1.html");
    EXPECT_TRUE(content::NavigateToURL(shell(), navigation_url));
    NavigationWebMessageSender* sender = NavigationWebMessageSender::GetForPage(
        web_contents()->GetPrimaryPage());
    EXPECT_TRUE(sender);
    HostToken host =
        static_cast<FakeWebMessageHost*>(sender->GetWebMessageHostForTesting())
            ->token();
    CheckNavigationMessage(listener().NextMessageForHost(host0),
                           NavigationWebMessageSender::kPageDeletedMessage);
    CheckNavigationCompletedMessage(listener().NextMessageForHost(host),
                                    /*url=*/navigation_url,
                                    /*is_same_document=*/false,
                                    /*is_page_initiated=*/false,
                                    /*is_error_page=*/false,
                                    /*is_reload=*/false,
                                    /*is_history=*/false,
                                    /*committed=*/true,
                                    /*status_code=*/200);
    CheckNavigationMessage(listener().NextMessageForHost(host),
                           NavigationWebMessageSender::kPageLoadEndMessage);
    EXPECT_FALSE(listener().HasNextMessageForHost(host));
    return host;
  }

  void CheckNavigationMessage(WebMessage* message, std::string type) {
    base::Value::Dict expected_dict;
    expected_dict.Set("type", type);
    ASSERT_EQ(
        NavigationWebMessageSender::CreateWebMessage(std::move(expected_dict))
            ->message,
        message->message);
  }

  void CheckNavigationCompletedMessage(WebMessage* message,
                                       GURL& url,
                                       bool is_same_document,
                                       bool is_page_initiated,
                                       bool is_error_page,
                                       bool is_reload,
                                       bool is_history,
                                       bool committed,
                                       int status_code) {
    base::Value::Dict message_dict =
        base::Value::Dict()
            .Set("type",
                 NavigationWebMessageSender::kNavigationCompletedMessage)
            .Set("url", url.spec())
            .Set("isSameDocument", is_same_document)
            .Set("isPageInitiated", is_page_initiated)
            .Set("isErrorPage", is_error_page)
            .Set("isReload", is_reload)
            .Set("isHistory", is_history)
            .Set("committed", committed)
            .Set("statusCode", status_code);
    ASSERT_EQ(
        NavigationWebMessageSender::CreateWebMessage(std::move(message_dict))
            ->message,
        message->message);
  }

 private:
  std::unique_ptr<JsCommunicationHost> js_communication_host_;
  NavigationMessageListener listener_;
  base::test::ScopedFeatureList features_;
};

// Test that adding the special navigationListener will result in receiving
// navigation messages for a variety of navigation cases:
// 1) Regular navigation
// 2) Reload
// 3) Same-document navigation
// 4) Same-document history navigation
// 5) Failed navigation resulting in an error page.
IN_PROC_BROWSER_TEST_P(NavigationListenerBrowserTest, Basic) {
  // Setup the navigation listener.
  SetupNavigationListener();
  // A NavigationWebMessageSender & FakeWebMessageHost is immediately created
  // for the initial empty document.
  HostToken host0 = GetCurrentHostToken();
  CheckNavigationMessage(listener().NextMessageForHost(host0),
                         NavigationWebMessageSender::kOptedInMessage);

  // Navigation #1: navigate to the first page.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL navigation_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(content::NavigateToURL(shell(), navigation_url));

  // A new NavigationWebMessageSender with a new FakeWebMessageHost has been
  // created for the new page.
  HostToken host1 = GetCurrentHostToken();

  // Assert that the initial empty document's page got deleted, because we
  // committed a new page.
  CheckNavigationMessage(listener().NextMessageForHost(host0),
                         NavigationWebMessageSender::kPageDeletedMessage);

  // Assert that we got navigation messages for the first navigation.
  CheckNavigationCompletedMessage(listener().NextMessageForHost(host1),
                                  /*url=*/navigation_url,
                                  /*is_same_document=*/false,
                                  /*is_page_initiated=*/false,
                                  /*is_error_page=*/false,
                                  /*is_reload=*/false,
                                  /*is_history=*/false,
                                  /*committed=*/true,
                                  /*status_code=*/200);
  CheckNavigationMessage(listener().NextMessageForHost(host1),
                         NavigationWebMessageSender::kPageLoadEndMessage);
  ASSERT_FALSE(listener().HasNextMessageForHost(host1));

  // Navigation #2: Reload the first page.
  content::ReloadBlockUntilNavigationsComplete(shell(), 1);

  // A new NavigationWebMessageSender with a new FakeWebMessageHost has been
  // created for the new page.
  HostToken host2 = GetCurrentHostToken();

  // The previous page has been deleted.
  listener().WaitForNextMessageForHost(host1);
  CheckNavigationMessage(listener().NextMessageForHost(host1),
                         NavigationWebMessageSender::kPageDeletedMessage);

  // Navigation messages for the reload.
  CheckNavigationCompletedMessage(listener().NextMessageForHost(host2),
                                  /*url=*/navigation_url,
                                  /*is_same_document=*/false,
                                  /*is_page_initiated=*/false,
                                  /*is_error_page=*/false,
                                  /*is_reload=*/true,
                                  /*is_history=*/false,
                                  /*committed=*/true,
                                  /*status_code=*/200);
  CheckNavigationMessage(listener().NextMessageForHost(host2),
                         NavigationWebMessageSender::kPageLoadEndMessage);
  ASSERT_FALSE(listener().HasNextMessageForAnyHost());

  // Navigation #3: Navigate same-document.
  GURL navigation_url_foo = embedded_test_server()->GetURL("/title1.html#foo");
  ASSERT_TRUE(content::NavigateToURL(shell(), navigation_url_foo));

  // The previous `host2` is reused as the navigation stays on the same page.
  // Also, no PAGE_LOAD_END since the navigation doesn't load a new page.
  CheckNavigationCompletedMessage(listener().NextMessageForHost(host2),
                                  /*url=*/navigation_url_foo,
                                  /*is_same_document=*/true,
                                  /*is_page_initiated=*/false,
                                  /*is_error_page=*/false,
                                  /*is_reload=*/false,
                                  /*is_history=*/false,
                                  /*committed=*/true,
                                  /*status_code=*/200);
  ASSERT_FALSE(listener().HasNextMessageForAnyHost());

  // Navigation #4: Navigate history same-document.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  CheckNavigationCompletedMessage(listener().NextMessageForHost(host2),
                                  /*url=*/navigation_url,
                                  /*is_same_document=*/true,
                                  /*is_page_initiated=*/false,
                                  /*is_error_page=*/false,
                                  /*is_reload=*/false,
                                  /*is_history=*/true,
                                  /*committed=*/true,
                                  /*status_code=*/200);
  ASSERT_FALSE(listener().HasNextMessageForAnyHost());

  // Navigation #5: Navigate to error page.
  GURL error_url = embedded_test_server()->GetURL("/not-found");
  ASSERT_FALSE(content::NavigateToURL(shell(), error_url));

  // A new NavigationWebMessageSender with a new FakeWebMessageHost has been
  // created for the new error page.
  HostToken host3 = GetCurrentHostToken();

  if (IsBackForwardCacheDisabled()) {
    // The previous page will be deleted, unless it gets into the back/forward
    // cache.
    listener().WaitForNextMessageForHost(host2);
    CheckNavigationMessage(listener().NextMessageForHost(host2),
                           NavigationWebMessageSender::kPageDeletedMessage);
  }

  CheckNavigationCompletedMessage(listener().NextMessageForHost(host3),
                                  /*url=*/error_url,
                                  /*is_same_document=*/false,
                                  /*is_page_initiated=*/false,
                                  /*is_error_page=*/true,
                                  /*is_reload=*/false,
                                  /*is_history=*/false,
                                  /*committed=*/true,
                                  /*status_code=*/404);
  CheckNavigationMessage(listener().NextMessageForHost(host3),
                         NavigationWebMessageSender::kPageLoadEndMessage);

  // No further messages.
  ASSERT_FALSE(listener().HasNextMessageForAnyHost());
}

// Test navigation messages when navigating away from a page that hasn't fired
// the load event.
IN_PROC_BROWSER_TEST_P(NavigationListenerBrowserTest, NoLoadEnd) {
  std::string page_with_infinite_loading_image = "/page_with_loading_image";
  std::string infinite_loading_image = "/image";
  net::test_server::ControllableHttpResponse page_request(
      embedded_test_server(), page_with_infinite_loading_image);
  net::test_server::ControllableHttpResponse image_request(
      embedded_test_server(), infinite_loading_image);
  ASSERT_TRUE(embedded_test_server()->Start());
  SetupNavigationListener();
  HostToken host0 = GetCurrentHostToken();
  CheckNavigationMessage(listener().NextMessageForHost(host0),
                         NavigationWebMessageSender::kOptedInMessage);

  // Navigation #1: navigate to the first page, which has an infinitely loading
  // image.
  GURL url_with_infinite_load =
      embedded_test_server()->GetURL(page_with_infinite_loading_image);
  GURL image_url = embedded_test_server()->GetURL(infinite_loading_image);
  ASSERT_TRUE(web_contents()->GetController().LoadURL(
      url_with_infinite_load, content::Referrer(), ui::PAGE_TRANSITION_TYPED,
      std::string()));
  // Send the response for the navigation, but not for the image request. This
  // will cause the page to not get the load event, since the image is still
  // loading. Also, don't finish sending the page's body, so that the page won't
  // get into BFCache (otherwise the test might be flaky with a PAGE_DELETED
  // call that might or might not pop up depending on whether the
  // DOMContentLoaded event arrived before we navigate away again or not).
  page_request.WaitForRequest();
  page_request.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<html><body>"
      "<script> window.onload = () => { document.title = 'loaded'; } </script>"
      "<img src='" +
      image_url.spec() + "'/>");
  image_request.WaitForRequest();

  // Wait for the next two navigation messages, after which we know that a new
  // NavigationWebMessageSender should already be created.
  listener().WaitForNextMessageForAnyHost();
  // The first message will indicate that the previous page has been deleted.
  CheckNavigationMessage(listener().NextMessageForHost(host0),
                         NavigationWebMessageSender::kPageDeletedMessage);

  // The next messages will be for the navigation above.
  HostToken host = GetCurrentHostToken();
  CheckNavigationCompletedMessage(listener().NextMessageForHost(host),
                                  /*url=*/url_with_infinite_load,
                                  /*is_same_document=*/false,
                                  /*is_page_initiated=*/false,
                                  /*is_error_page=*/false,
                                  /*is_reload=*/false,
                                  /*is_history=*/false,
                                  /*committed=*/true,
                                  /*status_code=*/200);

  // Assert that there's no PAGE_LOAD_END message and the load event didn't fire
  // in the document.
  ASSERT_FALSE(listener().HasNextMessageForHost(host));
  ASSERT_NE("loaded",
            content::EvalJs(web_contents(), "document.title").ExtractString());

  // Navigation #2: navigate to the second page before the first page finished
  // loading.
  GURL navigation_url_2 = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(web_contents()->GetController().LoadURL(
      navigation_url_2, content::Referrer(), ui::PAGE_TRANSITION_TYPED,
      std::string()));

  // The previous page will be deleted, as the document body never fully loaded.
  listener().WaitForNextMessageForHost(host);
  CheckNavigationMessage(listener().NextMessageForHost(host),
                         NavigationWebMessageSender::kPageDeletedMessage);

  // No PAGE_LOAD_END messages for the previous page.
  ASSERT_FALSE(listener().HasNextMessageForHost(host));

  // Wait for the next navigation message, after which we know that a new
  // NavigationWebMessageSender should already be created.
  listener().WaitForNextMessageForAnyHost();
  HostToken host2 = GetCurrentHostToken();
  ASSERT_NE(host, host2);

  // Assert that we got navigation messages for the second navigation.
  listener().WaitForNextMessageForHost(host2);
  CheckNavigationCompletedMessage(listener().NextMessageForHost(host2),
                                  /*url=*/navigation_url_2,
                                  /*is_same_document=*/false,
                                  /*is_page_initiated=*/false,
                                  /*is_error_page=*/false,
                                  /*is_reload=*/false,
                                  /*is_history=*/false,
                                  /*committed=*/true,
                                  /*status_code=*/200);
  // The PAGE_LOAD_END should be for the second page.
  listener().WaitForNextMessageForHost(host2);
  CheckNavigationMessage(listener().NextMessageForHost(host2),
                         NavigationWebMessageSender::kPageLoadEndMessage);

  // No other messages after this.
  ASSERT_FALSE(listener().HasNextMessageForAnyHost());
}

// Test navigation messages when a new renderer-initiated same-document
// navigation happens while a cross-document navigation is ongoing and hasn't
// received its network response. Both navigations should commit.
IN_PROC_BROWSER_TEST_P(NavigationListenerBrowserTest,
                       NewRendererInitiatedSameDocNavDuringCrossDocNav) {
  std::string page_with_delayed_response = "/page_with_delayed_response";
  net::test_server::ControllableHttpResponse request_to_delay(
      embedded_test_server(), page_with_delayed_response);
  ASSERT_TRUE(embedded_test_server()->Start());
  SetupNavigationListener();

  // Navigation #1: Navigate to the first page. This is needed because doing
  // same-document navigations from the initial empty document behaves slightly
  // differently from normal same-document navigations, so we want to avoid
  // doing it from the initial empty document.
  HostToken host = NavigateToFirstPage();

  // Navigation #2: Navigate to another page, which will have a slightly delayed
  // response, so that we can start another navigation before this one commits.
  GURL navigation_url_2 =
      embedded_test_server()->GetURL(page_with_delayed_response);

  ASSERT_TRUE(web_contents()->GetController().LoadURL(
      navigation_url_2, content::Referrer(), ui::PAGE_TRANSITION_TYPED,
      std::string()));

  // Before the navigation above finishes, do a same-document navigation.
  ASSERT_TRUE(content::ExecJs(web_contents(), "location.hash = '#foo';"));
  listener().WaitForNextMessageForHost(host);

  // Check that the same-document navigation committed successfully.
  GURL navigation_url_foo = embedded_test_server()->GetURL("/title1.html#foo");
  CheckNavigationCompletedMessage(listener().NextMessageForHost(host),
                                  /*url=*/navigation_url_foo,
                                  /*is_same_document=*/true,
                                  /*is_page_initiated=*/true,
                                  /*is_error_page=*/false,
                                  /*is_reload=*/false,
                                  /*is_history=*/false,
                                  /*committed=*/true,
                                  /*status_code=*/200);
  ASSERT_FALSE(listener().HasNextMessageForHost(host));

  // Send the response for the cross-document navigation after the same-document
  // navigation finished.
  request_to_delay.WaitForRequest();
  request_to_delay.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<html><body></body></html>");
  request_to_delay.Done();
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));

  // A new NavigationWebMessageSender has been created for the new page.
  HostToken host2 = GetCurrentHostToken();

  if (IsBackForwardCacheDisabled()) {
    // The previous page will be deleted, unless it gets into the back/forward
    // cache.
    listener().WaitForNextMessageForHost(host);
    CheckNavigationMessage(listener().NextMessageForHost(host),
                           NavigationWebMessageSender::kPageDeletedMessage);
  }

  // Check that the cross-document navigation committed successfully.
  CheckNavigationCompletedMessage(listener().NextMessageForHost(host2),
                                  /*url=*/navigation_url_2,
                                  /*is_same_document=*/false,
                                  /*is_page_initiated=*/false,
                                  /*is_error_page=*/false,
                                  /*is_reload=*/false,
                                  /*is_history=*/false,
                                  /*committed=*/true,
                                  /*status_code=*/200);
  CheckNavigationMessage(listener().NextMessageForHost(host2),
                         NavigationWebMessageSender::kPageLoadEndMessage);

  ASSERT_FALSE(listener().HasNextMessageForAnyHost());
}

// Test navigation messages when a new browser-initiated same-document
// navigation happens while a cross-document navigation is ongoing and hasn't
// received its network response. Different than above, the newer same-document
// navigation should cancel the first navigation here, since the newer
// NavigationHandle will take the place of the first NavigationHandle.
IN_PROC_BROWSER_TEST_P(NavigationListenerBrowserTest,
                       NewBrowserInitiatedSameDocNavDuringCrossDocNav) {
  std::string page_with_delayed_response = "/page_with_delayed_response";
  net::test_server::ControllableHttpResponse request_to_delay(
      embedded_test_server(), page_with_delayed_response);
  ASSERT_TRUE(embedded_test_server()->Start());
  SetupNavigationListener();

  // Navigation #1: Navigate to the first page. This is needed because doing
  // same-document navigations from the initial empty document behaves slightly
  // differently from normal same-document navigations, so we want to avoid
  // doing it from the initial empty document.
  HostToken host = NavigateToFirstPage();

  // Navigation #2: Navigate to another page, which will have a slightly delayed
  // response, so that we can start another navigation before this one finishes.
  GURL navigation_url_2 =
      embedded_test_server()->GetURL(page_with_delayed_response);
  ASSERT_TRUE(web_contents()->GetController().LoadURL(
      navigation_url_2, content::Referrer(), ui::PAGE_TRANSITION_TYPED,
      std::string()));
  // Ensure that the network request has been received.
  request_to_delay.WaitForRequest();

  // Before the navigation above finishes, do a same-document navigation.
  GURL navigation_url_foo = embedded_test_server()->GetURL("/title1.html#foo");
  ASSERT_TRUE(web_contents()->GetController().LoadURL(
      navigation_url_foo, content::Referrer(), ui::PAGE_TRANSITION_TYPED,
      std::string()));
  listener().WaitForNextMessageForHost(host);

  // Check that the slow cross-document navigation got canceled, because the new
  // same-document navigation created a new NavigationHandle, deleting the
  // earlier navigation's NavigationHandle.
  CheckNavigationCompletedMessage(listener().NextMessageForHost(host),
                                  /*url=*/navigation_url_2,
                                  /*is_same_document=*/false,
                                  /*is_page_initiated=*/false,
                                  /*is_error_page=*/false,
                                  /*is_reload=*/false,
                                  /*is_history=*/false,
                                  /*committed=*/false,
                                  /*status_code=*/200);

  listener().WaitForNextMessageForHost(host);
  // Check that the same-document navigation committed successfully.
  CheckNavigationCompletedMessage(listener().NextMessageForHost(host),
                                  /*url=*/navigation_url_foo,
                                  /*is_same_document=*/true,
                                  /*is_page_initiated=*/false,
                                  /*is_error_page=*/false,
                                  /*is_reload=*/false,
                                  /*is_history=*/false,
                                  /*committed=*/true,
                                  /*status_code=*/200);

  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  ASSERT_FALSE(listener().HasNextMessageForAnyHost());
}

// Test navigation messages when a new cross-document navigation happens while
// an earlier cross-document navigation is ongoing and hasn't received its
// network response. The newer navigation should cancel the first navigation
// here, since the newer NavigationHandle will take the place of the first
// NavigationHandle. This behavior will be the same regardless of whether the
// newer navigation is browser- or renderer-initiated.
IN_PROC_BROWSER_TEST_P(NavigationListenerBrowserTest,
                       NewCrossDocNavDuringCrossDocNav) {
  std::string page_with_delayed_response = "/page_with_delayed_response";
  net::test_server::ControllableHttpResponse request_to_delay(
      embedded_test_server(), page_with_delayed_response);
  ASSERT_TRUE(embedded_test_server()->Start());
  SetupNavigationListener();
  HostToken host = GetCurrentHostToken();
  CheckNavigationMessage(listener().NextMessageForHost(host),
                         NavigationWebMessageSender::kOptedInMessage);

  // Navigate to another page, which will have a slightly delayed response, so
  // that we can start another navigation before this one finishes.
  GURL navigation_url_2 =
      embedded_test_server()->GetURL(page_with_delayed_response);
  ASSERT_TRUE(web_contents()->GetController().LoadURL(
      navigation_url_2, content::Referrer(), ui::PAGE_TRANSITION_TYPED,
      std::string()));
  // Ensure that the network request has been received.
  request_to_delay.WaitForRequest();

  // Before the navigation above finishes, do another cross-document navigation.
  GURL navigation_url_3 = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(web_contents()->GetController().LoadURL(
      navigation_url_3, content::Referrer(), ui::PAGE_TRANSITION_TYPED,
      std::string()));
  listener().WaitForNextMessageForHost(host);

  // Check that the earlier cross-document navigation got canceled, because the
  // new navigation created a new NavigationHandle, deleting the earlier
  // navigation's NavigationHandle.
  CheckNavigationCompletedMessage(listener().NextMessageForHost(host),
                                  /*url=*/navigation_url_2,
                                  /*is_same_document=*/false,
                                  /*is_page_initiated=*/false,
                                  /*is_error_page=*/false,
                                  /*is_reload=*/false,
                                  /*is_history=*/false,
                                  /*committed=*/false,
                                  /*status_code=*/200);

  // Wait for the newer navigation to finish.
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));

  // A new NavigationWebMessageSender has been created for the new page.
  HostToken host2 = GetCurrentHostToken();

  // Check that the newer cross-document navigation committed successfully.
  CheckNavigationMessage(listener().NextMessageForHost(host),
                         NavigationWebMessageSender::kPageDeletedMessage);
  CheckNavigationCompletedMessage(listener().NextMessageForHost(host2),
                                  /*url=*/navigation_url_3,
                                  /*is_same_document=*/false,
                                  /*is_page_initiated=*/false,
                                  /*is_error_page=*/false,
                                  /*is_reload=*/false,
                                  /*is_history=*/false,
                                  /*committed=*/true,
                                  /*status_code=*/200);
  CheckNavigationMessage(listener().NextMessageForHost(host2),
                         NavigationWebMessageSender::kPageLoadEndMessage);

  ASSERT_FALSE(listener().HasNextMessageForAnyHost());
}

INSTANTIATE_TEST_SUITE_P(All,
                         NavigationListenerBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return base::StringPrintf(
                               "%s", info.param ? "listener_disables_bfcache"
                                                : "listener_allows_bfcache");
                         });

}  // namespace js_injection
