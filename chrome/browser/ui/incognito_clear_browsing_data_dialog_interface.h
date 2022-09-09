// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_INTERFACE_H_
#define CHROME_BROWSER_UI_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_INTERFACE_H_

// Abstract class which facilitate communication between clients in
// chrome/browser/ui/* and its implementation IncognitoClearBrowsingDataDialog
// in chrome/browser/ui/views/.
class IncognitoClearBrowsingDataDialogInterface {
 public:
  enum Type {
    kDefaultBubble = 0,
    kHistoryDisclaimerBubble = 1,
    kMaxValue = kHistoryDisclaimerBubble,
  };

  // Represents the action type that the user can take in the dialog.
  // Do not reorder items here because it's mirrored to UMA as
  // IncognitoClearBrowsingDataDialogActionType. Values should be enumerated
  // from 0. When removing items, comment them out and keep the existing numeric
  // values stable. Don't forget to also add any new entries to the enums.xml.
  enum class DialogActionType {
    kCancel = 0,
    kCloseIncognito = 1,
    kMaxValue = kCloseIncognito,
  };

 protected:
  // A method that is called by the UI implementation body when the user
  // interacts with the Close option of the Incognito clear browsing data
  // dialog.
  virtual void OnCloseWindowsButtonClicked() = 0;
  // A method that is called by the UI implementation body when the user
  // interacts with the Cancel option of the Incognito clear browsing data
  // dialog.
  virtual void OnCancelButtonClicked() = 0;
};

#endif  // CHROME_BROWSER_UI_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_INTERFACE_H_
