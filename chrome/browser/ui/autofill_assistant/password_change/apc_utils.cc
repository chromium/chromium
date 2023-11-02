// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill_assistant/password_change/apc_utils.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/autofill_assistant/password_change/vector_icons/vector_icons.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "components/vector_icons/vector_icons.h"

using autofill_assistant::password_change::TopIcon;

const gfx::VectorIcon& GetAssistantIconOrFallback() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return vector_icons::kAssistantIcon;
#else
  // Only developer builds will ever use this branch.
  return vector_icons::kProductIcon;
#endif
}

const gfx::VectorIcon& GetApcTopIconFromEnum(TopIcon icon, bool dark_mode) {
  if (!dark_mode) {
    switch (icon) {
      case TopIcon::TOP_ICON_UNSPECIFIED:
        return autofill_assistant::password_change::kUnspecifiedStateIcon;
      case TopIcon::TOP_ICON_OPEN_SITE_SETTINGS:
        return autofill_assistant::password_change::kOpenSiteSettingsIcon;
      case TopIcon::TOP_ICON_ENTER_OLD_PASSWORD:
        return autofill_assistant::password_change::kEnterOldPasswordIcon;
      case TopIcon::TOP_ICON_CHOOSE_NEW_PASSWORD:
        return autofill_assistant::password_change::kChooseNewPasswordIcon;
      case TopIcon::TOP_ICON_SAVE_NEW_PASSWORD:
        return autofill_assistant::password_change::kSaveNewPasswordIcon;
      case TopIcon::TOP_ICON_CHANGED_PASSWORD:
        return autofill_assistant::password_change::kChangedPasswordIcon;
      case TopIcon::TOP_ICON_PASSWORD_RESET_REQUESTED:
        return autofill_assistant::password_change::kPasswordResetRequestedIcon;
      case TopIcon::TOP_ICON_BAD_NEW_PASSWORD:
        return autofill_assistant::password_change::kBadNewPasswordIcon;
      case TopIcon::TOP_ICON_ERROR_OCCURRED:
        return autofill_assistant::password_change::kErrorOccurredIcon;
      case TopIcon::TOP_ICON_USER_ACTION_REQUIRED:
        return autofill_assistant::password_change::kUserActionRequiredIcon;
    }
  } else {
    switch (icon) {
      case TopIcon::TOP_ICON_UNSPECIFIED:
        return autofill_assistant::password_change::kUnspecifiedStateDarkIcon;
      case TopIcon::TOP_ICON_OPEN_SITE_SETTINGS:
        return autofill_assistant::password_change::kOpenSiteSettingsDarkIcon;
      case TopIcon::TOP_ICON_ENTER_OLD_PASSWORD:
        return autofill_assistant::password_change::kEnterOldPasswordDarkIcon;
      case TopIcon::TOP_ICON_CHOOSE_NEW_PASSWORD:
        return autofill_assistant::password_change::kChooseNewPasswordDarkIcon;
      case TopIcon::TOP_ICON_SAVE_NEW_PASSWORD:
        return autofill_assistant::password_change::kSaveNewPasswordDarkIcon;
      case TopIcon::TOP_ICON_CHANGED_PASSWORD:
        return autofill_assistant::password_change::kChangedPasswordDarkIcon;
      case TopIcon::TOP_ICON_PASSWORD_RESET_REQUESTED:
        return autofill_assistant::password_change::
            kPasswordResetRequestedDarkIcon;
      case TopIcon::TOP_ICON_BAD_NEW_PASSWORD:
        return autofill_assistant::password_change::kBadNewPasswordDarkIcon;
      case TopIcon::TOP_ICON_ERROR_OCCURRED:
        return autofill_assistant::password_change::kErrorOccurredDarkIcon;
      case TopIcon::TOP_ICON_USER_ACTION_REQUIRED:
        return autofill_assistant::password_change::kUserActionRequiredDarkIcon;
    }
  }
}
