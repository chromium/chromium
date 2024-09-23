// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function.h"
#include "ui/base/base_window.h"
#include "ui/views/widget/widget.h"

namespace windows_util {

// Views implementation of CalledFromChildWindow.
bool CalledFromChildWindow(ExtensionFunction* function,
                           const extensions::WindowController* controller) {
  // We can not directly compare window parents here. Extension function calls
  // can originate from bubbles, which are not physically parented to the
  // browser window (by design, e.g. extension popups or any extension context
  // shown inside a BubbleDialogDelegateView). However, the widget of the bubble
  // window is a child of the browser window's widget. So we check if the
  // sender's primary widget is the same as the current window's widget.
  content::WebContents* sender_web_contents = function->GetSenderWebContents();
  views::Widget* sender_widget =
      sender_web_contents ? views::Widget::GetWidgetForNativeWindow(
                                sender_web_contents->GetTopLevelNativeWindow())
                          : nullptr;
  views::Widget* current_window_widget =
      views::Widget::GetWidgetForNativeWindow(
          controller->window()->GetNativeWindow());
  return sender_widget && current_window_widget &&
         sender_widget->GetPrimaryWindowWidget() == current_window_widget;
}

}  // namespace windows_util
