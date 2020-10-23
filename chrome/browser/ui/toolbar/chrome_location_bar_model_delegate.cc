// Copyright 2016 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/login/login_tab_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"

#if !defined(OS_ANDROID)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif                                                // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_utils.h"
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"

// Id for extension that enables users to report sites to Safe Browsing.
const char kPreventElisionExtensionId[] = "jknemblkbdhdcpllfgbfekkdciegfboi";
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

ChromeLocationBarModelDelegate::ChromeLocationBarModelDelegate() {}

ChromeLocationBarModelDelegate::~ChromeLocationBarModelDelegate() {}

content::NavigationEntry* ChromeLocationBarModelDelegate::GetNavigationEntry()
    const {
  content::NavigationController* controller = GetNavigationController();
  return controller ? controller->GetVisibleEntry() : nullptr;
}

base::string16
ChromeLocationBarModelDelegate::FormattedStringWithEquivalentMeaning(
    const GURL& url,
    const base::string16& formatted_url) const {
  return AutocompleteInput::FormattedStringWithEquivalentMeaning(
      url, formatted_url, ChromeAutocompleteSchemeClassifier(GetProfile()),
      nullptr);
}

bool ChromeLocationBarModelDelegate::GetURL(GURL* url) const {
  DCHECK(url);
  content::NavigationEntry* entry = GetNavigationEntry();
  if (!entry)
    return false;

  *url = entry->GetVirtualURL();
  return true;
}

bool ChromeLocationBarModelDelegate::ShouldPreventElision() {
  RecordElisionConfig();
  if (GetElisionConfig() != ELISION_CONFIG_DEFAULT) {
    return true;
  }
  return false;
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
  if (!entry)
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

  if (entry->IsViewSourceMode() ||
      entry->GetPageType() == content::PAGE_TYPE_INTERSTITIAL) {
    return true;
  }

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
  if (!entry)
    return scoped_refptr<net::X509Certificate>();
  return entry->GetSSL().certificate;
}

const gfx::VectorIcon* ChromeLocationBarModelDelegate::GetVectorIconOverride()
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
  if (!entry)
    return false;

  Profile* const profile = GetProfile();
  if (!profile)
    return false;

  if (!search::DefaultSearchProviderIsGoogle(profile))
    return false;

  GURL ntp_url(base::FeatureList::IsEnabled(ntp_features::kWebUI)
                   ? chrome::kChromeUINewTabPageURL
                   : chrome::kChromeSearchLocalNtpUrl);
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

ChromeLocationBarModelDelegate::ElisionConfig
ChromeLocationBarModelDelegate::GetElisionConfig() const {
  Profile* const profile = GetProfile();
  if (profile &&
      profile->GetPrefs()->GetBoolean(omnibox::kPreventUrlElisionsInOmnibox)) {
    return ELISION_CONFIG_TURNED_OFF_BY_PREF;
  }
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (profile && extensions::ExtensionRegistry::Get(profile)
                     ->enabled_extensions()
                     .Contains(kPreventElisionExtensionId)) {
    return ELISION_CONFIG_TURNED_OFF_BY_EXTENSION;
  }
#endif
  return ELISION_CONFIG_DEFAULT;
}

void ChromeLocationBarModelDelegate::RecordElisionConfig() {
  Profile* const profile = GetProfile();
  // Only record metrics once for this object, and only record if the profile
  // has already been created to avoid false logging of the default config.
  if (elision_config_recorded_ || !profile) {
    return;
  }
  UMA_HISTOGRAM_ENUMERATION("Omnibox.ElisionConfig", GetElisionConfig(),
                            ELISION_CONFIG_MAX);
  elision_config_recorded_ = true;
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
