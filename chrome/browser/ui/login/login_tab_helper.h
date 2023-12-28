// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOGIN_LOGIN_TAB_HELPER_H_
#define CHROME_BROWSER_UI_LOGIN_LOGIN_TAB_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/auth.h"
#include "net/base/network_anonymization_key.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

class LoginHandler;

// LoginTabHelper is responsible for observing navigations that need to trigger
// authentication prompts, showing the login prompt, and handling user-entered
// credentials from the prompt.
class LoginTabHelper : public content::WebContentsObserver,
                       public content::WebContentsUserData<LoginTabHelper> {
 public:
  LoginTabHelper(const LoginTabHelper&) = delete;
  LoginTabHelper& operator=(const LoginTabHelper&) = delete;

  ~LoginTabHelper() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Returns false if the omnibox should hide the URL due to a proxy auth
  // prompt, and true otherwise. The URL should not be shown during a proxy auth
  // prompt to avoid origin confusion.
  bool ShouldDisplayURL() const;

  // Returns true if an auth prompt is currently visible.
  bool IsShowingPrompt() const;

  // Called when a response is received for a main-frame navigation with an auth
  // challenge. Rewrites the response to a blank page, on top of which
  // DidFinishNavigation() shows a login prompt.
  //
  // This method also tracks state to handle prompt cancellations. When a login
  // prompt is cancelled, the page is refreshed (triggered in HandleCredentials)
  // to retrieve and display the error page content from the server. This method
  // declines to rewrite responses to such refreshes (since the whole point of
  // the refresh is to show the error page content to the user), and tracks the
  // NavigationHandle ID so that DidFinishNavigation knows not to show another
  // login prompt when the refresh finishes.
  content::NavigationThrottle::ThrottleCheckResult
  WillProcessMainFrameUnauthorizedResponse(
      content::NavigationHandle* navigation_handle);

  void RegisterExtensionCancelledNavigation(
      const content::GlobalRequestID& request_id);

 protected:
  explicit LoginTabHelper(content::WebContents* web_contents);
  virtual std::unique_ptr<LoginHandler> CreateLoginHandler(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      LoginAuthRequiredCallback auth_required_callback);

 private:
  friend class content::WebContentsUserData<LoginTabHelper>;

  void HandleCredentials(
      const std::optional<net::AuthCredentials>& credentials);

  // When the user enters credentials into the login prompt, they are populated
  // in the auth cache and then page is reloaded to re-send the request with the
  // cached credentials. This method is passed as the callback to the call that
  // places the credentials into the cache.
  void Reload();

  std::unique_ptr<LoginHandler> login_handler_;

  net::AuthChallengeInfo challenge_;
  net::NetworkAnonymizationKey network_anonymization_key_;

  // Stores the navigation entry ID for a pending refresh due to a user
  // cancelling a login prompt. This is set to the visible navigation entry ID
  // when HandleCredentials() sees a prompt cancellation, and reset to 0 when a
  // navigation finishes. This field is used by
  // WillProcessMainFrameUnauthorizedResponse(), which does not rewrite the
  // response to a blank page when this ID is the currently visible navigation
  // entry ID. This is to avoid rewriting the refresh which is supposed to
  // display error page content from the server.
  int navigation_entry_id_with_cancelled_prompt_ = 0;

  // Stores the navigation handle ID for a navigation corresponding to a refresh
  // due to a user cancelling the login prompt. This is set by
  // WillProcessMainFrameUnauthorizedResponse(), and read by DidFinishNavigation
  // to avoid re-showing a login prompt after the user cancels a prompt. It is
  // reset to 0 when a navigation finishes since there is no longer a pending
  // refresh for a prompt cancellation at that point.
  //
  // Both the *handle* ID and *entry* ID are needed because the entry ID does
  // not stay consistent across the refresh.
  int64_t navigation_handle_id_with_cancelled_prompt_ = 0;

  // When an extension cancels an auth request for a navigation, these members
  // remember the navigation so that the LoginTabHelper can avoid showing an
  // auth prompt.
  content::GlobalRequestID request_id_for_extension_cancelled_navigation_;
  int64_t navigation_handle_id_for_extension_cancelled_navigation_ = 0;

  base::WeakPtrFactory<LoginTabHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_LOGIN_LOGIN_TAB_HELPER_H_
