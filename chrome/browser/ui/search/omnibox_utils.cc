// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/omnibox_utils.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/clipboard_utils.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/browser/web_contents.h"

namespace {

OmniboxView* GetOmniboxView(content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return nullptr;
  return browser->window()->GetLocationBar()->GetOmniboxView();
}

}  // namespace

namespace search {

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
    if (!omnibox_view->model()->popup_model()->IsOpen())
      web_contents->Focus();
  }
}

void PasteIntoOmnibox(const std::u16string& text,
                      content::WebContents* web_contents) {
  OmniboxView* omnibox_view = GetOmniboxView(web_contents);
  if (!omnibox_view)
    return;
  // The first case is for right click to paste, where the text is retrieved
  // from the clipboard already sanitized. The second case is needed to handle
  // drag-and-drop value and it has to be sanitazed before setting it into the
  // omnibox.
  std::u16string text_to_paste =
      text.empty() ? GetClipboardText(/*notify_if_restricted=*/true)
                   : omnibox_view->SanitizeTextForPaste(text);

  if (text_to_paste.empty())
    return;

  if (!omnibox_view->model()->has_focus()) {
    // Pasting into a "realbox" should not be considered the user explicitly
    // focusing the omnibox.
    omnibox_view->SetFocus(/*is_user_initiated=*/false);
  }

  omnibox_view->OnBeforePossibleChange();
  omnibox_view->model()->OnPaste();
  omnibox_view->SetUserText(text_to_paste);
  omnibox_view->OnAfterPossibleChange(true);
}

bool IsOmniboxInputInProgress(content::WebContents* web_contents) {
  OmniboxView* omnibox_view = GetOmniboxView(web_contents);
  return omnibox_view && omnibox_view->model()->user_input_in_progress() &&
         omnibox_view->model()->focus_state() == OMNIBOX_FOCUS_VISIBLE;
}

}  // namespace search
