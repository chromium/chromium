// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_SCREEN_EXTENSION_UI_WINDOW_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_SCREEN_EXTENSION_UI_WINDOW_H_

#include <memory>

namespace views {
class Widget;
}  // namespace views

namespace ash {
namespace login_screen_extension_ui {
struct CreateOptions;
class DialogDelegate;

// This class represents the window that can be created by the
// chrome.loginScreenUi API. It owns the window's widget and delegate, and
// closes the widget when it is destroyed.
// The window is not draggable, and has a close button which is not visible
// if `create_options.can_be_closed_by_user` is false.
class Window {
 public:
  explicit Window(CreateOptions* create_options);

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  ~Window();

  DialogDelegate* GetDialogDelegateForTesting();
  views::Widget* GetDialogWidgetForTesting();

 private:
  // Must outlive `dialog_widget_`, whose view points to it.
  std::unique_ptr<DialogDelegate> dialog_delegate_;
  std::unique_ptr<views::Widget> dialog_widget_;
};

class WindowFactory {
 public:
  WindowFactory();

  WindowFactory(const WindowFactory&) = delete;
  WindowFactory& operator=(const WindowFactory&) = delete;

  virtual ~WindowFactory();

  virtual std::unique_ptr<Window> Create(CreateOptions* create_options);
};

}  // namespace login_screen_extension_ui
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_SCREEN_EXTENSION_UI_WINDOW_H_
