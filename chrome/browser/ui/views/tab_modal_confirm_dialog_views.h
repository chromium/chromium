// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_MODAL_CONFIRM_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_MODAL_CONFIRM_DIALOG_VIEWS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "ui/gfx/native_widget_types.h"
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
                                   public views::DialogDelegate {
 public:
  TabModalConfirmDialogViews(
      std::unique_ptr<TabModalConfirmDialogDelegate> delegate,
      content::WebContents* web_contents);

  TabModalConfirmDialogViews(const TabModalConfirmDialogViews&) = delete;
  TabModalConfirmDialogViews& operator=(const TabModalConfirmDialogViews&) =
      delete;

  // views::DialogDelegate:
  std::u16string GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

 private:
  ~TabModalConfirmDialogViews() override;

  // TabModalConfirmDialog:
  void AcceptTabModalDialog() override;
  void CancelTabModalDialog() override;
  void CloseDialog() override;

  void LinkClicked(const ui::Event& event);

  views::View* GetInitiallyFocusedView() override;

  std::unique_ptr<TabModalConfirmDialogDelegate> delegate_;

  // The message box view whose commands we handle.
  raw_ptr<views::MessageBoxView> message_box_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_MODAL_CONFIRM_DIALOG_VIEWS_H_
