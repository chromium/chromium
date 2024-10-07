// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_UI_UTILS_H_
#define CHROME_BROWSER_UI_PASSWORDS_UI_UTILS_H_

#include <string>
#include <utility>

#include "build/build_config.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "ui/gfx/vector_icon_types.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace password_manager {
struct PasswordForm;
}  // namespace password_manager

class GURL;

enum class PasswordTitleType {
  SAVE_PASSWORD,    // plain password
  SAVE_ACCOUNT,     // login via IDP
  UPDATE_PASSWORD,  // update plain password
};

class Browser;
class Profile;

// The desired width and height in pixels for an account avatar.
constexpr int kAvatarImageSize = 32;

// Crops and scales |image_skia| to the desired size for an account avatar.
gfx::ImageSkia ScaleImageForAccountAvatar(gfx::ImageSkia image_skia);

// Returns the upper and lower label to be displayed in the account chooser UI
// for |form|. The lower label can be multiline.
std::pair<std::u16string, std::u16string> GetCredentialLabelsForAccountChooser(
    const password_manager::PasswordForm& form);

// Returns the formatted title in the Save Password bubble or the Update
// Password bubble (depending on |dialog_type|). If the registry controlled
// domain of |user_visible_url| (i.e. the one seen in the omnibox) differs from
// the registry controlled domain of |form_origin_url|, it adds the site name.
std::u16string GetSavePasswordDialogTitleText(
    const GURL& user_visible_url,
    const url::Origin& form_origin_url,
    PasswordTitleType dialog_type);

// Returns the formatted title in the Manage Passwords bubble. If the registry
// controlled domain of |user_visible_url| (i.e. the one seen in the omnibox)
// differs from the domain of the managed password origin URL
// |password_origin_url|, sets |IDS_MANAGE_PASSWORDS_DIFFERENT_DOMAIN_TITLE| or
// |IDS_MANAGE_PASSWORDS_DIFFERENT_DOMAIN_NO_PASSWORDS_TITLE| as
// the title so that it replaces "this site" in title text with output of
// |FormatUrlForSecurityDisplay(password_origin_url)|.
// Otherwise, sets |IDS_MANAGE_PASSWORDS_TITLE| or
// |IDS_MANAGE_PASSWORDS_NO_PASSWORDS_TITLE| as the title having "this site".
// The *_NO_PASSWORDS_* variants of the title strings are used when no
// credentials are present.
std::u16string GetManagePasswordsDialogTitleText(
    const GURL& user_visible_url,
    const url::Origin& password_origin_url,
    bool has_credentials);

// Returns text that is used when manage passwords bubble is used as a
// confirmation.
std::u16string GetConfirmationManagePasswordsDialogTitleText(bool is_update);

// Returns an username in the form that should be shown in the bubble.
std::u16string GetDisplayUsername(const password_manager::PasswordForm& form);

// Returns either the username or the |IDS_PASSWORD_MANAGER_EMPTY_LOGIN| in case
// it is empty.
std::u16string GetDisplayUsername(
    const password_manager::UiCredential& credential);

// Returns |federation_origin| in a human-readable format.
std::u16string GetDisplayFederation(const password_manager::PasswordForm& form);

// Returns the plain text representation of the password in the form that should
// be shown in the bubble.
std::u16string GetDisplayPassword(const password_manager::PasswordForm& form);

// Check if |profile| syncing the Auto sign-in settings (by checking that user
// syncs the PRIORITY_PREFERENCE). The view appearance might depend on it.
bool IsSyncingAutosignSetting(Profile* profile);

// Returns a string URL to the Google Password Manager's passwords subpage
std::string GetGooglePasswordManagerSubPageURLStr();

#if !BUILDFLAG(IS_ANDROID)
// Navigates to the Google Password Manager page.
void NavigateToManagePasswordsPage(
    Browser* browser,
    password_manager::ManagePasswordsReferrer referrer);

// Navigates to the Google Password Manager subpage to show the credential
// details for the `password_domain_name`.
void NavigateToPasswordDetailsPage(
    Browser* browser,
    const std::string& password_domain_name,
    password_manager::ManagePasswordsReferrer referrer);

// Navigates to the Password Manager settings page and focuses the account store
// toggle.
void NavigateToManagePasswordsSettingsAccountStoreToggle(Browser* browser);

#endif  // !BUILDFLAG(IS_ANDROID)

mojo::Remote<network::mojom::URLLoaderFactory> GetURLLoaderForMainFrame(
    content::WebContents* web_contents);

// Returns that vector icon to represent Google Password Manager in Desktop UI.
// Returns different version for branded builds.
const gfx::VectorIcon& GooglePasswordManagerVectorIcon();

#endif  // CHROME_BROWSER_UI_PASSWORDS_UI_UTILS_H_
