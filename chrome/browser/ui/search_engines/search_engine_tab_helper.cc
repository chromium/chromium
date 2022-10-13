// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_fetcher_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search_engines/edit_search_engine_controller.h"
#include "chrome/common/url_constants.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_fetcher.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

namespace {

// Returns true if the entry's transition type is FORM_SUBMIT.
bool IsFormSubmit(NavigationEntry* entry) {
  return ui::PageTransitionCoreTypeIs(entry->GetTransitionType(),
                                      ui::PAGE_TRANSITION_FORM_SUBMIT);
}

}  // namespace

// static
void SearchEngineTabHelper::BindOpenSearchDescriptionDocumentHandler(
    content::RenderFrameHost* rfh,
    mojo::PendingReceiver<chrome::mojom::OpenSearchDescriptionDocumentHandler>
        receiver) {
  // Bind only for outermost main frames.
  if (rfh->GetParentOrOuterDocument())
    return;

  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* tab_helper = SearchEngineTabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return;
  tab_helper->osdd_handler_receivers_.Add(tab_helper, std::move(receiver));
}

SearchEngineTabHelper::~SearchEngineTabHelper() = default;

void SearchEngineTabHelper::DidFinishNavigation(
    content::NavigationHandle* handle) {
  GenerateKeywordIfNecessary(handle);
}

void SearchEngineTabHelper::WebContentsDestroyed() {
  favicon_driver_observation_.Reset();
}

std::u16string SearchEngineTabHelper::GenerateKeywordFromNavigationEntry(
    NavigationEntry* entry) {
  // Don't autogenerate keywords for pages that are the result of form
  // submissions.
  if (IsFormSubmit(entry))
    return std::u16string();

  // We want to use the user typed URL if available since that represents what
  // the user typed to get here, and fall back on the regular URL if not.
  GURL url = entry->GetUserTypedURL();
  if (!url.is_valid()) {
    url = entry->GetURL();
    if (!url.is_valid())
      return std::u16string();
  }

  // Don't autogenerate keywords for referrers that
  // a) are anything other than HTTP/HTTPS or
  // b) have a path.
  //
  // If we relax the path constraint, we need to be sure to sanitize the path
  // elements and update AutocompletePopup to look for keywords using the path.
  // See http://b/issue?id=863583.
  if (!(url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme)) ||
      (url.path().length() > 1)) {
    return std::u16string();
  }

  return TemplateURL::GenerateKeyword(url);
}

SearchEngineTabHelper::SearchEngineTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SearchEngineTabHelper>(*web_contents) {
  DCHECK(web_contents);

  favicon::CreateContentFaviconDriverForWebContents(web_contents);
  favicon_driver_observation_.Observe(
      favicon::ContentFaviconDriver::FromWebContents(web_contents));
}

void SearchEngineTabHelper::PageHasOpenSearchDescriptionDocument(
    const GURL& page_url,
    const GURL& osdd_url) {
  // Checks to see if we should generate a keyword based on the OSDD, and if
  // necessary uses TemplateURLFetcher to download the OSDD and create a
  // keyword.

  // Make sure that the page is the current page and other basic checks.
  // When |page_url| has file: scheme, this method doesn't work because of
  // http://b/issue?id=863583. For that reason, this doesn't check and allow
  // urls referring to osdd urls with same schemes.
  if (!osdd_url.is_valid() || !osdd_url.SchemeIsHTTPOrHTTPS())
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (page_url != web_contents()->GetLastCommittedURL() ||
      !TemplateURLFetcherFactory::GetForProfile(profile) ||
      profile->IsOffTheRecord())
    return;

  // If the current page is a form submit, find the last page that was not a
  // form submit and use its url to generate the keyword from.
  NavigationController& controller = web_contents()->GetController();
  NavigationEntry* entry = controller.GetLastCommittedEntry();
  for (int index = controller.GetLastCommittedEntryIndex();
       (index > 0) && IsFormSubmit(entry);
       entry = controller.GetEntryAtIndex(index))
    --index;
  if (!entry || IsFormSubmit(entry))
    return;

  // Autogenerate a keyword for the autodetected case; in the other cases we'll
  // generate a keyword later after fetching the OSDD.
  std::u16string keyword = GenerateKeywordFromNavigationEntry(entry);
  if (keyword.empty())
    return;

  auto* frame = web_contents()->GetPrimaryMainFrame();
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory;
  frame->CreateNetworkServiceDefaultFactory(
      url_loader_factory.BindNewPipeAndPassReceiver());

  // Download the OpenSearch description document. If this is successful, a
  // new keyword will be created when done.
  TemplateURLFetcherFactory::GetForProfile(profile)->ScheduleDownload(
      keyword, osdd_url, entry->GetFavicon().url,
      frame->GetLastCommittedOrigin(), url_loader_factory.get(),
      frame->GetRoutingID(),
      content::GlobalRequestID::MakeBrowserInitiated().request_id);
}

void SearchEngineTabHelper::OnFaviconUpdated(
    favicon::FaviconDriver* driver,
    NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (url_service && url_service->loaded())
    url_service->UpdateProviderFavicons(driver->GetActiveURL(), icon_url);
}

void SearchEngineTabHelper::GenerateKeywordIfNecessary(
    content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame() ||
      !handle->GetSearchableFormURL().is_valid())
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (profile->IsOffTheRecord())
    return;

  NavigationController& controller = web_contents()->GetController();
  int last_index = controller.GetLastCommittedEntryIndex();
  // When there was no previous page, the last index will be 0. This is
  // normally due to a form submit that opened in a new tab.
  // TODO(brettw) bug 916126: we should support keywords when form submits
  //              happen in new tabs.
  if (last_index <= 0)
    return;

  std::u16string keyword(GenerateKeywordFromNavigationEntry(
      controller.GetEntryAtIndex(last_index - 1)));
  if (keyword.empty())
    return;

  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (!url_service)
    return;

  if (!url_service->loaded()) {
    url_service->Load();
    return;
  }

  GURL url = handle->GetSearchableFormURL();
  if (!url_service->CanAddAutogeneratedKeyword(keyword, url))
    return;

  TemplateURLData data;
  data.SetShortName(keyword);
  data.SetKeyword(keyword);
  data.SetURL(url.spec());
  DCHECK(controller.GetLastCommittedEntry());
  const GURL& current_favicon =
      controller.GetLastCommittedEntry()->GetFavicon().url;
  // If the favicon url isn't valid, it means there really isn't a favicon, or
  // the favicon url wasn't obtained before the load started. This assumes the
  // latter.
  // TODO(sky): Need a way to set the favicon that doesn't involve generating
  // its url.
  if (current_favicon.is_valid()) {
    data.favicon_url = current_favicon;
  } else if (handle->GetReferrer().url.is_valid()) {
    data.favicon_url =
        TemplateURL::GenerateFaviconURL(handle->GetReferrer().url);
  }
  data.safe_for_autoreplace = true;
  data.input_encodings.push_back(handle->GetSearchableFormEncoding());

  // This Add() call may displace the previously auto-generated TemplateURL.
  // But it will never displace the Default Search Engine, nor will it displace
  // any OpenSearch document derived engines, which outrank this one.
  url_service->Add(std::make_unique<TemplateURL>(data));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SearchEngineTabHelper);
