// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/ui_utils.h"

#include <stddef.h>

#include <algorithm>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/user_education/show_promo_in_page.h"
#include "chrome/browser/ui/webui/password_manager/password_manager_ui.h"
#endif

namespace {

using password_manager::ManagePasswordsReferrer;

// Checks whether two URLs are from the same domain or host.
bool SameDomainOrHost(const GURL& gurl, const url::Origin& origin) {
  return net::registry_controlled_domains::SameDomainOrHost(
      gurl, origin,
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
      skia_image, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kAvatarImageSize, kAvatarImageSize));
}

std::pair<std::u16string, std::u16string> GetCredentialLabelsForAccountChooser(
    const password_manager::PasswordForm& form) {
  std::u16string federation;
  if (form.IsFederatedCredential()) {
    federation = GetDisplayFederation(form);
  }

  if (form.display_name.empty()) {
    return std::make_pair(form.username_value, std::move(federation));
  }

  // Display name isn't empty.
  if (federation.empty()) {
    return std::make_pair(form.display_name, form.username_value);
  }

  return std::make_pair(form.display_name,
                        form.username_value + u"\n" + federation);
}

std::u16string GetSavePasswordDialogTitleText(
    const GURL& user_visible_url,
    const url::Origin& form_origin_url,
    PasswordTitleType dialog_type) {
  std::vector<size_t> offsets;
  std::vector<std::u16string> replacements;
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
    replacements.push_back(url_formatter::FormatOriginForSecurityDisplay(
        form_origin_url, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
  }

  return l10n_util::GetStringFUTF16(title_id, replacements, &offsets);
}

std::u16string GetManagePasswordsDialogTitleText(
    const GURL& user_visible_url,
    const url::Origin& password_origin_url,
    bool has_credentials) {
  DCHECK(!password_origin_url.opaque());
  // Check whether the registry controlled domains for user-visible URL
  // (i.e. the one seen in the omnibox) and the managed password origin URL
  // differ or not.
  if (!SameDomainOrHost(user_visible_url, password_origin_url)) {
    std::u16string formatted_url =
        url_formatter::FormatOriginForSecurityDisplay(password_origin_url);
    return l10n_util::GetStringFUTF16(
        has_credentials
            ? IDS_MANAGE_PASSWORDS_DIFFERENT_DOMAIN_TITLE
            : IDS_MANAGE_PASSWORDS_DIFFERENT_DOMAIN_NO_PASSWORDS_TITLE,
        formatted_url);
  }
  return l10n_util::GetStringUTF16(
      has_credentials ? IDS_MANAGE_PASSWORDS_TITLE
                      : IDS_MANAGE_PASSWORDS_NO_PASSWORDS_TITLE);
}

std::u16string GetConfirmationManagePasswordsDialogTitleText(bool is_update) {
  return is_update ? l10n_util::GetStringUTF16(
                         IDS_PASSWORD_MANAGER_CONFIRM_UPDATE_TITLE)
                   : l10n_util::GetStringUTF16(
                         IDS_PASSWORD_MANAGER_CONFIRM_SAVED_TITLE);
}

std::u16string GetDisplayUsername(const password_manager::PasswordForm& form) {
  return form.username_value.empty()
             ? l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN)
             : form.username_value;
}

std::u16string GetDisplayUsername(
    const password_manager::UiCredential& credential) {
  return credential.username().empty()
             ? l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN)
             : credential.username();
}

std::u16string GetDisplayFederation(
    const password_manager::PasswordForm& form) {
  return url_formatter::FormatUrlForSecurityDisplay(
      form.federation_origin.GetURL(),
      url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
}

std::u16string GetDisplayPassword(const password_manager::PasswordForm& form) {
  return !form.IsFederatedCredential()
             ? form.password_value
             : l10n_util::GetStringFUTF16(IDS_PASSWORDS_VIA_FEDERATION,
                                          GetDisplayFederation(form));
}

bool IsSyncingAutosignSetting(Profile* profile) {
  const syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  return (
      sync_service &&
      sync_service->GetActiveDataTypes().Has(syncer::PRIORITY_PREFERENCES));
}

std::string GetGooglePasswordManagerSubPageURLStr() {
  return base::StrCat({chrome::kChromeUIPasswordManagerURL, "/",
                       chrome::kPasswordManagerSubPage});
}

// Navigation is handled differently on Android.
#if !BUILDFLAG(IS_ANDROID)
void NavigateToManagePasswordsPage(Browser* browser,
                                   ManagePasswordsReferrer referrer) {
  base::UmaHistogramEnumeration("PasswordManager.ManagePasswordsReferrer",
                                referrer);
  chrome::ShowPasswordManager(browser);
}

void NavigateToPasswordDetailsPage(Browser* browser,
                                   const std::string& password_domain_name,
                                   ManagePasswordsReferrer referrer) {
  base::UmaHistogramEnumeration("PasswordManager.ManagePasswordsReferrer",
                                referrer);
  chrome::ShowPasswordDetailsPage(browser, password_domain_name);
}

void NavigateToManagePasswordsSettingsAccountStoreToggle(Browser* browser) {
  ShowPromoInPage::Params params;
  params.target_url = GURL(chrome::kChromeUIPasswordManagerSettingsURL);
  params.bubble_anchor_id = PasswordManagerUI::kAccountStoreToggleElementId;
  params.bubble_arrow = user_education::HelpBubbleArrow::kTopRight;
  params.bubble_text = l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_IPH_ACCOUNT_STORAGE_TOGGLE);

  ShowPromoInPage::Start(browser, std::move(params));
}

#endif  // !BUILDFLAG(IS_ANDROID)

mojo::Remote<network::mojom::URLLoaderFactory> GetURLLoaderForMainFrame(
    content::WebContents* web_contents) {
  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory;
  frame->CreateNetworkServiceDefaultFactory(
      url_loader_factory.BindNewPipeAndPassReceiver());
  return url_loader_factory;
}

const gfx::VectorIcon& GooglePasswordManagerVectorIcon() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return vector_icons::kGooglePasswordManagerIcon;
#else
  return kKeyIcon;
#endif
}
