// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_tab_helper.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/network_context.mojom.h"

LoginTabHelper::~LoginTabHelper() {}

void LoginTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // When navigating away, the LoginHandler for the previous navigation (if any)
  // should get cleared.

  // Do not clear the login prompt for subframe or same-document navigations;
  // these could happen in the case of 401/407 error pages that have fancy
  // response bodies that have subframes or can trigger same-document
  // navigations.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument())
    return;

  login_handler_.reset();
}

void LoginTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Check if this navigation was already handled by an extension cancelling the
  // auth request. If so, do not show a prompt for it.
  int64_t navigation_id_for_extension_cancelled_navigation =
      navigation_handle_id_for_extension_cancelled_navigation_;
  // Once a navigation has finished, any pending navigation handled by an
  // extension is no longer pending, so clear this field.
  navigation_handle_id_for_extension_cancelled_navigation_ = 0;
  if (navigation_handle->GetNavigationId() ==
      navigation_id_for_extension_cancelled_navigation) {
    return;
  }

  // LoginTabHelper stores the navigation entry ID and navigation handle ID
  // corresponding to the refresh that occurs when a user cancels a prompt. (The
  // refresh is to retrieve the error page body from the server so that it can
  // be displayed to the user.) Once a navigation has finished, such a refresh
  // is no longer pending, so clear these fields.
  navigation_entry_id_with_cancelled_prompt_ = 0;
  int64_t navigation_handle_id_with_cancelled_prompt =
      navigation_handle_id_with_cancelled_prompt_;
  navigation_handle_id_with_cancelled_prompt_ = 0;
  // If the finishing navigation corresponding to a refresh for a cancelled
  // prompt, then return here to avoid showing a prompt again.
  if (navigation_handle->GetNavigationId() ==
      navigation_handle_id_with_cancelled_prompt) {
    return;
  }

  if (!navigation_handle->GetAuthChallengeInfo()) {
    return;
  }

  // Show a login prompt with the navigation's AuthChallengeInfo on HTTP 401/407
  // responses.
  int response_code = navigation_handle->GetResponseHeaders()->response_code();
  if (response_code !=
          net::HttpStatusCode::HTTP_PROXY_AUTHENTICATION_REQUIRED &&
      response_code != net::HttpStatusCode::HTTP_UNAUTHORIZED) {
    return;
  }

  challenge_ = navigation_handle->GetAuthChallengeInfo().value();
  network_anonymization_key_ =
      navigation_handle->GetIsolationInfo().network_anonymization_key();

  login_handler_ = CreateLoginHandler(
      navigation_handle->GetAuthChallengeInfo().value(),
      navigation_handle->GetWebContents(),
      base::BindOnce(
          &LoginTabHelper::HandleCredentials,
          // Since the LoginTabHelper owns the |login_handler_| that calls this
          // callback, it's safe to use base::Unretained here; the
          // |login_handler_| cannot outlive its owning LoginTabHelper.
          base::Unretained(this)));
  login_handler_->ShowLoginPrompt(navigation_handle->GetURL());

  // If the challenge comes from a proxy, the URL should be hidden in the
  // omnibox to avoid origin confusion. Call DidChangeVisibleSecurityState() to
  // trigger the omnibox to update, picking up the result of ShouldDisplayURL().
  if (challenge_.is_proxy) {
    navigation_handle->GetWebContents()->DidChangeVisibleSecurityState();
  }
}

bool LoginTabHelper::ShouldDisplayURL() const {
  return !login_handler_ || !challenge_.is_proxy;
}

bool LoginTabHelper::IsShowingPrompt() const {
  return !!login_handler_;
}

