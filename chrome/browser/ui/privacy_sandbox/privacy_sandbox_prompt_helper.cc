// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt_helper.h"

#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

namespace {

// Gets the type of prompt that should be displayed for |profile|, this includes
// the possibility of no prompt being required.
PrivacySandboxService::PromptType GetRequiredPromptType(Profile* profile) {
  if (!profile || !profile->IsRegularProfile())
    return PrivacySandboxService::PromptType::kNone;

  auto* privacy_sandbox_serivce =
      PrivacySandboxServiceFactory::GetForProfile(profile);
  if (!privacy_sandbox_serivce)
    return PrivacySandboxService::PromptType::kNone;

  return privacy_sandbox_serivce->GetRequiredPromptType();
}

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
  if (auto* privacy_sandbox_serivce =
          PrivacySandboxServiceFactory::GetForProfile(profile())) {
    if (privacy_sandbox_serivce->IsPromptOpenForBrowser(browser))
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
