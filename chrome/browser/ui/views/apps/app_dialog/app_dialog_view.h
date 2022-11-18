// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_DIALOG_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"

namespace ui {
class ImageModel;
}

// The app dialog that may display the app's name, icon. This is the base class
// for app related dialog classes, e.g AppBlockDialogView, AppPauseDialogView.
class AppDialogView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(AppDialogView);
  explicit AppDialogView(const ui::ImageModel& image);
  ~AppDialogView() override;

 protected:
  void InitializeView(const std::u16string& heading_text);

  // Can only be called after InitializeView().
  void SetLabelText(const std::u16string& text);

 private:
  raw_ptr<views::Label> label_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_DIALOG_VIEW_H_
