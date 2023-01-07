// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SANDBOX_SANDBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SANDBOX_SANDBOX_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class Value;
}  // namespace base

namespace sandbox_handler {
// This class takes care of sending the list of processes and their sandboxing
// status to the chrome://sandbox WebUI page when it is requested.
class SandboxHandler : public content::WebUIMessageHandler {
 public:
  SandboxHandler();

  SandboxHandler(const SandboxHandler&) = delete;
  SandboxHandler& operator=(const SandboxHandler&) = delete;

  ~SandboxHandler() override;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // Callback for the "requestSandboxDiagnostics" message.
  void HandleRequestSandboxDiagnostics(const base::Value::List& args);

  void OnSandboxDataFetched(base::Value results);

  void FetchSandboxDiagnosticsCompleted(base::Value sandbox_policies);
  void GetRendererProcessesAndFinish();

  // The ID of the callback that will get invoked with the sandbox list.
  base::Value sandbox_diagnostics_callback_id_;
  base::Value::List browser_processes_;
  base::Value sandbox_policies_;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<SandboxHandler> weak_ptr_factory_{this};
};

}  // namespace sandbox_handler

#endif  // CHROME_BROWSER_UI_WEBUI_SANDBOX_SANDBOX_HANDLER_H_
