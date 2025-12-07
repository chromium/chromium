// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/floating_workspace/floating_workspace_dialog.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace ash {

class FloatingWorkspaceDialogHandler : public content::WebUIMessageHandler {
 public:
  FloatingWorkspaceDialogHandler();
  ~FloatingWorkspaceDialogHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  void ShowDefaultScreen();
  void ShowNetworkScreen();
  void ShowErrorScreen();

  FloatingWorkspaceDialog::State state() { return state_; }

 private:
  void Initialize(const base::Value::List& args);
  void ShowNetworkDetails(const base::Value::List& args);
  void ShowNetworkConfig(const base::Value::List& args);
  void AddNetwork(const base::Value::List& args);
  void GetHostname(const base::Value::List& args);
  void Respond(const std::string& callback_id, base::ValueView response);

  FloatingWorkspaceDialog::State state_ =
      FloatingWorkspaceDialog::State::kDefault;
  base::WeakPtrFactory<FloatingWorkspaceDialogHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_HANDLER_H_
