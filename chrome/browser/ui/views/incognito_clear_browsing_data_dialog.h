// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/incognito_clear_browsing_data_dialog_interface.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Profile;

namespace views {
class View;
}  // namespace views

class IncognitoClearBrowsingDataDialog
    : public IncognitoClearBrowsingDataDialogInterface,
      public views::BubbleDialogDelegateView {
  METADATA_HEADER(IncognitoClearBrowsingDataDialog,
                  views::BubbleDialogDelegateView)

 public:
  IncognitoClearBrowsingDataDialog(views::View* anchor_view,
                                   Profile* incognito_profile,
                                   Type type);
  IncognitoClearBrowsingDataDialog(
      const IncognitoClearBrowsingDataDialog& other) = delete;
  IncognitoClearBrowsingDataDialog& operator=(
      const IncognitoClearBrowsingDataDialog& other) = delete;
  ~IncognitoClearBrowsingDataDialog() override = default;

 private:
  static void CloseDialog();

  // Helper methods to add functionality to the button.
  void OnCloseWindowsButtonClicked() override;
  void OnCancelButtonClicked() override;

  // Helper methods to decorate the dialog for different dialog type.
  void SetDialogForDefaultBubbleType();
  void SetDialogForHistoryDisclaimerBubbleType();

  const Type dialog_type_;
  raw_ptr<Profile> incognito_profile_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_H_
