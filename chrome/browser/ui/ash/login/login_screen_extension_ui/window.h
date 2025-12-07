// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_SCREEN_EXTENSION_UI_WINDOW_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_SCREEN_EXTENSION_UI_WINDOW_H_

#include <memory>

#include "base/memory/raw_ptr.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {
namespace login_screen_extension_ui {
struct CreateOptions;
class DialogDelegate;
class LoginWebView;

// This class represents the window that can be created by the
// chrome.loginScreenUi API. It manages the window's widget, view and delegate,
// which are all automatically deleted when the widget closes.
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
  raw_ptr<DialogDelegate> dialog_delegate_ = nullptr;
  raw_ptr<LoginWebView> dialog_view_ = nullptr;
  raw_ptr<views::Widget> dialog_widget_ = nullptr;
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
