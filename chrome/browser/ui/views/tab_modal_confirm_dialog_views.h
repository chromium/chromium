// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_MODAL_CONFIRM_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_MODAL_CONFIRM_DIALOG_VIEWS_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}

namespace views {
class MessageBoxView;
class Widget;
}

// Displays a tab-modal dialog, i.e. a dialog that will block the current page
// but still allow the user to switch to a different page.
// To display the dialog, allocate this object on the heap. It will open the
// dialog from its constructor and then delete itself when the user dismisses
// the dialog.
class TabModalConfirmDialogViews : public TabModalConfirmDialog,
                                   public views::DialogDelegate,
                                   public views::LinkListener {
 public:
  TabModalConfirmDialogViews(
      std::unique_ptr<TabModalConfirmDialogDelegate> delegate,
      content::WebContents* web_contents);

  // views::DialogDelegate:
  int GetDialogButtons() const override;
  base::string16 GetWindowTitle() const override;
  bool Cancel() override;
  bool Accept() override;
  bool Close() override;
  bool ShouldShowCloseButton() const override;

  // views::WidgetDelegate:
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  void DeleteDelegate() override;
  ui::ModalType GetModalType() const override;

 private:
  ~TabModalConfirmDialogViews() override;

  // TabModalConfirmDialog:
  void AcceptTabModalDialog() override;
  void CancelTabModalDialog() override;

  // TabModalConfirmDialogCloseDelegate:
  void CloseDialog() override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  views::View* GetInitiallyFocusedView() override;

  std::unique_ptr<TabModalConfirmDialogDelegate> delegate_;

  // The message box view whose commands we handle.
  views::MessageBoxView* message_box_view_;

  DISALLOW_COPY_AND_ASSIGN(TabModalConfirmDialogViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_MODAL_CONFIRM_DIALOG_VIEWS_H_
