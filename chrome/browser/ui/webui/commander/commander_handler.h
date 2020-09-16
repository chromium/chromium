// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMMANDER_COMMANDER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_COMMANDER_COMMANDER_HANDLER_H_

#include "chrome/browser/ui/commander/commander_view_model.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"

// Handles serializing and unserializing communication between the commander
// backend and WebUI interface.
class CommanderHandler : public content::WebUIMessageHandler {
 public:
  // The delegate allows CommanderHandler to send messages up to the
  // browser-side commander system.
  class Delegate {
   public:
    // The key under which the delegate is expected to be stashed in the
    // containing web contents.
    static const char kKey[];
    // Called when the text is changed in the WebUI interface.
    virtual void OnTextChanged(const base::string16& text) = 0;
    // Called when an option is selected (clicked or enter pressed) in the WebUI
    // interface.
    virtual void OnOptionSelected(size_t option_index, int result_set_id) = 0;
    // Called when the WebUI interface wants to dismiss the UI.
    virtual void OnDismiss() = 0;
    // Called when the WebUI interface's content height has changed.
    virtual void OnHeightChanged(int new_height) = 0;
    // Called when the web interface's availability changes (for example, if
    // the renderer crashes, this should be called with false).
    virtual void OnHandlerEnabled(bool is_enabled) = 0;
  };
  CommanderHandler();
  ~CommanderHandler() override;

  // Called when a new view model should be displayed.
  void ViewModelUpdated(commander::CommanderViewModel view_model);

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // WebUIMessageHandler overrides.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // WebUI message handlers

  // Handles text changes in the primary textfield. Expects a single string
  // argument.
  void HandleTextChanged(const base::ListValue* args);

  // Handles the user selecting one of the available command options.
  // Expects two numeric argument representing the index of the chosen command,
  // and the result set id of the active view model (see documentation in
  // commander::CommanderViewModel).
  void HandleOptionSelected(const base::ListValue* args);

  // Handles the user pressing "Escape", or otherwise indicating they would
  // like to dismiss the UI. No arguments expected.
  void HandleDismiss(const base::ListValue* args);

  // Handles the display height of the UI changing. Expects one numeric argument
  // representing the new height.
  void HandleHeightChanged(const base::ListValue* args);

  Delegate* delegate_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBUI_COMMANDER_COMMANDER_HANDLER_H_
