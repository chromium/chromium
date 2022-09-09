// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill_assistant/password_change/apc_utils.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/autofill_assistant/password_change/vector_icons/vector_icons.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "components/vector_icons/vector_icons.h"

const gfx::VectorIcon& GetAssistantIconOrFallback() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return vector_icons::kAssistantIcon;
#else
  // Only developer builds will ever use this branch.
  return vector_icons::kProductIcon;
#endif
}

const gfx::VectorIcon& GetApcTopIconFromEnum(
    autofill_assistant::password_change::TopIcon icon) {
  switch (icon) {
    case autofill_assistant::password_change::TopIcon::TOP_ICON_UNSPECIFIED:
      return autofill_assistant::password_change::kUnspecifiedStateIcon;
    case autofill_assistant::password_change::TopIcon::
        TOP_ICON_OPEN_SITE_SETTINGS:
      return autofill_assistant::password_change::kOpenSiteSettingsIcon;
    case autofill_assistant::password_change::TopIcon::
        TOP_ICON_ENTER_OLD_PASSWORD:
      return autofill_assistant::password_change::kEnterOldPasswordIcon;
    case autofill_assistant::password_change::TopIcon::
        TOP_ICON_CHOOSE_NEW_PASSWORD:
      return autofill_assistant::password_change::kChooseNewPasswordIcon;
    case autofill_assistant::password_change::TopIcon::
        TOP_ICON_SAVE_NEW_PASSWORD:
      return autofill_assistant::password_change::kSaveNewPasswordIcon;
    case autofill_assistant::password_change::TOP_ICON_CHANGED_PASSWORD:
      return autofill_assistant::password_change::kChangedPasswordIcon;
    case autofill_assistant::password_change::TOP_ICON_BAD_NEW_PASSWORD:
      return autofill_assistant::password_change::kBadNewPasswordIcon;
    case autofill_assistant::password_change::TOP_ICON_ERROR_OCCURRED:
      return autofill_assistant::password_change::kErrorOccurredIcon;
    case autofill_assistant::password_change::TOP_ICON_USER_ACTION_REQUIRED:
      return autofill_assistant::password_change::kUserActionRequiredIcon;
  }

  return autofill_assistant::password_change::kUnspecifiedStateIcon;
}
