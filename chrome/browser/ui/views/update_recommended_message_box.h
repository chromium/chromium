// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_UPDATE_RECOMMENDED_MESSAGE_BOX_H_
#define CHROME_BROWSER_UI_VIEWS_UPDATE_RECOMMENDED_MESSAGE_BOX_H_

#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class MessageBoxView;
}

// A dialog box that tells the user that an update is recommended in order for
// the latest version to be put to use.
class UpdateRecommendedMessageBox : public views::DialogDelegate {
 public:
  static void Show(gfx::NativeWindow parent_window);

 private:
  UpdateRecommendedMessageBox();
  ~UpdateRecommendedMessageBox() override;

  // Overridden from views::DialogDelegate:
  bool Accept() override;

  // Overridden from views::WidgetDelegate:
  bool ShouldShowWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  base::string16 GetWindowTitle() const override;
  void DeleteDelegate() override;
  ui::ModalType GetModalType() const override;
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  views::MessageBoxView* message_box_view_;

  DISALLOW_COPY_AND_ASSIGN(UpdateRecommendedMessageBox);
};

#endif  // CHROME_BROWSER_UI_VIEWS_UPDATE_RECOMMENDED_MESSAGE_BOX_H_
