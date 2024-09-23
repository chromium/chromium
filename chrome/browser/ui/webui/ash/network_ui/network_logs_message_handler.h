// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_NETWORK_UI_NETWORK_LOGS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_NETWORK_UI_NETWORK_LOGS_MESSAGE_HANDLER_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace ash {

class NetworkLogsMessageHandler : public content::WebUIMessageHandler {
 public:
  NetworkLogsMessageHandler();
  ~NetworkLogsMessageHandler() override;
  NetworkLogsMessageHandler(const NetworkLogsMessageHandler&) = delete;
  NetworkLogsMessageHandler& operator=(const NetworkLogsMessageHandler&) =
      delete;

 private:
  // WebUIMessageHandler
  void RegisterMessages() override;

  void Respond(const std::string& callback_id,
               const std::string& result,
               bool is_error);
  void OnStoreLogs(const base::Value::List& list);
  void OnWriteSystemLogs(const std::string& callback_id,
                         base::Value::Dict&& options,
                         std::optional<base::FilePath> syslogs_path);
  void MaybeWriteDebugLogs(const std::string& callback_id,
                           base::Value::Dict&& options);
  void OnWriteDebugLogs(const std::string& callback_id,
                        base::Value::Dict&& options,
                        std::optional<base::FilePath> logs_path);
  void MaybeWritePolicies(const std::string& callback_id,
                          base::Value::Dict&& options);
  void OnWritePolicies(const std::string& callback_id, bool result);
  void OnWriteSystemLogsCompleted(const std::string& callback_id);
  void OnSetShillDebugging(const base::Value::List& list);
  void OnSetShillDebuggingCompleted(const std::string& callback_id,
                                    bool succeeded);

  base::FilePath out_dir_;
  base::WeakPtrFactory<NetworkLogsMessageHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_NETWORK_UI_NETWORK_LOGS_MESSAGE_HANDLER_H_
