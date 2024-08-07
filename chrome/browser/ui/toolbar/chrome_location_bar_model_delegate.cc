// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/chrome_location_bar_model_delegate.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/login/login_tab_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/search/ntp_features.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "components/vector_icons/vector_icons.h"     // nogncheck
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_utils.h"
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

ChromeLocationBarModelDelegate::ChromeLocationBarModelDelegate() {}

ChromeLocationBarModelDelegate::~ChromeLocationBarModelDelegate() {}

content::NavigationEntry* ChromeLocationBarModelDelegate::GetNavigationEntry()
    const {
  content::NavigationController* controller = GetNavigationController();
  return controller ? controller->GetVisibleEntry() : nullptr;
}

std::u16string
ChromeLocationBarModelDelegate::FormattedStringWithEquivalentMeaning(
    const GURL& url,
    const std::u16string& formatted_url) const {
  return AutocompleteInput::FormattedStringWithEquivalentMeaning(
      url, formatted_url, ChromeAutocompleteSchemeClassifier(GetProfile()),
      nullptr);
}

bool ChromeLocationBarModelDelegate::GetURL(GURL* url) const {
  DCHECK(url);
  content::NavigationEntry* entry = GetNavigationEntry();
  if (!entry || entry->IsInitialEntry())
    return false;

  *url = entry->GetVirtualURL();
  return true;
}

bool ChromeLocationBarModelDelegate::ShouldPreventElision() {
  Profile* const profile = GetProfile();
  if (profile &&
      profile->GetPrefs()->GetBoolean(omnibox::kPreventUrlElisionsInOmnibox)) {
    return true;
  }
  return net::IsCertStatusError(GetVisibleSecurityState()->cert_status);
}

bool ChromeLocationBarModelDelegate::ShouldDisplayURL() const {
  // Note: The order here is important.
  // - The WebUI test must come before the extension scheme test because there
  //   can be WebUIs that have extension schemes (e.g. the bookmark manager). In
  //   that case, we should prefer what the WebUI instance says.
  // - The view-source test must come before the NTP test because of the case
  //   of view-source:chrome://newtab, which should display its URL despite what
  //   chrome://newtab says.
  content::NavigationEntry* entry = GetNavigationEntry();
  if (!entry || entry->IsInitialEntry())
    return true;

  security_interstitials::SecurityInterstitialTabHelper*
      security_interstitial_tab_helper =
          security_interstitials::SecurityInterstitialTabHelper::
              FromWebContents(GetActiveWebContents());
  if (security_interstitial_tab_helper &&
      security_interstitial_tab_helper->IsDisplayingInterstitial())
    return security_interstitial_tab_helper->ShouldDisplayURL();

  LoginTabHelper* login_tab_helper =
      LoginTabHelper::FromWebContents(GetActiveWebContents());
  if (login_tab_helper && login_tab_helper->IsShowingPrompt())
    return login_tab_helper->ShouldDisplayURL();

  if (entry->IsViewSourceMode())
    return true;

  const auto is_ntp = [](const GURL& url) {
    return url.SchemeIs(content::kChromeUIScheme) &&
           url.host() == chrome::kChromeUINewTabHost;
  };

  GURL url = entry->GetURL();
  if (is_ntp(entry->GetVirtualURL()) || is_ntp(url))
    return false;

  Profile* profile = GetProfile();
  return !profile || !search::IsInstantNTPURL(url, profile);
}

bool ChromeLocationBarModelDelegate::
    ShouldUseUpdatedConnectionSecurityIndicators() const {
  return base::FeatureList::IsEnabled(
      omnibox::kUpdatedConnectionSecurityIndicators);
}

security_state::SecurityLevel ChromeLocationBarModelDelegate::GetSecurityLevel()
    const {
  content::WebContents* web_contents = GetActiveWebContents();
  // If there is no active WebContents (which can happen during toolbar
  // initialization), assume no security style.
  if (!web_contents) {
    return security_state::NONE;
  }
  auto* helper = SecurityStateTabHelper::FromWebContents(web_contents);
  return helper->GetSecurityLevel();
}

