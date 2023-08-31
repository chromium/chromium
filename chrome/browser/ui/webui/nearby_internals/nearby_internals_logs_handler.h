// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_LOGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_LOGS_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "components/cross_device/logging/log_buffer.h"
#include "components/cross_device/logging/logging.h"
#include "content/public/browser/web_ui_message_handler.h"

// WebUIMessageHandler for the NS_LOG Macro to pass logging messages to the
// chrome://nearby-internals logging tab.
class NearbyInternalsLogsHandler : public content::WebUIMessageHandler,
                                   public CrossDeviceLogBuffer::Observer {
 public:
  NearbyInternalsLogsHandler();
  NearbyInternalsLogsHandler(const NearbyInternalsLogsHandler&) = delete;
  NearbyInternalsLogsHandler& operator=(const NearbyInternalsLogsHandler&) =
      delete;
  ~NearbyInternalsLogsHandler() override;

  // content::WebUIMessageHandler
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  int FeatureEnumToInt(Feature feature);

  // CrossDeviceLogBuffer::Observer
  void OnLogMessageAdded(
      const CrossDeviceLogBuffer::LogMessage& log_message) override;
  void OnCrossDeviceLogBufferCleared() override;

  // Message handler callback that returns the Log Buffer in dictionary form.
  void HandleGetLogMessages(const base::Value::List& args);

  // Message handler callback that clears the Log Buffer.
  void ClearCrossDeviceLogBuffer(const base::Value::List& args);

  base::ScopedObservation<CrossDeviceLogBuffer, CrossDeviceLogBuffer::Observer>
      observation_{this};
  base::WeakPtrFactory<NearbyInternalsLogsHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_LOGS_HANDLER_H_
