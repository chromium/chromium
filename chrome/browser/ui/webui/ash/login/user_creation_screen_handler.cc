// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"

#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

UserCreationScreenHandler::UserCreationScreenHandler()
    : BaseScreenHandler(kScreenId) {}

UserCreationScreenHandler::~UserCreationScreenHandler() = default;

void UserCreationScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->AddF("userCreationTitle", IDS_OOBE_USER_CREATION_TITLE,
                ui::GetChromeOSDeviceName());
  builder->Add("userCreationSubtitle", IDS_OOBE_USER_CREATION_SUBTITLE);
  builder->AddF("userCreationUpdatedTitle", IDS_OOBE_USER_CREATION_NEW_TITLE,
                ui::GetChromeOSDeviceName());
  builder->Add("userCreationUpdatedSubtitle",
               IDS_OOBE_USER_CREATION_NEW_SUBTITLE);
  builder->AddF("userCreationAddPersonTitle",
                IDS_OOBE_USER_CREATION_ADD_PERSON_TITLE,
                ui::GetChromeOSDeviceName());
  builder->Add("userCreationAddPersonSubtitle",
               IDS_OOBE_USER_CREATION_ADD_PERSON_SUBTITLE);
  builder->Add("userCreationAddPersonUpdatedTitle",
               IDS_OOBE_USER_CREATION_ADD_PERSON_NEW_TITLE);
  builder->Add("userCreationAddPersonUpdatedSubtitle",
               IDS_OOBE_USER_CREATION_ADD_PERSON_NEW_SUBTITLE);
  builder->Add("createForSelfLabel", IDS_OOBE_USER_CREATION_SELF_BUTTON_LABEL);
  builder->Add("createForSelfDescription",
               IDS_OOBE_USER_CREATION_SELF_BUTTON_DESCRIPTION);
  builder->Add("createForChildLabel",
               IDS_OOBE_USER_CREATION_CHILD_BUTTON_LABEL);
  builder->Add("createForChildDescription",
               IDS_OOBE_USER_CREATION_CHILD_BUTTON_DESCRIPTION);

  builder->Add("userCreationPersonalButtonTitle",
               IDS_OOBE_USER_CREATION_PERSONEL_USE_BUTTON_LABEL);
  builder->Add("userCreationPersonalButtonDescription",
               IDS_OOBE_USER_CREATION_PERSONEL_USE_BUTTON_DESCRIPTION);
  builder->Add("userCreationChildButtonTitle",
               IDS_OOBE_USER_CREATION_CHILD_USE_BUTTON_LABEL);
  builder->Add("userCreationChildButtonDescription",
               IDS_OOBE_USER_CREATION_CHILD_USE_BUTTON_DESCRIPTION);
  builder->Add("userCreationEnrollButtonTitle",
               IDS_OOBE_USER_CREATION_ENROLL_USE_BUTTON_LABEL);
  builder->Add("userCreationEnrollButtonDescription",
               IDS_OOBE_USER_CREATION_ENROLL_USE_BUTTON_DESCRIPTION);
  builder->Add("userCreationEnrollLearnMore",
               IDS_OOBE_USER_CREATION_ENROLL_LEARN_MORE);
  builder->Add("userCreationLearnMoreAria",
               IDS_OOBE_USER_CREATION_ENROLL_LEARN_MORE_ARIA);

  // Enrollment Triage Strings
  builder->Add("userCreationEnrollTriageTitle",
               IDS_OOBE_USER_CREATION_ENROLL_TRIAGE_TITLE);
  builder->Add("userCreationEnrollTriageSubtitle",
               IDS_OOBE_USER_CREATION_ENROLL_SUBTITLE);
  builder->Add("userCreationEnrollTriageDescriptionTitle",
               IDS_OOBE_USER_CREATION_ENROLL_ADDITIONAL_DESCRIPTION_TITLE);
  builder->Add("userCreationEnrollTriageDescription",
               IDS_OOBE_USER_CREATION_ENROLL_ADDITIONAL_DESCRIPTION);
  builder->Add("userCreationEnrollTriageAcceptEnrollButtonLabel",
               IDS_OOBE_USER_CREATION_ENROLL_TRIAGE_ACCEPT_ENROLL_BUTTON_LABEL);
  builder->Add(
      "userCreationEnrollTriageDeclineEnrollButtonLabel",
      IDS_OOBE_USER_CREATION_ENROLL_TRIAGE_DECLINE_ENROLL_BUTTON_LABEL);

  // Child Setup Strings
  builder->Add("userCreationChildSetupTitle",
               IDS_OOBE_USER_CREATION_CHILD_SETUP_TITLE);
  builder->Add("userCreationChildSetupChildAccountButtonText",
               IDS_OOBE_USER_CREATION_CHILD_SETUP_CHILD_ACCOUNT_BUTTON_LABEL);
  builder->Add(
      "userCreationChildSetupChildAccountButtonLabel",
      IDS_OOBE_USER_CREATION_CHILD_SETUP_CHILD_ACCOUNT_BUTTON_DESCRIPTION);
  builder->Add("userCreationChildSetupSchoolAccountButtonText",
               IDS_OOBE_USER_CREATION_CHILD_SETUP_SCHOOL_ACCOUNT_BUTTON_LABEL);
  builder->Add(
      "userCreationChildSetupSchoolAccountButtonLabel",
      IDS_OOBE_USER_CREATION_CHILD_SETUP_SCHOOL_ACCOUNT_BUTTON_DESCRIPTION);

  // Enroll Triage Learn more
  builder->Add("userCreationEnrollLearnMoreTitle",
               IDS_OOBE_USER_CREATION_ENROLL_LEARN_MORE_TITLE);
  builder->Add("userCreationEnrollLearnMoreText",
               IDS_OOBE_USER_CREATION_ENROLL_LEARN_MORE_TEXT);
}

void UserCreationScreenHandler::Show() {
  ShowInWebUI();
}

void UserCreationScreenHandler::SetDefaultStep() {
  CallExternalAPI("setDefaultStep");
}

void UserCreationScreenHandler::SetTriageStep() {
  CallExternalAPI("setTriageStep");
}

void UserCreationScreenHandler::SetChildSetupStep() {
  CallExternalAPI("setChildSetupStep");
}

base::WeakPtr<UserCreationView> UserCreationScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void UserCreationScreenHandler::SetIsBackButtonVisible(bool value) {
  CallExternalAPI("setIsBackButtonVisible", value);
}

}  // namespace ash
