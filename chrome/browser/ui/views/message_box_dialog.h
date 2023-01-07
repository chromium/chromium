// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MESSAGE_BOX_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_MESSAGE_BOX_DIALOG_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/simple_message_box.h"

#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class MessageBoxView;
}

class MessageBoxDialog : public views::DialogDelegate,
                         public views::WidgetObserver {
 public:
  using MessageBoxResultCallback =
      base::OnceCallback<void(chrome::MessageBoxResult result)>;

  MessageBoxDialog(const MessageBoxDialog&) = delete;
  MessageBoxDialog& operator=(const MessageBoxDialog&) = delete;

  static chrome::MessageBoxResult Show(
      gfx::NativeWindow parent,
      const std::u16string& title,
      const std::u16string& message,
      chrome::MessageBoxType type,
      const std::u16string& yes_text,
      const std::u16string& no_text,
      const std::u16string& checkbox_text,
      MessageBoxResultCallback callback = MessageBoxResultCallback());

  // views::DialogDelegate:
  std::u16string GetWindowTitle() const override;
  views::View* GetContentsView() override;
  bool ShouldShowCloseButton() const override;

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

 private:
  MessageBoxDialog(const std::u16string& title,
                   const std::u16string& message,
                   chrome::MessageBoxType type,
                   const std::u16string& yes_text,
                   const std::u16string& no_text,
                   const std::u16string& checkbox_text,
                   bool is_system_modal);
  ~MessageBoxDialog() override;

  void Run(MessageBoxResultCallback result_callback);
  void Done(chrome::MessageBoxResult result);

  void OnDialogAccepted();

  // Widget:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  const std::u16string window_title_;
  const chrome::MessageBoxType type_;
  raw_ptr<views::MessageBoxView> message_box_view_;
  MessageBoxResultCallback result_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MESSAGE_BOX_DIALOG_H_
