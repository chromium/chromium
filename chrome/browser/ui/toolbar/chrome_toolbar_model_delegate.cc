// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/chrome_toolbar_model_delegate.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"

#if !defined(OS_ANDROID)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_utils.h"
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

ChromeToolbarModelDelegate::ChromeToolbarModelDelegate() {}

ChromeToolbarModelDelegate::~ChromeToolbarModelDelegate() {}

content::NavigationEntry* ChromeToolbarModelDelegate::GetNavigationEntry()
    const {
  content::NavigationController* controller = GetNavigationController();
  return controller ? controller->GetVisibleEntry() : nullptr;
}

base::string16 ChromeToolbarModelDelegate::FormattedStringWithEquivalentMeaning(
    const GURL& url,
    const base::string16& formatted_url) const {
  return AutocompleteInput::FormattedStringWithEquivalentMeaning(
      url, formatted_url, ChromeAutocompleteSchemeClassifier(GetProfile()),
      nullptr);
}

bool ChromeToolbarModelDelegate::GetURL(GURL* url) const {
  DCHECK(url);
  content::NavigationEntry* entry = GetNavigationEntry();
  if (!entry)
    return false;

  *url = ShouldDisplayURL() ? entry->GetVirtualURL() : GURL();
  return true;
}

bool ChromeToolbarModelDelegate::ShouldDisplayURL() const {
  // Note: The order here is important.
  // - The WebUI test must come before the extension scheme test because there
  //   can be WebUIs that have extension schemes (e.g. the bookmark manager). In
  //   that case, we should prefer what the WebUI instance says.
  // - The view-source test must come before the NTP test because of the case
  //   of view-source:chrome://newtab, which should display its URL despite what
  //   chrome://newtab says.
  content::NavigationEntry* entry = GetNavigationEntry();
  if (!entry)
    return true;

  if (entry->IsViewSourceMode() ||
      entry->GetPageType() == content::PAGE_TYPE_INTERSTITIAL) {
    return true;
  }

  GURL url = entry->GetURL();
  GURL virtual_url = entry->GetVirtualURL();
  if (url.SchemeIs(content::kChromeUIScheme) ||
      virtual_url.SchemeIs(content::kChromeUIScheme)) {
    if (!url.SchemeIs(content::kChromeUIScheme))
      url = virtual_url;
    return url.host() != chrome::kChromeUINewTabHost;
  }

  Profile* profile = GetProfile();
  return !profile || !search::IsInstantNTPURL(url, profile);
}

security_state::SecurityLevel ChromeToolbarModelDelegate::GetSecurityLevel()
    const {
  content::WebContents* web_contents = GetActiveWebContents();
  // If there is no active WebContents (which can happen during toolbar
  // initialization), assume no security style.
  if (!web_contents)
    return security_state::NONE;
  auto* helper = SecurityStateTabHelper::FromWebContents(web_contents);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  return security_info.security_level;
}

scoped_refptr<net::X509Certificate> ChromeToolbarModelDelegate::GetCertificate()
    const {
  content::NavigationEntry* entry = GetNavigationEntry();
  if (!entry)
    return scoped_refptr<net::X509Certificate>();
  return entry->GetSSL().certificate;
}

bool ChromeToolbarModelDelegate::FailsBillingCheck() const {
  content::WebContents* web_contents = GetActiveWebContents();
  // If there is no active WebContents (which can happen during toolbar
  // initialization), nothing can fail.
  if (!web_contents)
    return false;
  security_state::SecurityInfo security_info;
  SecurityStateTabHelper::FromWebContents(web_contents)
      ->GetSecurityInfo(&security_info);
  return security_info.malicious_content_status ==
         security_state::MALICIOUS_CONTENT_STATUS_BILLING;
}

bool ChromeToolbarModelDelegate::FailsMalwareCheck() const {
  content::WebContents* web_contents = GetActiveWebContents();
  // If there is no active WebContents (which can happen during toolbar
  // initialization), nothing can fail.
  if (!web_contents)
    return false;
  security_state::SecurityInfo security_info;
  SecurityStateTabHelper::FromWebContents(web_contents)
      ->GetSecurityInfo(&security_info);
  const auto status = security_info.malicious_content_status;
  return status != security_state::MALICIOUS_CONTENT_STATUS_BILLING &&
         status != security_state::MALICIOUS_CONTENT_STATUS_NONE;
}

const gfx::VectorIcon* ChromeToolbarModelDelegate::GetVectorIconOverride()
    const {
#if !defined(OS_ANDROID)
  GURL url;
  GetURL(&url);

  if (url.SchemeIs(content::kChromeUIScheme))
    return &omnibox::kProductIcon;

  if (url.SchemeIs(extensions::kExtensionScheme))
    return &omnibox::kExtensionAppIcon;
#endif

  return nullptr;
}

bool ChromeToolbarModelDelegate::IsOfflinePage() const {
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  content::WebContents* web_contents = GetActiveWebContents();
  return web_contents &&
         offline_pages::OfflinePageUtils::GetOfflinePageFromWebContents(
             web_contents);
#else
  return false;
#endif
}

content::NavigationController*
ChromeToolbarModelDelegate::GetNavigationController() const {
  // This |current_tab| can be null during the initialization of the toolbar
  // during window creation (i.e. before any tabs have been added to the
  // window).
  content::WebContents* current_tab = GetActiveWebContents();
  return current_tab ? &current_tab->GetController() : nullptr;
}

Profile* ChromeToolbarModelDelegate::GetProfile() const {
  content::NavigationController* controller = GetNavigationController();
  return controller
             ? Profile::FromBrowserContext(controller->GetBrowserContext())
             : nullptr;
}
