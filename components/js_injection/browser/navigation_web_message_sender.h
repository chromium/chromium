// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_BROWSER_NAVIGATION_WEB_MESSAGE_SENDER_H_
#define COMPONENTS_JS_INJECTION_BROWSER_NAVIGATION_WEB_MESSAGE_SENDER_H_

#include <string>

#include "base/values.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class NavigationHandle;
class Page;
}  // namespace content

namespace features {

// Enable creation of this class for special navigation listeners.
BASE_DECLARE_FEATURE(kEnableNavigationListener);

}  // namespace features

namespace js_injection {

class EmptyReplyProxy;
class WebMessageHost;
class WebMessageHostFactory;
struct WebMessage;

// A special interceptor for "navigation listener" WebMessageListeners where
// instead of establishing a connection with the renderer, a navigation observer
// is created on the browser, to inject navigation-related notification
// messages to the embedder.
//
// The NavigationWebMessageSender is 1:1 with Page, and navigation messages
// related to a certain Page will be sent by the NavigationWebMessageSender
// associated with that Page.
// See https://crbug.com/332809183 on why this is needed.
class NavigationWebMessageSender
    : public content::PageUserData<NavigationWebMessageSender>,
      public content::WebContentsObserver {
 public:
  // Depending on which object name is used, BFCache might be disallowed. If
  // `kNavigationListenerDisableBFCacheObjectName` is used, the client requests
  // to disable BFCache, so it will ensure all pages can't be BFCached. We
  // provide this functionality so that if the client can't handle BFCache yet
  // (due to needing to update client-side code to handle reusing pages, etc)
  // while the BFCache flag is already enabled on the WebView side (e.g. during
  // random experimentation), it won't get confused  with the ordering of events
  // (e.g. PAGE_DELETED not getting called for a long time). If the client uses
  // `kNavigationListenerAllowBFCacheObjectName` instead, it signals that it can
  // handle BFCache cases correctly, so pages are allowed to enter BFCache (if
  // the BFCache feature is turned on).
  static const char16_t kNavigationListenerAllowBFCacheObjectName[];
  static const char16_t kNavigationListenerDisableBFCacheObjectName[];

  // === Various messages that can be dispatched to the client ===

  // Only dispatched once globally, when the first NavigationWebMessageSender is
  // created. This indicates to the client that the special navigation listeners
  // are implemented (it might not be available in older versions).
  static const char kOptedInMessage[];

  // The navigation messages will contain details of the navigation like the
  // URL, whether the navigation is same-document or not, etc.
  //
  // Indicates that a navigation has started. This is dispatched on
  // `DidStartNavigation()`.
  static const char kNavigationStartedMessage[];
  // Indicates that a navigation has been redirected. This is dispatched on
  // `DidStartNavigation()`.
  static const char kNavigationRedirectedMessage[];
  // Indicates that a navigation has completed. This is dispatched on
  // `DidFinishNavigation()`.
  static const char kNavigationCompletedMessage[];

  // Indicates that the page has finished loading. This is dispatched on
  // `DidFinishLoad()`.
  static const char kPageLoadEndMessage[];
  // Indicates that the page has been deleted. This is dispatched from the class
  // destructor, since this is a PageUserData. If the page is BFCached, this
  // will be when the page is evicted. Otherwise, it will be when the primary
  // Page changed after a cross-document navigation away from this apge.
  static const char kPageDeletedMessage[];

  static bool IsNavigationListener(const std::u16string& js_object_name);
  static void CreateForPageIfNeeded(content::Page& page,
                                    const std::u16string& js_object_name,
                                    WebMessageHostFactory* factory);

  ~NavigationWebMessageSender() override;

  void DispatchOptInMessage();

 private:
  friend class PageUserData<NavigationWebMessageSender>;
  friend class NavigationListenerBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(NavigationListenerBrowserTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(NavigationListenerBrowserTest, NoLoadEnd);
  FRIEND_TEST_ALL_PREFIXES(NavigationListenerBrowserTest,
                           NewRendererInitiatedSameDocNavDuringCrossDocNav);
  FRIEND_TEST_ALL_PREFIXES(NavigationListenerBrowserTest,
                           NewBrowserInitiatedSameDocNavDuringCrossDocNav);
  FRIEND_TEST_ALL_PREFIXES(NavigationListenerBrowserTest,
                           NewCrossDocNavDuringCrossDocNav);

  static std::unique_ptr<WebMessage> CreateWebMessage(
      base::Value::Dict message_dict);

  NavigationWebMessageSender(content::Page& page,
                             const std::u16string& js_object_name,
                             WebMessageHostFactory* factory);

  // content::WebContentsObserver implementations
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void PostMessageWithType(std::string_view type);
  void PostMessage(base::Value::Dict message_dict);

  bool ShouldSendMessageForNavigation(
      content::NavigationHandle* navigation_handle);

  WebMessageHost* GetWebMessageHostForTesting() { return host_.get(); }

  std::unique_ptr<EmptyReplyProxy> reply_proxy_;
  std::unique_ptr<WebMessageHost> host_;
  PAGE_USER_DATA_KEY_DECL();
};

}  // namespace js_injection

#endif  // COMPONENTS_JS_INJECTION_BROWSER_NAVIGATION_WEB_MESSAGE_SENDER_H_
