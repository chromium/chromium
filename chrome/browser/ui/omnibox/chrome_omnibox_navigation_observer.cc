// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_navigation_observer.h"

#include "base/functional/bind.h"
#include "base/trace_event/typed_macros.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
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

class ChromeOmniboxNavigationObserver;

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

// Choose the appropriate URLLoaderFactory: either an explicitly specified or a
// default for the given profile.
network::mojom::URLLoaderFactory* GetURLLoaderFactory(
    network::mojom::URLLoaderFactory* loader_factory,
    Profile* profile) {
  if (loader_factory)
    return loader_factory;
  return profile->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess()
      .get();
}

// Helper to keep ChromeOmniboxNavigationObserver alive while the initiated
// navigation is pending.
struct NavigationUserData
    : public content::NavigationHandleUserData<NavigationUserData> {
  NavigationUserData(content::NavigationHandle& navigation,
                     scoped_refptr<ChromeOmniboxNavigationObserver> observer)
      : observer(std::move(observer)) {}
  ~NavigationUserData() override = default;

  scoped_refptr<ChromeOmniboxNavigationObserver> observer;

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(NavigationUserData);

}  // namespace

class ChromeOmniboxNavigationObserver::AlternativeNavigationURLLoader {
 public:
  AlternativeNavigationURLLoader(
      const GURL& destination_url,
      scoped_refptr<ChromeOmniboxNavigationObserver> navigation_observer,
      base::OnceCallback<void(bool)> on_complete,
      network::mojom::URLLoaderFactory* loader_factory)
      : destination_url_(destination_url),
        navigation_observer_(std::move(navigation_observer)),
        on_complete_(std::move(on_complete)) {
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
    loader_ = network::SimpleURLLoader::Create(std::move(request),
                                               traffic_annotation);
    loader_->SetAllowHttpErrorResults(true);
    loader_->SetOnRedirectCallback(base::BindRepeating(
        &AlternativeNavigationURLLoader::OnRedirect, base::Unretained(this)));

    loader_->DownloadToString(
        loader_factory,
        base::BindOnce(&AlternativeNavigationURLLoader::OnURLLoadComplete,
                       base::Unretained(this)),
        1u /* max_body_size */);
  }

  void OnRedirect(const GURL& url_before_redirect,
                  const net::RedirectInfo& redirect_info,
                  const network::mojom::URLResponseHead& response_head,
                  std::vector<std::string>* to_be_removed_headers) {
    bool valid_redirect =
        IsValidNavigation(destination_url_, redirect_info.new_url);
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
    // * Users on networks that return 3xx redirects to the https version for
    // all
    //   requests for local sites.
    if (valid_redirect &&
        OnlyChangeIsFromHTTPToHTTPS(destination_url_, redirect_info.new_url)) {
      return;
    }

    // Otherwise report results based on whether the redirect itself is valid.
    // OnDoneWithURL() will also stop the redirect from being followed since it
    // destroys |*loader_|.
    //
    // We stop-on-redirect here for a couple of reasons:
    // * Sites with lots of redirects, especially through slow machines, take
    // time
    //   to follow the redirects.  This delays the appearance of the infobar,
    //   sometimes by several seconds, which feels really broken.
    // * Some servers behind redirects respond to HEAD with an error and GET
    // with
    //   a valid response, in violation of the HTTP spec.  Stop-on-redirects
    //   reduces the number of cases where this error makes us believe there was
    //   no server.
    OnDoneWithURL(valid_redirect);

    // |this| may be deleted at this point.
  }

  void OnURLLoadComplete(std::unique_ptr<std::string> body) {
    int response_code = -1;
    if (loader_->ResponseInfo() && loader_->ResponseInfo()->headers)
      response_code = loader_->ResponseInfo()->headers->response_code();
    // We may see ERR_INSUFFICIENT_RESOURCES here even if everything is workable
    // if the server includes a body in response to a HEAD, as a size limit was
    // set while fetching.
    bool fetch_likely_ok =
        loader_->NetError() == net::OK ||
        loader_->NetError() == net::ERR_INSUFFICIENT_RESOURCES;
    OnDoneWithURL(fetch_likely_ok &&
                  ResponseCodeIndicatesSuccess(response_code));

    // |this| may be deleted at this point.
  }

  void OnDoneWithURL(bool success) { std::move(on_complete_).Run(success); }

 private:
  const GURL destination_url_;

  // URLLoader should keep NavigationObserver alive until it's done.
  scoped_refptr<ChromeOmniboxNavigationObserver> navigation_observer_;
  // Callback to invoke when we're done.
  base::OnceCallback<void(bool)> on_complete_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
};