content::NavigationThrottle::ThrottleCheckResult
LoginTabHelper::WillProcessMainFrameUnauthorizedResponse(
    content::NavigationHandle* navigation_handle) {
  // If the user has just cancelled the auth prompt for this navigation, then
  // the page is being refreshed to retrieve the 401 body from the server, so
  // allow the refresh to proceed. The entry to compare against is the pending
  // entry, because while refreshing after cancelling the prompt, the page that
  // showed the prompt will be the pending entry until the refresh
  // commits. Comparing against GetVisibleEntry() would also work, but it's less
  // specific and not guaranteed to exist in all cases (e.g., in the case of
  // navigating a window just opened via window.open()).
  if (web_contents()->GetController().GetPendingEntry() &&
      web_contents()->GetController().GetPendingEntry()->GetUniqueID() ==
          navigation_entry_id_with_cancelled_prompt_) {
    // Note the navigation handle ID so that when this refresh navigation
    // finishes, DidFinishNavigation declines to show another login prompt. We
    // need the navigation handle ID (rather than the navigation entry ID) here
    // because the navigation entry ID will change once the refresh finishes.
    navigation_handle_id_with_cancelled_prompt_ =
        navigation_handle->GetNavigationId();

    int response_code =
        navigation_handle->GetResponseHeaders()->response_code();
    // For HTTPS navigations with 407 responses, we want to show an empty
    // page. We need to cancel the navigation and commit an empty error
    // page directly here, because otherwise the HttpErrorNavigationThrottle
    // will see that the response body is empty (because the network stack
    // refuses to read the response body) and will try to commit a generic
    // non-empty error page instead.
    if (navigation_handle->GetURL().SchemeIs(url::kHttpsScheme) &&
        response_code ==
            net::HttpStatusCode::HTTP_PROXY_AUTHENTICATION_REQUIRED) {
      return {content::NavigationThrottle::CANCEL,
              net::ERR_INVALID_AUTH_CREDENTIALS, "<html></html>"};
    }
    return content::NavigationThrottle::PROCEED;
  }

  // Do not cancel the navigation if there is no auth challenge. We only want to
  // cancel the navigation below to show a blank page if there is an auth
  // challenge for which to show a login prompt.
  if (!navigation_handle->GetAuthChallengeInfo()) {
    return content::NavigationThrottle::PROCEED;
  }

  // Check if this response was for an auth request that an extension handled by
  // cancelling auth. If so, remember the navigation handle ID so as to be able
  // to suppress a prompt for this navigation when it finishes in
  // DidFinishNavigation().
  if (navigation_handle->GetGlobalRequestID().request_id ==
      request_id_for_extension_cancelled_navigation_.request_id) {
    // Navigation requests are always initiated in the browser process. Due to a
    // bug (https://crbug.com/1078216), different |child_id|s are used in
    // different places to represent the browser process. Therefore, we don't
    // compare the two GlobalRequestIDs directly here but rather check that they
    // each have the expected |child_id| value signifying the browser process
    // initiated the request.
    CHECK_EQ(request_id_for_extension_cancelled_navigation_.child_id, 0);
    CHECK_EQ(navigation_handle->GetGlobalRequestID().child_id, -1);
    navigation_handle_id_for_extension_cancelled_navigation_ =
        navigation_handle->GetNavigationId();
    request_id_for_extension_cancelled_navigation_ = {0, -1};
    return content::NavigationThrottle::PROCEED;
  }

  // Otherwise, rewrite the response to a blank page. DidFinishNavigation will
  // show a login prompt on top of this blank page.
  return {content::NavigationThrottle::CANCEL,
          net::ERR_INVALID_AUTH_CREDENTIALS, "<html></html>"};
}

LoginTabHelper::LoginTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<LoginTabHelper>(*web_contents) {}

std::unique_ptr<LoginHandler> LoginTabHelper::CreateLoginHandler(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    LoginAuthRequiredCallback auth_required_callback) {
  return LoginHandler::Create(auth_info, web_contents,
                              std::move(auth_required_callback));
}

void LoginTabHelper::HandleCredentials(
    const std::optional<net::AuthCredentials>& credentials) {
  login_handler_.reset();

  if (credentials.has_value()) {
    content::StoragePartition* storage_partition =
        web_contents()->GetBrowserContext()->GetStoragePartition(
            web_contents()->GetSiteInstance());
    // Pass a weak pointer for the callback, as the WebContents (and thus this
    // LoginTabHelper) could be destroyed while the network service is
    // processing the new cache entry.
    storage_partition->GetNetworkContext()->AddAuthCacheEntry(
        challenge_, network_anonymization_key_, credentials.value(),
        base::BindOnce(&LoginTabHelper::Reload,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Once credentials have been provided, in the case of proxy auth where the
  // URL is hidden when the prompt is showing, trigger
  // DidChangeVisibleSecurityState() to re-show the URL now that the prompt is
  // gone.
  if (challenge_.is_proxy) {
    web_contents()->DidChangeVisibleSecurityState();
  }

  // If the prompt has been cancelled, reload to retrieve the error page body
  // from the server.
  if (!credentials.has_value()) {
    navigation_entry_id_with_cancelled_prompt_ =
        web_contents()->GetController().GetVisibleEntry()->GetUniqueID();
    // Post a task to reload instead of reloading directly. This accounts for
    // the case when the prompt has been cancelled due to the tab
    // closing. Reloading synchronously while a tab is closing causes a DCHECK
    // failure.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&LoginTabHelper::Reload,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void LoginTabHelper::RegisterExtensionCancelledNavigation(
    const content::GlobalRequestID& request_id) {
  request_id_for_extension_cancelled_navigation_ = request_id;
}

void LoginTabHelper::Reload() {
  web_contents()->GetController().Reload(content::ReloadType::NORMAL,
                                         false /* check_for_repost */);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LoginTabHelper);
