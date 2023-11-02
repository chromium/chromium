// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill_assistant/password_change/apc_utils.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/autofill_assistant/password_change/vector_icons/vector_icons.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ApcUtilsTest, GetAssistantIconOrFallback) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(&GetAssistantIconOrFallback(), &vector_icons::kAssistantIcon);
#else
  EXPECT_EQ(&GetAssistantIconOrFallback(), &vector_icons::kProductIcon);
#endif
}

TEST(ApcUtilsTest, GetApcTopIconFromEnum) {
  using autofill_assistant::password_change::TopIcon;

  // Selected light mode tests.
  EXPECT_EQ(&GetApcTopIconFromEnum(TopIcon::TOP_ICON_OPEN_SITE_SETTINGS, false),
            &autofill_assistant::password_change::kOpenSiteSettingsIcon);
  EXPECT_EQ(&GetApcTopIconFromEnum(TopIcon::TOP_ICON_ENTER_OLD_PASSWORD, false),
            &autofill_assistant::password_change::kEnterOldPasswordIcon);
  EXPECT_EQ(
      &GetApcTopIconFromEnum(TopIcon::TOP_ICON_CHOOSE_NEW_PASSWORD, false),
      &autofill_assistant::password_change::kChooseNewPasswordIcon);
  EXPECT_EQ(&GetApcTopIconFromEnum(TopIcon::TOP_ICON_SAVE_NEW_PASSWORD, false),
            &autofill_assistant::password_change::kSaveNewPasswordIcon);
  EXPECT_EQ(&GetApcTopIconFromEnum(TopIcon::TOP_ICON_CHANGED_PASSWORD, false),
            &autofill_assistant::password_change::kChangedPasswordIcon);
  EXPECT_EQ(
      &GetApcTopIconFromEnum(TopIcon::TOP_ICON_PASSWORD_RESET_REQUESTED, false),
      &autofill_assistant::password_change::kPasswordResetRequestedIcon);
  EXPECT_EQ(&GetApcTopIconFromEnum(TopIcon::TOP_ICON_BAD_NEW_PASSWORD, false),
            &autofill_assistant::password_change::kBadNewPasswordIcon);
  EXPECT_EQ(&GetApcTopIconFromEnum(TopIcon::TOP_ICON_ERROR_OCCURRED, false),
            &autofill_assistant::password_change::kErrorOccurredIcon);
  EXPECT_EQ(
      &GetApcTopIconFromEnum(TopIcon::TOP_ICON_USER_ACTION_REQUIRED, false),
      &autofill_assistant::password_change::kUserActionRequiredIcon);

  // Selected dark mode tests.
  EXPECT_EQ(&GetApcTopIconFromEnum(TopIcon::TOP_ICON_OPEN_SITE_SETTINGS, true),
            &autofill_assistant::password_change::kOpenSiteSettingsDarkIcon);
  EXPECT_EQ(&GetApcTopIconFromEnum(TopIcon::TOP_ICON_ENTER_OLD_PASSWORD, true),
            &autofill_assistant::password_change::kEnterOldPasswordDarkIcon);
  EXPECT_EQ(&GetApcTopIconFromEnum(TopIcon::TOP_ICON_CHOOSE_NEW_PASSWORD, true),
            &autofill_assistant::password_change::kChooseNewPasswordDarkIcon);
  EXPECT_EQ(&GetApcTopIconFromEnum(TopIcon::TOP_ICON_SAVE_NEW_PASSWORD, true),
            &autofill_assistant::password_change::kSaveNewPasswordDarkIcon);
  EXPECT_EQ(&GetApcTopIconFromEnum(TopIcon::TOP_ICON_CHANGED_PASSWORD, true),
            &autofill_assistant::password_change::kChangedPasswordDarkIcon);
  EXPECT_EQ(
      &GetApcTopIconFromEnum(TopIcon::TOP_ICON_PASSWORD_RESET_REQUESTED, true),
      &autofill_assistant::password_change::kPasswordResetRequestedDarkIcon);
  EXPECT_EQ(&GetApcTopIconFromEnum(TopIcon::TOP_ICON_BAD_NEW_PASSWORD, true),
            &autofill_assistant::password_change::kBadNewPasswordDarkIcon);
  EXPECT_EQ(&GetApcTopIconFromEnum(TopIcon::TOP_ICON_ERROR_OCCURRED, true),
            &autofill_assistant::password_change::kErrorOccurredDarkIcon);
  EXPECT_EQ(
      &GetApcTopIconFromEnum(TopIcon::TOP_ICON_USER_ACTION_REQUIRED, true),
      &autofill_assistant::password_change::kUserActionRequiredDarkIcon);
}
