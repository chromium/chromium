// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_tab_helper.h"

#include "base/feature_list.h"
#include "base/task/post_task.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/common/chrome_features.h"
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
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument())
    return;

  if (!delegate_)
    return;

  delegate_.reset();
  url_for_delegate_ = GURL();
}

void LoginTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(
      base::FeatureList::IsEnabled(features::kHTTPAuthCommittedInterstitials));

  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
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

  // TODO(https://crbug.com/969097): handle auth challenges that lead to
  // downloads (i.e. don't commit).

  // Show a login prompt with the navigation's AuthChallengeInfo on FTP
  // navigations and on HTTP 401/407 responses.
  if (!navigation_handle->GetURL().SchemeIs(url::kFtpScheme)) {
    int response_code =
        navigation_handle->GetResponseHeaders()->response_code();
    if (response_code !=
            net::HttpStatusCode::HTTP_PROXY_AUTHENTICATION_REQUIRED &&
        response_code != net::HttpStatusCode::HTTP_UNAUTHORIZED) {
      return;
    }
  }

  challenge_ = navigation_handle->GetAuthChallengeInfo().value();
  network_isolation_key_ = navigation_handle->GetNetworkIsolationKey();

  url_for_delegate_ = navigation_handle->GetURL();
  delegate_ = CreateLoginPrompt(
      navigation_handle->GetAuthChallengeInfo().value(),
      navigation_handle->GetWebContents(),
      navigation_handle->GetGlobalRequestID(), true,
      navigation_handle->GetURL(),
      // TODO(https://crbug.com/968881): response headers can be null because
      // they are only used for passing the request to extensions, and that
      // doesn't happen in POST_COMMIT mode. This API needs to be cleaned up.
      nullptr, LoginHandler::POST_COMMIT,
      base::BindOnce(
          &LoginTabHelper::HandleCredentials,
          // Since the LoginTabHelper owns the |delegate_| that calls this
          // callback, it's safe to use base::Unretained here; the |delegate_|
          // cannot outlive its owning LoginTabHelper.
          base::Unretained(this)));

  // If the challenge comes from a proxy, the URL should be hidden in the
  // omnibox to avoid origin confusion. Call DidChangeVisibleSecurityState() to
  // trigger the omnibox to update, picking up the result of ShouldDisplayURL().
  if (challenge_.is_proxy) {
    navigation_handle->GetWebContents()->DidChangeVisibleSecurityState();
  }
}

bool LoginTabHelper::ShouldDisplayURL() const {
  return !delegate_ || !challenge_.is_proxy;
}

bool LoginTabHelper::IsShowingPrompt() const {
  return !!delegate_;
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
    return content::NavigationThrottle::PROCEED;
  }

  // Otherwise, rewrite the response to a blank page. DidFinishNavigation will
  // show a login prompt on top of this blank page.
  return {content::NavigationThrottle::CANCEL,
          net::ERR_INVALID_AUTH_CREDENTIALS, "<html></html>"};
}

LoginTabHelper::LoginTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

void LoginTabHelper::HandleCredentials(
    const base::Optional<net::AuthCredentials>& credentials) {
  delegate_.reset();
  url_for_delegate_ = GURL();

  if (credentials.has_value()) {
    // Pass a weak pointer for the callback, as the WebContents (and thus this
    // LoginTabHelper) could be destroyed while the network service is
    // processing the new cache entry.
    content::BrowserContext::GetDefaultStoragePartition(
        web_contents()->GetBrowserContext())
        ->GetNetworkContext()
        ->AddAuthCacheEntry(challenge_, network_isolation_key_,
                            credentials.value(),
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
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&LoginTabHelper::Reload,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void LoginTabHelper::Reload() {
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, true);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LoginTabHelper)
