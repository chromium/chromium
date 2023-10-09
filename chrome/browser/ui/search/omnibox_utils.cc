// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/omnibox_utils.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/clipboard_utils.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "content/public/browser/web_contents.h"

namespace search {

OmniboxView* GetOmniboxView(content::WebContents* web_contents) {
  return GetOmniboxView(chrome::FindBrowserWithTab(web_contents));
}

OmniboxView* GetOmniboxView(Browser* browser) {
  if (!browser)
    return nullptr;
  return browser->window()->GetLocationBar()->GetOmniboxView();
}

void FocusOmnibox(bool focus, content::WebContents* web_contents) {
  OmniboxView* omnibox_view = GetOmniboxView(web_contents);
  if (!omnibox_view)
    return;

  if (focus) {
    // This is an invisible focus to support "fakebox" implementations on NTPs
    // (including other search providers). We shouldn't consider it as the user
    // explicitly focusing the omnibox.
    omnibox_view->SetFocus(/*is_user_initiated=*/false);
    omnibox_view->model()->SetCaretVisibility(false);
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
    if (!omnibox_view->model()->PopupIsOpen()) {
      web_contents->Focus();
    }
  }
}

bool IsOmniboxInputInProgress(content::WebContents* web_contents) {
  OmniboxView* omnibox_view = GetOmniboxView(web_contents);
  return omnibox_view && omnibox_view->model()->user_input_in_progress() &&
         omnibox_view->model()->focus_state() == OMNIBOX_FOCUS_VISIBLE;
}

}  // namespace search
