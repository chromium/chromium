// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_INPUT_METHOD_OBSERVER_H_
#define CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_INPUT_METHOD_OBSERVER_H_

#include "chromecast/browser/webview/webview_controller.h"
#include "ui/base/ime/input_method_observer.h"

namespace chromecast {

// Used to watch for text field input focus changes and notify the client
// accordingly.
class WebviewInputMethodObserver : public ui::InputMethodObserver {
 public:
  WebviewInputMethodObserver(chromecast::WebContentController* controller,
                             ui::InputMethod* input_method);
  ~WebviewInputMethodObserver() override;

  WebviewInputMethodObserver(const WebviewInputMethodObserver&) = delete;
  WebviewInputMethodObserver& operator=(const WebviewInputMethodObserver&) =
      delete;

  // ui::InputMethodObserver
  void OnFocus() override {}
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override;
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override;
  void OnVirtualKeyboardVisibilityChangedIfEnabled(bool should_show) override;

 private:
  chromecast::WebContentController* controller_;
  ui::InputMethod* input_method_;
  std::unique_ptr<chromecast::webview::WebviewResponse> last_focus_response_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_INPUT_METHOD_OBSERVER_H_
