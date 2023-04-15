// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt_helper.h"

#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt.h"
#include "chrome/common/extensions/chrome_manifest_url_handlers.h"
#include "chrome/common/webui_url_constants.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"

namespace {

// Gets the type of prompt that should be displayed for |profile|, this includes
// the possibility of no prompt being required.
PrivacySandboxService::PromptType GetRequiredPromptType(Profile* profile) {
  if (!profile || !profile->IsRegularProfile())
    return PrivacySandboxService::PromptType::kNone;

  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(profile);
  if (!privacy_sandbox_service) {
    return PrivacySandboxService::PromptType::kNone;
  }

  return privacy_sandbox_service->GetRequiredPromptType();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool HasExtensionNtpOverride(
    extensions::ExtensionRegistry* extension_registry) {
  for (const auto& extension : extension_registry->enabled_extensions()) {
    const auto& overrides =
        extensions::URLOverrides::GetChromeURLOverrides(extension.get());
    if (overrides.find(chrome::kChromeUINewTabHost) != overrides.end()) {
      return true;
    }
  }
  return false;
}

// Returns whether |url| is an NTP controlled entirely by Chrome.
bool IsChromeControlledNtpUrl(const GURL& url) {
  // Convert to origins for comparison, as any appended paths are irrelevant.
  const auto ntp_origin = url::Origin::Create(url);

  return ntp_origin ==
             url::Origin::Create(GURL(chrome::kChromeUINewTabPageURL)) ||
         ntp_origin == url::Origin::Create(
                           GURL(chrome::kChromeUINewTabPageThirdPartyURL));
}
#endif

}  // namespace

PrivacySandboxPromptHelper::~PrivacySandboxPromptHelper() = default;

PrivacySandboxPromptHelper::PrivacySandboxPromptHelper(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<PrivacySandboxPromptHelper>(*web_contents) {}

void PrivacySandboxPromptHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!ProfileRequiresPrompt(profile()))
    return;

  // Only valid top frame navigations are considered.
  if (!navigation_handle || !navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/1315580, crbug.com/1315579): When navigating to a NTP that
  // isn't Chrome-controlled on ChromeOS, open an about blank tab to display the
  // prompt. On other platforms, it's being handled during the startup. This
  // logic can be removed when Lacros is ready.
  if (web_contents()->GetLastCommittedURL() == chrome::kChromeUINewTabURL) {
    const bool has_extention_override =
        HasExtensionNtpOverride(extensions::ExtensionRegistry::Get(profile()));

    const GURL new_tab_page = search::GetNewTabPageURL(profile());
    const bool is_non_chrome_controlled_ntp =
        navigation_handle->GetURL() == new_tab_page &&
        !IsChromeControlledNtpUrl(new_tab_page);

    if (has_extention_override || is_non_chrome_controlled_ntp) {
      web_contents()->OpenURL(content::OpenURLParams(
          GURL(url::kAboutBlankURL), content::Referrer(),
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_AUTO_TOPLEVEL, /*is_renderer_initiated=*/false));
      return;
    }
  }
#endif

  // Check whether the navigation target is a suitable prompt location. The
  // navigation URL, rather than the visible or committed URL, is required to
  // distinguish between different types of NTPs.
  if (!PrivacySandboxService::IsUrlSuitableForPrompt(
          navigation_handle->GetURL())) {
    return;
  }

  // If a Sync setup is in progress, the prompt should not be shown.
  if (auto* sync_service = SyncServiceFactory::GetForProfile(profile())) {
    if (sync_service->IsSetupInProgress())
      return;
  }

  auto* browser =
      chrome::FindBrowserWithWebContents(navigation_handle->GetWebContents());

  // If a Privacy Sandbox prompt already exists for this browser, do not attempt
  // to open another one.
  if (auto* privacy_sandbox_service =
          PrivacySandboxServiceFactory::GetForProfile(profile())) {
    if (privacy_sandbox_service->IsPromptOpenForBrowser(browser)) {
      return;
    }
  }

  const bool is_window_too_small = !CanWindowFitPrivacySandboxPrompt(browser);
  base::UmaHistogramBoolean("Settings.PrivacySandbox.DialogWindowTooSmall",
                            is_window_too_small);
  // If the windows size is too small, it is difficult to read or interrupt with
  // the dialog. The dialog is blocking modal, that is why we want to prevent it
  // from showing if there isn't enough space.
  if (is_window_too_small) {
    return;
  }

  // Record the URL that the prompt was displayed over.
  uint32_t host_hash = base::Hash(navigation_handle->GetURL().IsAboutBlank()
                                      ? "about:blank"
                                      : navigation_handle->GetURL().host());
  base::UmaHistogramSparse("Settings.PrivacySandbox.DialogDisplayHost",
                           static_cast<base::HistogramBase::Sample>(host_hash));

  browser->tab_strip_model()->ActivateTabAt(
      browser->tab_strip_model()->GetIndexOfWebContents(
          navigation_handle->GetWebContents()));

  ShowPrivacySandboxPrompt(browser, GetRequiredPromptType(profile()));
}

// static
bool PrivacySandboxPromptHelper::ProfileRequiresPrompt(Profile* profile) {
  return GetRequiredPromptType(profile) !=
         PrivacySandboxService::PromptType::kNone;
}

Profile* PrivacySandboxPromptHelper::profile() {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrivacySandboxPromptHelper);
