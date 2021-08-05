// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_H_

#include "base/callback.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Profile;
class NonAccessibleImageView;

namespace views {
class View;
}  // namespace views

namespace gfx {
class ImageSkia;
}

class IncognitoClearBrowsingDataDialog
    : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(IncognitoClearBrowsingDataDialog);

  static void Show(views::View* anchor_view, Profile* incognito_profile);
  static bool IsShowing();

  // testing
  static IncognitoClearBrowsingDataDialog*
  GetIncognitoClearBrowsingDataDialogForTesting();
  void SetDestructorCallbackForTesting(base::OnceClosure callback);

  IncognitoClearBrowsingDataDialog(
      const IncognitoClearBrowsingDataDialog& other) = delete;
  IncognitoClearBrowsingDataDialog& operator=(
      const IncognitoClearBrowsingDataDialog& other) = delete;
  ~IncognitoClearBrowsingDataDialog() override;

  void OnThemeChanged() override;

 private:
  explicit IncognitoClearBrowsingDataDialog(views::View* anchor_view,
                                            Profile* incognito_profile);

  static void CloseDialog();

  gfx::ImageSkia* GetHeaderArt();

  // Helper methods to add functionality to the button.
  void OnCloseWindowsButtonClicked();
  void OnCancelButtonClicked();

  Profile* incognito_profile_;
  NonAccessibleImageView* header_view_;
  base::OnceClosure destructor_callback_ = base::DoNothing();
};

#endif  // CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_H_
