// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/management_transition_screen_handler.h"

#include "ash/components/arc/session/arc_management_transition.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/management_transition_screen.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

ManagementTransitionScreenHandler::ManagementTransitionScreenHandler()
    : BaseScreenHandler(kScreenId) {}

ManagementTransitionScreenHandler::~ManagementTransitionScreenHandler() =
    default;

void ManagementTransitionScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("addingManagementTitle", IDS_ADDING_MANAGEMENT_TITLE);
  builder->Add("addingManagementTitleUnknownAdmin",
               IDS_ADDING_MANAGEMENT_TITLE_UNKNOWN_ADMIN);
  builder->Add("removingSupervisionTitle", IDS_REMOVING_SUPERVISION_TITLE);
  builder->Add("addingSupervisionTitle", IDS_ADDING_SUPERVISION_TITLE);
  builder->Add("managementTransitionIntroMessage",
               IDS_SUPERVISION_TRANSITION_MESSAGE);
  builder->Add("managementTransitionErrorTitle",
               IDS_SUPERVISION_TRANSITION_ERROR_TITLE);
  builder->Add("managementTransitionErrorMessage",
               IDS_SUPERVISION_TRANSITION_ERROR_MESSAGE);
  builder->Add("managementTransitionErrorButton",
               IDS_SUPERVISION_TRANSITION_ERROR_BUTTON);
}

void ManagementTransitionScreenHandler::Show(
    arc::ArcManagementTransition arc_management_transition,
    std::string management_entity) {
  base::Value::Dict data;
  data.Set("arcTransition", static_cast<int>(arc_management_transition));
  data.Set("managementEntity", management_entity);

  ShowInWebUI(std::move(data));
}

void ManagementTransitionScreenHandler::ShowError() {
  CallExternalAPI("showStep", "error");
}

base::WeakPtr<ManagementTransitionScreenView>
ManagementTransitionScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
