// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_MULTIDEVICE_INTERNALS_MULTIDEVICE_INTERNALS_LOGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_MULTIDEVICE_INTERNALS_MULTIDEVICE_INTERNALS_LOGS_HANDLER_H_

#include "base/scoped_observation.h"
#include "chromeos/components/multidevice/logging/log_buffer.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

namespace chromeos {

namespace multidevice {

// WebUIMessageHandler for the PA_LOG Macro to pass logging messages to the
// chrome://multidevice-internals logging tab.
class MultideviceLogsHandler : public content::WebUIMessageHandler,
                               public multidevice::LogBuffer::Observer {
 public:
  MultideviceLogsHandler();
  MultideviceLogsHandler(const MultideviceLogsHandler&) = delete;
  MultideviceLogsHandler& operator=(const MultideviceLogsHandler&) = delete;
  ~MultideviceLogsHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // chromeos::multidevice::LogBuffer::Observer:
  void OnLogMessageAdded(
      const multidevice::LogBuffer::LogMessage& log_message) override;
  void OnLogBufferCleared() override;

 private:
  // Message handler callback that returns the Log Buffer in dictionary form.
  void HandleGetLogMessages(const base::ListValue* args);

  // Message handler callback that clears the Log Buffer.
  void ClearLogBuffer(const base::ListValue* args);

  base::ScopedObservation<multidevice::LogBuffer,
                          multidevice::LogBuffer::Observer>
      observation_{this};
};

}  // namespace multidevice

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_MULTIDEVICE_INTERNALS_MULTIDEVICE_INTERNALS_LOGS_HANDLER_H_
