// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/omnibox_utils.h"

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace search {

namespace {

LocationBar* GetLocationBar(content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  const auto* tab = tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab) {
    return nullptr;
  }

  auto* bwi =
      const_cast<BrowserWindowInterface*>(tab->GetBrowserWindowInterface());
  if (!bwi) {
    return nullptr;
  }

  return bwi->GetFeatures().location_bar();
}

OmniboxView* GetOmniboxView(content::WebContents* web_contents) {
  auto* location_bar = GetLocationBar(web_contents);
  if (!location_bar) {
    return nullptr;
  }

  return location_bar->GetOmniboxView();
}

OmniboxEditModel* GetOmniboxEditModel(content::WebContents* web_contents) {
  auto* controller = GetOmniboxController(web_contents);
  if (!controller) {
    return nullptr;
  }

  return controller->edit_model();
}

}  // namespace

OmniboxController* GetOmniboxController(content::WebContents* web_contents) {
  auto* location_bar = GetLocationBar(web_contents);
  if (!location_bar) {
    return nullptr;
  }

  return location_bar->GetOmniboxController();
}

void FocusOmnibox(bool focus, content::WebContents* web_contents) {
  auto* omnibox_view = GetOmniboxView(web_contents);
  if (!omnibox_view) {
    return;
  }
  auto* controller = GetOmniboxController(web_contents);
  if (!controller) {
    return;
  }
  auto* edit_model = controller->edit_model();

  if (focus) {
    // This is an invisible focus to support "fakebox" implementations on NTPs
    // (including other search providers). We shouldn't consider it as the user
    // explicitly focusing the omnibox.
    omnibox_view->SetFocus(/*is_user_initiated=*/false);
    edit_model->SetCaretVisibility(false);
    // If the user clicked on the fakebox, any text already in the omnibox
    // should get cleared when they start typing. Selecting all the existing
    // text is a convenient way to accomplish this. It also gives a slight
    // visual cue to users who really understand selection state about what
    // will happen if they start typing.
    omnibox_view->SelectAll(false);
    omnibox_view->ShowVirtualKeyboardIfEnabled();
  } else {
    // Remove focus only if the popup is closed. This will prevent someone
    // from changing the omnibox value and closing the popup without user
    // interaction.
    if (!controller->IsPopupOpen()) {
      web_contents->Focus();
    }
  }
}

bool IsOmniboxInputInProgress(content::WebContents* web_contents) {
  auto* edit_model = GetOmniboxEditModel(web_contents);
  if (!edit_model) {
    return false;
  }
  return edit_model->user_input_in_progress() &&
         edit_model->focus_state() == OMNIBOX_FOCUS_VISIBLE;
}

}  // namespace search