net::CertStatus ChromeLocationBarModelDelegate::GetCertStatus() const {
  content::WebContents* web_contents = GetActiveWebContents();
  // If there is no active WebContents (which can happen during toolbar
  // initialization), assume no cert status.
  if (!web_contents) {
    return 0;
  }
  auto* helper = SecurityStateTabHelper::FromWebContents(web_contents);
  return helper->GetVisibleSecurityState()->cert_status;
}

std::unique_ptr<security_state::VisibleSecurityState>
ChromeLocationBarModelDelegate::GetVisibleSecurityState() const {
  content::WebContents* web_contents = GetActiveWebContents();
  // If there is no active WebContents (which can happen during toolbar
  // initialization), assume no security info.
  if (!web_contents) {
    return std::make_unique<security_state::VisibleSecurityState>();
  }
  auto* helper = SecurityStateTabHelper::FromWebContents(web_contents);
  return helper->GetVisibleSecurityState();
}

scoped_refptr<net::X509Certificate>
ChromeLocationBarModelDelegate::GetCertificate() const {
  content::NavigationEntry* entry = GetNavigationEntry();
  if (!entry || entry->IsInitialEntry())
    return scoped_refptr<net::X509Certificate>();
  return entry->GetSSL().certificate;
}

const gfx::VectorIcon* ChromeLocationBarModelDelegate::GetVectorIconOverride()
    const {
#if !BUILDFLAG(IS_ANDROID)
  GURL url;
  GetURL(&url);

  if (url.SchemeIs(content::kChromeUIScheme)) {
    return &omnibox::kProductChromeRefreshIcon;
  }

  if (url.SchemeIs(extensions::kExtensionScheme)) {
    return &vector_icons::kExtensionChromeRefreshIcon;
  }
#endif

  return nullptr;
}

bool ChromeLocationBarModelDelegate::IsOfflinePage() const {
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  content::WebContents* web_contents = GetActiveWebContents();
  return web_contents &&
         offline_pages::OfflinePageUtils::GetOfflinePageFromWebContents(
             web_contents);
#else
  return false;
#endif
}

bool ChromeLocationBarModelDelegate::IsNewTabPage() const {
  content::NavigationEntry* const entry = GetNavigationEntry();
  if (!entry || entry->IsInitialEntry())
    return false;

  Profile* const profile = GetProfile();
  if (!profile)
    return false;

  if (!search::DefaultSearchProviderIsGoogle(profile))
    return false;

  GURL ntp_url(chrome::kChromeUINewTabPageURL);
  return ntp_url.scheme_piece() == entry->GetURL().scheme_piece() &&
         ntp_url.host_piece() == entry->GetURL().host_piece();
}

bool ChromeLocationBarModelDelegate::IsNewTabPageURL(const GURL& url) const {
  return url.spec() == chrome::kChromeUINewTabURL;
}

bool ChromeLocationBarModelDelegate::IsHomePage(const GURL& url) const {
  Profile* const profile = GetProfile();
  if (!profile)
    return false;

  return url.spec() == profile->GetPrefs()->GetString(prefs::kHomePage);
}

content::NavigationController*
ChromeLocationBarModelDelegate::GetNavigationController() const {
  // This |current_tab| can be null during the initialization of the toolbar
  // during window creation (i.e. before any tabs have been added to the
  // window).
  content::WebContents* current_tab = GetActiveWebContents();
  return current_tab ? &current_tab->GetController() : nullptr;
}

Profile* ChromeLocationBarModelDelegate::GetProfile() const {
  content::NavigationController* controller = GetNavigationController();
  return controller
             ? Profile::FromBrowserContext(controller->GetBrowserContext())
             : nullptr;
}

AutocompleteClassifier*
ChromeLocationBarModelDelegate::GetAutocompleteClassifier() {
  Profile* const profile = GetProfile();
  return profile ? AutocompleteClassifierFactory::GetForProfile(profile)
                 : nullptr;
}

TemplateURLService* ChromeLocationBarModelDelegate::GetTemplateURLService() {
  Profile* const profile = GetProfile();
  return profile ? TemplateURLServiceFactory::GetForProfile(profile) : nullptr;
}

// static
void ChromeLocationBarModelDelegate::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(omnibox::kPreventUrlElisionsInOmnibox, false);
}
