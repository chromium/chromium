// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_VIEW_UTILS_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_VIEW_UTILS_H_

#include "base/strings/string16.h"

namespace autofill {
struct PasswordForm;
}

namespace gfx {
class ImageSkia;
}  // namespace gfx

class GURL;

enum class PasswordTitleType {
  SAVE_PASSWORD,    // plain password
  SAVE_ACCOUNT,     // login via IDP
  UPDATE_PASSWORD,  // update plain password
};

class Profile;

// The desired width and height in pixels for an account avatar.
constexpr int kAvatarImageSize = 32;

// The desired width and height for the 'i' icon used for the PSL matches in the
// account chooser.
constexpr int kInfoIconSize = 16;

// Crops and scales |image_skia| to the desired size for an account avatar.
gfx::ImageSkia ScaleImageForAccountAvatar(gfx::ImageSkia image_skia);

// Returns the upper and lower label to be displayed in the account chooser UI
// for |form|. The lower label can be multiline.
std::pair<base::string16, base::string16> GetCredentialLabelsForAccountChooser(
    const autofill::PasswordForm& form);

// Sets the formatted |title| in the Save Password bubble or the Update Password
// bubble (depending on |dialog_type|). If the registry controlled domain of
// |user_visible_url| (i.e. the one seen in the omnibox) differs from the
// registry controlled domain of |form_origin_url|, it adds the site name.
void GetSavePasswordDialogTitleTextAndLinkRange(const GURL& user_visible_url,
                                                const GURL& form_origin_url,
                                                PasswordTitleType dialog_type,
                                                base::string16* title);

// Sets the formatted |title| in the Manage Passwords bubble. If the registry
// controlled domain of |user_visible_url| (i.e. the one seen in the omnibox)
// differs from the domain of the managed password origin URL
// |password_origin_url|, sets |IDS_MANAGE_PASSWORDS_DIFFERENT_DOMAIN_TITLE| or
// |IDS_MANAGE_PASSWORDS_DIFFERENT_DOMAIN_NO_PASSWORDS_TITLE| as
// the |title| so that it replaces "this site" in title text with output of
// |FormatUrlForSecurityDisplay(password_origin_url)|.
// Otherwise, sets |IDS_MANAGE_PASSWORDS_TITLE| or
// |IDS_MANAGE_PASSWORDS_NO_PASSWORDS_TITLE| as |title| having "this site".
// The *_NO_PASSWORDS_* variants of the title strings are used when no
// credentials are present.
void GetManagePasswordsDialogTitleText(const GURL& user_visible_url,
                                       const GURL& password_origin_url,
                                       bool has_credentials,
                                       base::string16* title);

// Returns an username in the form that should be shown in the bubble.
base::string16 GetDisplayUsername(const autofill::PasswordForm& form);

// Check if |profile| syncing the Auto sign-in settings (by checking that user
// syncs the PRIORITY_PREFERENCE). The view appearance might depend on it.
bool IsSyncingAutosignSetting(Profile* profile);

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_VIEW_UTILS_H_
