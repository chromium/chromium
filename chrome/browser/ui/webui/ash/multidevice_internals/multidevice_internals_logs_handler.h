// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MULTIDEVICE_INTERNALS_MULTIDEVICE_INTERNALS_LOGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MULTIDEVICE_INTERNALS_MULTIDEVICE_INTERNALS_LOGS_HANDLER_H_

#include "base/scoped_observation.h"
#include "base/values.h"
#include "chromeos/ash/components/multidevice/logging/log_buffer.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace ash::multidevice {

// WebUIMessageHandler for the PA_LOG Macro to pass logging messages to the
// chrome://multidevice-internals logging tab.
class MultideviceLogsHandler : public content::WebUIMessageHandler,
                               public LogBuffer::Observer {
 public:
  MultideviceLogsHandler();
  MultideviceLogsHandler(const MultideviceLogsHandler&) = delete;
  MultideviceLogsHandler& operator=(const MultideviceLogsHandler&) = delete;
  ~MultideviceLogsHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // LogBuffer::Observer:
  void OnLogMessageAdded(const LogBuffer::LogMessage& log_message) override;
  void OnLogBufferCleared() override;

 private:
  // Message handler callback that returns the Log Buffer in dictionary form.
  void HandleGetLogMessages(const base::Value::List& args);

  // Message handler callback that clears the Log Buffer.
  void ClearLogBuffer(const base::Value::List& args);

  base::ScopedObservation<LogBuffer, LogBuffer::Observer> observation_{this};
};

}  // namespace ash::multidevice

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MULTIDEVICE_INTERNALS_MULTIDEVICE_INTERNALS_LOGS_HANDLER_H_
