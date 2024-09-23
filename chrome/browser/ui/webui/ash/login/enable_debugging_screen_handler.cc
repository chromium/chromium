// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/enable_debugging_screen_handler.h"

#include <string>

#include "base/logging.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/enable_debugging_screen.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/strings/grit/components_strings.h"

namespace ash {

EnableDebuggingScreenHandler::EnableDebuggingScreenHandler()
    : BaseScreenHandler(kScreenId) {}

EnableDebuggingScreenHandler::~EnableDebuggingScreenHandler() = default;

void EnableDebuggingScreenHandler::Show() {
  DVLOG(1) << "Showing enable debugging screen.";
  ShowInWebUI();
}

base::WeakPtr<EnableDebuggingScreenView>
EnableDebuggingScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void EnableDebuggingScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("enableDebuggingScreenTitle",
               IDS_ENABLE_DEBUGGING_SCREEN_TITLE);
  builder->Add("enableDebuggingScreenAccessibleTitle",
               IDS_ENABLE_DEBUGGING_SCREEN_TITLE);
  builder->Add("enableDebuggingCancelButton", IDS_CANCEL);
  builder->Add("enableDebuggingOKButton", IDS_OK);
  builder->Add("enableDebuggingRemoveButton",
               IDS_ENABLE_DEBUGGING_REMOVE_ROOTFS_BUTTON);
  builder->Add("enableDebuggingEnableButton",
               IDS_ENABLE_DEBUGGING_ENABLE_BUTTON);
  builder->Add("enableDebuggingRemveRootfsMessage",
               IDS_ENABLE_DEBUGGING_SCREEN_ROOTFS_REMOVE_MSG);
  builder->Add("enableDebuggingLearnMore",
               IDS_ENABLE_DEBUGGING_LEARN_MORE);
  builder->Add("enableDebuggingSetupMessage",
               IDS_ENABLE_DEBUGGING_SETUP_MESSAGE);
  builder->AddF("enableDebuggingWarningTitle",
                IDS_ENABLE_DEBUGGING_SCREEN_WARNING_MSG,
                IDS_SHORT_PRODUCT_NAME);
  builder->AddF("enableDebuggingDoneMessage",
                IDS_ENABLE_DEBUGGING_DONE_MESSAGE,
                IDS_SHORT_PRODUCT_NAME);
  builder->Add("enableDebuggingErrorTitle",
                IDS_ENABLE_DEBUGGING_ERROR_TITLE);
  builder->AddF("enableDebuggingErrorMessage",
                IDS_ENABLE_DEBUGGING_ERROR_MESSAGE,
                IDS_SHORT_PRODUCT_NAME);
  builder->Add("enableDebuggingPasswordLabel",
               IDS_ENABLE_DEBUGGING_ROOT_PASSWORD_LABEL);
  builder->Add("enableDebuggingConfirmPasswordLabel",
               IDS_ENABLE_DEBUGGING_CONFIRM_PASSWORD_LABEL);
  builder->Add("enableDebuggingPasswordLengthNote",
               IDS_ENABLE_DEBUGGING_EMPTY_ROOT_PASSWORD_LABEL);
}

// static
void EnableDebuggingScreenHandler::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDebuggingFeaturesRequested, false);
}

void EnableDebuggingScreenHandler::UpdateUIState(UIState state) {
  CallExternalAPI("updateState", static_cast<int>(state));
}

}  // namespace ash
