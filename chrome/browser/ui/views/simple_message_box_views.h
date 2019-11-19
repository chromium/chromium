// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIMPLE_MESSAGE_BOX_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_SIMPLE_MESSAGE_BOX_VIEWS_H_

#include "chrome/browser/ui/simple_message_box.h"

#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

class SimpleMessageBoxViews : public views::DialogDelegate,
                              public views::WidgetObserver {
 public:
  using MessageBoxResultCallback =
      base::OnceCallback<void(chrome::MessageBoxResult result)>;

  static chrome::MessageBoxResult Show(
      gfx::NativeWindow parent,
      const base::string16& title,
      const base::string16& message,
      chrome::MessageBoxType type,
      const base::string16& yes_text,
      const base::string16& no_text,
      const base::string16& checkbox_text,
      bool can_close,
      MessageBoxResultCallback callback = MessageBoxResultCallback());

  // views::DialogDelegate:
  int GetDialogButtons() const override;
  bool Cancel() override;
  bool Accept() override;
  bool Close() override;
  base::string16 GetWindowTitle() const override;
  void DeleteDelegate() override;
  ui::ModalType GetModalType() const override;
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  bool ShouldShowCloseButton() const override;

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

 private:
  SimpleMessageBoxViews(const base::string16& title,
                        const base::string16& message,
                        chrome::MessageBoxType type,
                        const base::string16& yes_text,
                        const base::string16& no_text,
                        const base::string16& checkbox_text,
                        bool is_system_modal,
                        bool can_close);
  ~SimpleMessageBoxViews() override;

  void Run(MessageBoxResultCallback result_callback);
  void Done();

  const base::string16 window_title_;
  const chrome::MessageBoxType type_;
  chrome::MessageBoxResult result_;
  views::MessageBoxView* message_box_view_;
  MessageBoxResultCallback result_callback_;
  bool is_system_modal_;
  bool can_close_;

  DISALLOW_COPY_AND_ASSIGN(SimpleMessageBoxViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIMPLE_MESSAGE_BOX_VIEWS_H_
