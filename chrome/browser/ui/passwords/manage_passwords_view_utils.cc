// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"

#include <stddef.h>

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// Checks whether two URLs are from the same domain or host.
bool SameDomainOrHost(const GURL& gurl1, const GURL& gurl2) {
  return net::registry_controlled_domains::SameDomainOrHost(
      gurl1, gurl2,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

gfx::ImageSkia ScaleImageForAccountAvatar(gfx::ImageSkia skia_image) {
  gfx::Size size = skia_image.size();
  if (size.height() != size.width()) {
    gfx::Rect target(size);
    int side = std::min(size.height(), size.width());
    target.ClampToCenteredSize(gfx::Size(side, side));
    skia_image = gfx::ImageSkiaOperations::ExtractSubset(skia_image, target);
  }
  return gfx::ImageSkiaOperations::CreateResizedImage(
      skia_image,
      skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kAvatarImageSize, kAvatarImageSize));
}

std::pair<base::string16, base::string16> GetCredentialLabelsForAccountChooser(
    const autofill::PasswordForm& form) {
  base::string16 federation;
  if (!form.federation_origin.opaque())
    federation = base::UTF8ToUTF16(form.federation_origin.host());

  if (form.display_name.empty())
    return std::make_pair(form.username_value, std::move(federation));

  // Display name isn't empty.
  if (federation.empty())
    return std::make_pair(form.display_name, form.username_value);

  return std::make_pair(
      form.display_name,
      form.username_value + base::ASCIIToUTF16("\n") + federation);
}

void GetSavePasswordDialogTitleTextAndLinkRange(const GURL& user_visible_url,
                                                const GURL& form_origin_url,
                                                PasswordTitleType dialog_type,
                                                base::string16* title) {
  DCHECK(!password_manager::IsValidAndroidFacetURI(form_origin_url.spec()));
  std::vector<size_t> offsets;
  std::vector<base::string16> replacements;
  int title_id = 0;
  switch (dialog_type) {
    case PasswordTitleType::SAVE_PASSWORD:
      title_id = IDS_SAVE_PASSWORD;
      break;
    case PasswordTitleType::SAVE_ACCOUNT:
      title_id = IDS_SAVE_ACCOUNT;
      break;
    case PasswordTitleType::UPDATE_PASSWORD:
      title_id = IDS_UPDATE_PASSWORD;
      break;
  }

  // Check whether the registry controlled domains for user-visible URL (i.e.
  // the one seen in the omnibox) and the password form post-submit navigation
  // URL differs or not.
  if (!SameDomainOrHost(user_visible_url, form_origin_url)) {
    DCHECK_NE(PasswordTitleType::SAVE_ACCOUNT, dialog_type)
        << "Calls to save account should always happen on the same domain.";
    title_id = dialog_type == PasswordTitleType::UPDATE_PASSWORD
                   ? IDS_UPDATE_PASSWORD_DIFFERENT_DOMAINS_TITLE
                   : IDS_SAVE_PASSWORD_DIFFERENT_DOMAINS_TITLE;
    replacements.push_back(url_formatter::FormatUrlForSecurityDisplay(
        form_origin_url, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
  }

  *title = l10n_util::GetStringFUTF16(title_id, replacements, &offsets);
}

void GetManagePasswordsDialogTitleText(const GURL& user_visible_url,
                                       const GURL& password_origin_url,
                                       bool has_credentials,
                                       base::string16* title) {
  DCHECK(!password_manager::IsValidAndroidFacetURI(password_origin_url.spec()));
  // Check whether the registry controlled domains for user-visible URL
  // (i.e. the one seen in the omnibox) and the managed password origin URL
  // differ or not.
  if (!SameDomainOrHost(user_visible_url, password_origin_url)) {
    base::string16 formatted_url =
        url_formatter::FormatUrlForSecurityDisplay(password_origin_url);
    *title = l10n_util::GetStringFUTF16(
        has_credentials
            ? IDS_MANAGE_PASSWORDS_DIFFERENT_DOMAIN_TITLE
            : IDS_MANAGE_PASSWORDS_DIFFERENT_DOMAIN_NO_PASSWORDS_TITLE,
        formatted_url);
  } else {
    *title = l10n_util::GetStringUTF16(
        has_credentials ? IDS_MANAGE_PASSWORDS_TITLE
                        : IDS_MANAGE_PASSWORDS_NO_PASSWORDS_TITLE);
  }
}

base::string16 GetDisplayUsername(const autofill::PasswordForm& form) {
  return form.username_value.empty()
             ? l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN)
             : form.username_value;
}

bool IsSyncingAutosignSetting(Profile* profile) {
  const browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  return (sync_service && sync_service->IsFirstSetupComplete() &&
          sync_service->IsSyncFeatureActive() &&
          sync_service->GetActiveDataTypes().Has(syncer::PRIORITY_PREFERENCES));
}
