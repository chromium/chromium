// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_navigation_observer.h"

#include "base/bind.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

// Helpers --------------------------------------------------------------------

namespace {

// HTTP 2xx, 401, and 407 all indicate that the target address exists.
bool ResponseCodeIndicatesSuccess(int response_code) {
  return ((response_code / 100) == 2) || (response_code == 401) ||
         (response_code == 407);
}

// Returns true if |final_url| doesn't represent an ISP hijack of
// |original_url|, based on the IntranetRedirectDetector's RedirectOrigin().
bool IsValidNavigation(const GURL& original_url, const GURL& final_url) {
  const GURL& redirect_url(IntranetRedirectDetector::RedirectOrigin());
  return !redirect_url.is_valid() ||
         net::registry_controlled_domains::SameDomainOrHost(
             original_url, final_url,
             net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES) ||
         !net::registry_controlled_domains::SameDomainOrHost(
             final_url, redirect_url,
             net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
}

// Returns true if |origin| is a http URL and |destination| is a https URL and
// the URLs are otherwise identical.
bool OnlyChangeIsFromHTTPToHTTPS(const GURL& origin, const GURL& destination) {
  // Exit early if possible.
  if (!origin.SchemeIs(url::kHttpScheme) ||
      !destination.SchemeIs(url::kHttpsScheme))
    return false;

  GURL::Replacements replace_scheme;
  replace_scheme.SetSchemeStr(url::kHttpsScheme);
  GURL origin_with_https = origin.ReplaceComponents(replace_scheme);
  return origin_with_https == destination;
}

}  // namespace

// ChromeOmniboxNavigationObserver --------------------------------------------

ChromeOmniboxNavigationObserver::ChromeOmniboxNavigationObserver(
    Profile* profile,
    const base::string16& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternate_nav_match)
    : text_(text),
      match_(match),
      alternate_nav_match_(alternate_nav_match),
      template_url_service_(TemplateURLServiceFactory::GetForProfile(profile)),
      shortcuts_backend_(ShortcutsBackendFactory::GetForProfile(profile)),
      load_state_(LOAD_NOT_SEEN),
      fetch_state_(FETCH_NOT_COMPLETE) {
  if (alternate_nav_match_.destination_url.is_valid())
    CreateLoader(alternate_nav_match_.destination_url);

  // We need to start by listening to AllSources, since we don't know which tab
  // the navigation might occur in.
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
                 content::NotificationService::AllSources());
}

ChromeOmniboxNavigationObserver::~ChromeOmniboxNavigationObserver() {}

void ChromeOmniboxNavigationObserver::OnSuccessfulNavigation() {
  if (shortcuts_backend_.get())
    shortcuts_backend_->AddOrUpdateShortcut(text_, match_);
}

void ChromeOmniboxNavigationObserver::On404() {
  TemplateURL* template_url = match_.GetTemplateURL(
      template_url_service_, false /* allow_fallback_to_destination_host */);
  // If the omnibox navigation was to a URL (and hence did not involve a
  // TemplateURL / search at all) or the invoked search engine has been deleted
  // or otherwise modified, doing nothing is the right thing.
  if (template_url == nullptr)
    return;
  // If there's any hint that we should keep this search engine around, don't
  // mess with it.
  if (template_url_service_->ShowInDefaultList(template_url) ||
      !template_url->safe_for_autoreplace())
    return;
  // This custom search engine is safe to delete.
  template_url_service_->Remove(template_url);
}

void ChromeOmniboxNavigationObserver::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> testing_loader_factory) {
  loader_factory_for_testing_ = std::move(testing_loader_factory);
}

void ChromeOmniboxNavigationObserver::CreateAlternateNavInfoBar() {
  AlternateNavInfoBarDelegate::CreateForOmniboxNavigation(
      web_contents(), text_, alternate_nav_match_, match_.destination_url);
}

bool ChromeOmniboxNavigationObserver::HasSeenPendingLoad() const {
  return load_state_ != LOAD_NOT_SEEN;
}

void ChromeOmniboxNavigationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_PENDING, type);

  // It's possible for an attempted omnibox navigation to cause the extensions
  // system to synchronously navigate an extension background page.  Not only is
  // this navigation not the one we want to observe, the associated WebContents
  // is invisible and has no InfoBarService, so trying to show an infobar in it
  // later will crash.  Just ignore this navigation and keep listening.
  content::NavigationController* controller =
      content::Source<content::NavigationController>(source).ptr();
  content::WebContents* web_contents = controller->GetWebContents();
  if (!InfoBarService::FromWebContents(web_contents))
    return;

  // Ignore navigations to the wrong URL.
  // This shouldn't actually happen, but right now it's possible because the
  // prerenderer doesn't properly notify us when it swaps in a prerendered page.
  // Plus, the swap-in can trigger instant to kick off a new background
  // prerender, which we _do_ get notified about.  Once crbug.com/247848 is
  // fixed, this conditional should be able to be replaced with a [D]CHECK;
  // until then we ignore the incorrect navigation (and will be torn down
  // without having received the correct notification).
  if (match_.destination_url !=
      content::Details<content::NavigationEntry>(details)->GetVirtualURL())
    return;

  // If we've already observed one load, this tab is getting reloaded, so this
  // is no longer the navigation we wanted to look at.
  if (load_state_ != LOAD_NOT_SEEN) {
    delete this;
    return;
  }

  // We've seen a pending load; update our state accordingly and begin watching
  // for future state changes.
  registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
                    content::NotificationService::AllSources());
  load_state_ = LOAD_PENDING;
  WebContentsObserver::Observe(web_contents);

  // Start the alternate nav loader if need be.
  if (loader_) {
    network::mojom::URLLoaderFactory* loader_factory = nullptr;
    if (loader_factory_for_testing_) {
      loader_factory = loader_factory_for_testing_.get();
    } else {
      loader_factory = content::BrowserContext::GetDefaultStoragePartition(
                           controller->GetBrowserContext())
                           ->GetURLLoaderFactoryForBrowserProcess()
                           .get();
    }
    loader_->DownloadToString(
        loader_factory,
        base::BindOnce(&ChromeOmniboxNavigationObserver::OnURLLoadComplete,
                       base::Unretained(this)),
        1u /* max_body_size */);
  }
}

void ChromeOmniboxNavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if ((load_state_ != LOAD_COMMITTED) && navigation_handle->IsErrorPage() &&
      navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSameDocument())
    delete this;
}

void ChromeOmniboxNavigationObserver::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  load_state_ = LOAD_COMMITTED;
  if (ResponseCodeIndicatesSuccess(load_details.http_status_code) &&
      IsValidNavigation(match_.destination_url,
                        load_details.entry->GetVirtualURL()))
    OnSuccessfulNavigation();
  if (load_details.http_status_code == 404)
    On404();
  if (!loader_ || (fetch_state_ != FETCH_NOT_COMPLETE))
    OnAllLoadingFinished();  // deletes |this|!
}

void ChromeOmniboxNavigationObserver::WebContentsDestroyed() {
  delete this;
}

void ChromeOmniboxNavigationObserver::OnURLRedirect(
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* to_be_removed_headers) {
  bool valid_redirect = IsValidNavigation(alternate_nav_match_.destination_url,
                                          redirect_info.new_url);
  // If this is a valid redirect (not hijacked), and the redirect is from
  // http->https (no other changes), then follow it instead of assuming the
  // destination is valid.  This fixes several cases when the infobar appears
  // when it shouldn't, e.g.,
  // * Users who have the HTTPS Everywhere extension enabled with the setting
  //   "Block all unencrypted requests".  (All requests get redirected to
  //   https://.)
  // * Users who enter "google" in the omnibox (or any other preloaded HSTS
  //   domain name).
  //   For these Chrome generates an internal redirect to the HTTPS version of
  //   the domain, which is not always valid.  E.g., https://google/ is not
  //   valid.
  // * Users on networks that return 3xx redirects to the https version for all
  //   requests for local sites.
  if (valid_redirect &&
      OnlyChangeIsFromHTTPToHTTPS(alternate_nav_match_.destination_url,
                                  redirect_info.new_url)) {
    return;
  }

  // Otherwise report results based on whether the redirect itself is valid.
  // OnDoneWithURL() will also stop the redirect from being followed since it
  // destroys |*loader_|.
  //
  // We stop-on-redirect here for a couple of reasons:
  // * Sites with lots of redirects, especially through slow machines, take time
  //   to follow the redirects.  This delays the appearance of the infobar,
  //   sometimes by several seconds, which feels really broken.
  // * Some servers behind redirects respond to HEAD with an error and GET with
  //   a valid response, in violation of the HTTP spec.  Stop-on-redirects
  //   reduces the number of cases where this error makes us believe there was
  //   no server.
  OnDoneWithURL(valid_redirect);
}

void ChromeOmniboxNavigationObserver::OnURLLoadComplete(
    std::unique_ptr<std::string> body) {
  int response_code = -1;
  if (loader_->ResponseInfo() && loader_->ResponseInfo()->headers)
    response_code = loader_->ResponseInfo()->headers->response_code();
  // We may see ERR_INSUFFICIENT_RESOURCES here even if everything is workable
  // if the server includes a body in response to a HEAD, as a size limit was
  // set while fetching.
  bool fetch_likely_ok = loader_->NetError() == net::OK ||
                         loader_->NetError() == net::ERR_INSUFFICIENT_RESOURCES;
  OnDoneWithURL(fetch_likely_ok && ResponseCodeIndicatesSuccess(response_code));
}

void ChromeOmniboxNavigationObserver::OnDoneWithURL(bool success) {
  loader_ = nullptr;
  fetch_state_ = success ? FETCH_SUCCEEDED : FETCH_FAILED;
  if (load_state_ == LOAD_COMMITTED)
    OnAllLoadingFinished();  // deletes |this|!
}

void ChromeOmniboxNavigationObserver::OnAllLoadingFinished() {
  if (fetch_state_ == FETCH_SUCCEEDED)
    CreateAlternateNavInfoBar();
  delete this;
}

void ChromeOmniboxNavigationObserver::CreateLoader(
    const GURL& destination_url) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("omnibox_navigation_observer", R"(
        semantics {
          sender: "Omnibox"
          description:
            "Certain omnibox inputs, e.g. single words, may either be search "
            "queries or attempts to navigate to intranet hostnames. When "
            "such a hostname is not in the user's history, a background "
            "request is made to see if it is navigable.  If so, the browser "
            "will display a prompt on the search results page asking if the "
            "user wished to navigate instead of searching."
          trigger:
            "User attempts to search for a string that is plausibly a "
            "navigable hostname but is not in the local history."
          data:
            "None. However, the hostname itself is a string the user "
            "searched for, and thus can expose data about the user's "
            "searches."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification:
            "By disabling DefaultSearchProviderEnabled, one can disable "
            "default search, and once users can't search, they can't hit "
            "this. More fine-grained policies are requested to be "
            "implemented (crbug.com/81226)."
        })");
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = destination_url;
  request->method = "HEAD";
  request->load_flags = net::LOAD_DO_NOT_SAVE_COOKIES;
  loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  loader_->SetAllowHttpErrorResults(true);
  loader_->SetOnRedirectCallback(base::BindRepeating(
      &ChromeOmniboxNavigationObserver::OnURLRedirect, base::Unretained(this)));
}