ChromeOmniboxNavigationObserver::ChromeOmniboxNavigationObserver(
    content::NavigationHandle& navigation,
    Profile* profile,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternative_nav_match,
    network::mojom::URLLoaderFactory* loader_factory,
    ShowInfobarCallback show_infobar)
    : content::WebContentsObserver(navigation.GetWebContents()),
      text_(text),
      match_(match),
      alternative_nav_match_(alternative_nav_match),
      navigation_id_(navigation.GetNavigationId()),
      profile_(profile),
      show_infobar_(std::move(show_infobar)) {
  NavigationUserData::CreateForNavigationHandle(navigation, this);
  if (alternative_nav_match_.destination_url.is_valid()) {
    loader_ = std::make_unique<AlternativeNavigationURLLoader>(
        alternative_nav_match.destination_url, this,
        base::BindOnce(
            &ChromeOmniboxNavigationObserver::OnAlternativeLoaderDone, this),
        GetURLLoaderFactory(loader_factory, profile));
  }
}

ChromeOmniboxNavigationObserver::~ChromeOmniboxNavigationObserver() {
  if (!web_contents())
    return;
  if (fetch_state_ == AlternativeFetchState::kFetchSucceeded) {
    std::move(show_infobar_).Run(this);
  }
}

void ChromeOmniboxNavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->GetNavigationId() != navigation_id_)
    return;

  // This is the navigation we've started ourselves in the primary main frame
  // of the WebContents.
  DCHECK(navigation_handle->IsInPrimaryMainFrame());

  // Ignore navigations which didn't commit, or committed a page which bypassed
  // the network (e.g. about:blank).
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->GetResponseHeaders()) {
    return;
  }

  // We can get only get virtual URL from the WebContents for now. This is
  // correct here (as we are processing a committed primary main frame
  // navigation), but we should consider adding a way to get virtual URL
  // directly from NavigationHandle.
  if (ResponseCodeIndicatesSuccess(
          navigation_handle->GetResponseHeaders()->response_code()) &&
      IsValidNavigation(match_.destination_url,
                        navigation_handle->GetWebContents()->GetVisibleURL())) {
    ChromeOmniboxClient::OnSuccessfulNavigation(profile_, text_, match_);
  }

  if (navigation_handle->GetResponseHeaders()->response_code() == 404) {
    On404();
  }
}

void ChromeOmniboxNavigationObserver::On404() {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  TemplateURL* template_url = match_.GetTemplateURL(
      template_url_service, false /* allow_fallback_to_destination_host */);
  // If the omnibox navigation was to a URL (and hence did not involve a
  // TemplateURL / search at all) or the invoked search engine has been
  // deleted or otherwise modified, doing nothing is the right thing.
  if (template_url == nullptr)
    return;
  // If there's any hint that we should keep this search engine around, don't
  // mess with it.
  if (template_url_service->ShowInDefaultList(template_url) ||
      !template_url->safe_for_autoreplace() ||
      template_url->starter_pack_id() != 0) {
    return;
  }
  // This custom search engine is safe to delete.
  template_url_service->Remove(template_url);
}

void ChromeOmniboxNavigationObserver::OnAlternativeLoaderDone(bool success) {
  TRACE_EVENT("omnibox",
              "ChromeOmniboxNavigationObserver::OnAlternativeLoaderDone",
              "success", success);
  if (success) {
    fetch_state_ = AlternativeFetchState::kFetchSucceeded;
  } else {
    fetch_state_ = AlternativeFetchState::kFetchFailed;
  }
  loader_.reset();
  // |this| might be deleted here.
}

void ChromeOmniboxNavigationObserver::ShowAlternativeNavInfoBar() {
  AlternateNavInfoBarDelegate::CreateForOmniboxNavigation(
      web_contents(), text_, alternative_nav_match_, match_.destination_url);
}

// static
void ChromeOmniboxNavigationObserver::Create(
    content::NavigationHandle* navigation,
    Profile* profile,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternative_nav_match) {
  TRACE_EVENT("omnibox", "ChromeOmniboxNavigationObserver::Create",
              "navigation", navigation, "match", match, "alternative_nav_match",
              alternative_nav_match);

  if (!navigation)
    return;

  // The observer will be kept alive until both navigation and the loading
  // fetcher finish.
  new ChromeOmniboxNavigationObserver(
      *navigation, profile, text, match, alternative_nav_match, nullptr,
      base::BindOnce([](ChromeOmniboxNavigationObserver* observer) {
        observer->ShowAlternativeNavInfoBar();
      }));
}

void ChromeOmniboxNavigationObserver::CreateForTesting(
    content::NavigationHandle* navigation,
    Profile* profile,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternative_nav_match,
    network::mojom::URLLoaderFactory* loader_factory,
    ShowInfobarCallback show_infobar) {
  if (!navigation)
    return;

  // The observer will be kept alive until both navigation and the loading
  // fetcher finish.
  new ChromeOmniboxNavigationObserver(*navigation, profile, text, match,
                                      alternative_nav_match, loader_factory,
                                      std::move(show_infobar));
}
