// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_UI_ACTION_LOGGER_H_
#define CHROME_BROWSER_UI_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_UI_ACTION_LOGGER_H_

namespace translate {

// Histogram for recording the UI events related to the Partial Translate
// bubble.
constexpr char kPartialTranslateBubbleUiEventHistogramName[] =
    "Translate.PartialTranslateBubbleUiEvent";

enum class PartialTranslateBubbleUiEvent {
  // Update PartialTranslateBubbleUiEvent in enums.xml when making changes.
  // The Partial Translate bubble was shown to the user.
  BUBBLE_SHOWN = 0,

  // The user clicked the target language tab to start a translation.
  TARGET_LANGUAGE_TAB_SELECTED = 1,

  // The user clicked the source language tab to revert the translation.
  SOURCE_LANGUAGE_TAB_SELECTED = 2,

  // The user clicked the “Translate Full Page” button.
  TRANSLATE_FULL_PAGE_BUTTON_CLICKED = 3,

  // The user clicked the "Close" [X] button.
  CLOSE_BUTTON_CLICKED = 4,

  // The user clicked the "Try Again" button from the error view.
  TRY_AGAIN_BUTTON_CLICKED = 5,

  // The user clicked the “Page is Not in {Source Language}” item in the options
  // menu.
  CHANGE_SOURCE_LANGUAGE_OPTION_CLICKED = 6,

  // The user clicked the “Choose Another Language” item in the options menu.
  CHANGE_TARGET_LANGUAGE_OPTION_CLICKED = 7,

  // The user made a selection in the source language combobox.
  SOURCE_LANGUAGE_MENU_ITEM_CLICKED = 8,

  // The user made a selection in the target language combobox.
  TARGET_LANGUAGE_MENU_ITEM_CLICKED = 9,

  // The user clicked the "Reset" button from the source language selection
  // view.
  SOURCE_LANGUAGE_RESET_BUTTON_CLICKED = 10,

  // The user clicked the "Reset" button from the target language selection
  // view.
  TARGET_LANGUAGE_RESET_BUTTON_CLICKED = 11,

  // The user clicked the "Done" button from the source language
  // selection view.
  SOURCE_LANGUAGE_SELECTION_DONE_BUTTON_CLICKED = 12,

  // The user clicked the "Translate" button from the source language
  // selection view.
  SOURCE_LANGUAGE_SELECTION_TRANSLATE_BUTTON_CLICKED = 13,

  // The user clicked the "Done" button from the target language
  // selection view.
  TARGET_LANGUAGE_SELECTION_DONE_BUTTON_CLICKED = 14,

  // The user clicked the "Translate" button from the target language
  // selection view.
  TARGET_LANGUAGE_SELECTION_TRANSLATE_BUTTON_CLICKED = 15,

  kMaxValue = TARGET_LANGUAGE_SELECTION_TRANSLATE_BUTTON_CLICKED
};

// Logs metrics for the user's PartialTranslateBubbleUiEvent |action|.
void ReportPartialTranslateBubbleUiAction(
    translate::PartialTranslateBubbleUiEvent action);

}  // namespace translate

#endif  // CHROME_BROWSER_UI_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_UI_ACTION_LOGGER_H_
