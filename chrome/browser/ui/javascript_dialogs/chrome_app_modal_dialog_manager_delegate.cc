// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/chrome_app_modal_dialog_manager_delegate.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/url_identity.h"
#include "components/javascript_dialogs/app_modal_dialog_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace {
// Expected URL types for `UrlIdentity::CreateFromUrl(`.
constexpr UrlIdentity::TypeSet kUrlIdentityAllowedTypes = {
    UrlIdentity::Type::kDefault, UrlIdentity::Type::kFile,
    UrlIdentity::Type::kIsolatedWebApp, UrlIdentity::Type::kChromeExtension};
constexpr UrlIdentity::FormatOptions kUrlIdentityOptions = {
    .default_options = {}};
}  // namespace

ChromeAppModalDialogManagerDelegate::~ChromeAppModalDialogManagerDelegate() =
    default;

std::u16string ChromeAppModalDialogManagerDelegate::GetTitle(
    content::WebContents* web_contents,
    const url::Origin& origin) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  UrlIdentity url_identity = UrlIdentity::CreateFromUrl(
      profile, origin.GetURL(), kUrlIdentityAllowedTypes, kUrlIdentityOptions);

  if (url_identity.type == UrlIdentity::Type::kChromeExtension) {
    return l10n_util::GetStringFUTF16(IDS_JAVASCRIPT_MESSAGEBOX_TITLE_EXTENSION,
                                      url_identity.name);
  }

  if (url_identity.type == UrlIdentity::Type::kIsolatedWebApp) {
    return url_identity.name;
  }

  return javascript_dialogs::AppModalDialogManager::GetSiteFrameTitle(
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin(), origin);
}
