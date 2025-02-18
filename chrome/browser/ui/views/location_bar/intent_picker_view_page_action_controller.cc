// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/intent_picker_view_page_action_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"

namespace content {
class WebContents;
}

IntentPickerViewPageActionController::IntentPickerViewPageActionController(
    tabs::TabInterface* tab_interface)
    : tab_interface_(*tab_interface) {}
// TODO(396720194): Add a check to ensure the tab_interface ptr is not nullptr.
void IntentPickerViewPageActionController::UpdatePageActionVisibility(
    bool should_show_icon) {
  if (!base::FeatureList::IsEnabled(features::kPageActionsMigration)) {
    return;
  }
  content::WebContents* const web_contents = tab_interface_->GetContents();
  if (!web_contents) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->IsOffTheRecord()) {
    return;
  }
  page_actions::PageActionController* page_action_controller =
      tab_interface_->GetTabFeatures()->page_action_controller();
  CHECK(page_action_controller);
  if (should_show_icon) {
    page_action_controller->Show(kActionShowIntentPicker);
    page_action_controller->ShowSuggestionChip(kActionShowIntentPicker);
  } else {
    HideIcon();
  }
}

void IntentPickerViewPageActionController::HideIcon() {
  page_actions::PageActionController* page_action_controller =
      tab_interface_->GetTabFeatures()->page_action_controller();
  CHECK(page_action_controller);
  page_action_controller->Hide(kActionShowIntentPicker);
  if (IntentPickerBubbleView* bubble_controller =
          IntentPickerBubbleView::intent_picker_bubble()) {
    bubble_controller->CloseCurrentBubble();
  }
}
