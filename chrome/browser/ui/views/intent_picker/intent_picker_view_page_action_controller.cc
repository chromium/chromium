// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/intent_picker/intent_picker_view_page_action_controller.h"

#include "base/check_op.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"

IntentPickerViewPageActionController::IntentPickerViewPageActionController(
    tabs::TabInterface& tab_interface)
    : tab_interface_(tab_interface) {
  CHECK(IsPageActionMigrated(PageActionIconType::kIntentPicker));
}

void IntentPickerViewPageActionController::UpdatePageActionVisibility(
    bool should_show_icon) {
  Profile* const profile =
      tab_interface_->GetBrowserWindowInterface()->GetProfile();
  if (profile->IsOffTheRecord()) {
    return;
  }
  page_actions::PageActionController* page_action_controller =
      tab_interface_->GetTabFeatures()->page_action_controller();
  CHECK(page_action_controller);
  if (should_show_icon) {
    page_action_controller->Show(kActionShowIntentPicker);
  } else {
    HideIcon();
  }
}

void IntentPickerViewPageActionController::HideIcon() {
  page_actions::PageActionController* page_action_controller =
      tab_interface_->GetTabFeatures()->page_action_controller();
  CHECK(page_action_controller);
  page_action_controller->Hide(kActionShowIntentPicker);
}
