// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_CHOOSER_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_CHOOSER_DIALOG_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/window/dialog_delegate.h"

class ChooserController;
class DeviceChooserContentView;

// Displays a chooser view as a modal dialog constrained
// to the window/tab displaying the given web contents.
class ChooserDialogView : public views::DialogDelegateView,
                          public views::TableViewObserver {
 public:
  explicit ChooserDialogView(
      std::unique_ptr<ChooserController> chooser_controller);
  ~ChooserDialogView() override;

  // views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  ui::ModalType GetModalType() const override;

  // views::DialogDelegate:
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  views::View* GetInitiallyFocusedView() override;
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;

  // views::DialogDelegateView:
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  // views::TableViewObserver:
  void OnSelectionChanged() override;

  DeviceChooserContentView* device_chooser_content_view_for_test() const;

 private:
  DeviceChooserContentView* device_chooser_content_view_;

  DISALLOW_COPY_AND_ASSIGN(ChooserDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_CHOOSER_DIALOG_VIEW_H_
