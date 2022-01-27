// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_dialog_helper.h"

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_dialog.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

namespace {

// Gets the type of dialog that should be displayed for |profile|, this includes
// the possibility of no dialog being required.
PrivacySandboxService::DialogType GetRequiredDialogType(Profile* profile) {
  if (!profile || !profile->IsRegularProfile())
    return PrivacySandboxService::DialogType::kNone;

  auto* privacy_sandbox_serivce =
      PrivacySandboxServiceFactory::GetForProfile(profile);
  if (!privacy_sandbox_serivce)
    return PrivacySandboxService::DialogType::kNone;

  return privacy_sandbox_serivce->GetRequiredDialogType();
}

}  // namespace

PrivacySandboxDialogHelper::~PrivacySandboxDialogHelper() = default;

PrivacySandboxDialogHelper::PrivacySandboxDialogHelper(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<PrivacySandboxDialogHelper>(*web_contents) {}

void PrivacySandboxDialogHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Ignore everything except chrome:// urls.
  if (!navigation_handle || !navigation_handle->IsInMainFrame() ||
      !navigation_handle->GetWebContents()->GetLastCommittedURL().SchemeIs(
          content::kChromeUIScheme)) {
    return;
  }

  // TODO(crbug.com/1286276): This logic is too simple, there are more
  // circumstances on which we may not wish to show the dialog, such as sync
  // consent currently in progress.
  if (!ProfileRequiresDialog(profile()))
    return;

  if (auto* browser = chrome::FindBrowserWithProfile(profile()))
    ShowPrivacySandboxDialog(browser, GetRequiredDialogType(profile()));
}

// static
bool PrivacySandboxDialogHelper::ProfileRequiresDialog(Profile* profile) {
  return GetRequiredDialogType(profile) !=
         PrivacySandboxService::DialogType::kNone;
}

Profile* PrivacySandboxDialogHelper::profile() {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrivacySandboxDialogHelper);
