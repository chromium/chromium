// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOAD_INTERNALS_DOWNLOAD_INTERNALS_UI_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOAD_INTERNALS_DOWNLOAD_INTERNALS_UI_MESSAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/background_service/logger.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace download {
class BackgroundDownloadService;
}  // namespace download

namespace download_internals {

// Class acting as a controller of the chrome://download-internals WebUI.
class DownloadInternalsUIMessageHandler : public content::WebUIMessageHandler,
                                          public download::Logger::Observer {
 public:
  DownloadInternalsUIMessageHandler();

  DownloadInternalsUIMessageHandler(const DownloadInternalsUIMessageHandler&) =
      delete;
  DownloadInternalsUIMessageHandler& operator=(
      const DownloadInternalsUIMessageHandler&) = delete;

  ~DownloadInternalsUIMessageHandler() override;

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // download::Logger::Observer implementation.
  void OnServiceStatusChanged(const base::Value::Dict& service_status) override;
  void OnServiceDownloadsAvailable(
      const base::Value::List& service_downloads) override;
  void OnServiceDownloadChanged(
      const base::Value::Dict& service_download) override;
  void OnServiceDownloadFailed(
      const base::Value::Dict& service_download) override;
  void OnServiceRequestMade(const base::Value::Dict& service_request) override;

 private:
  // Get the current DownloadService and sub component statuses.
  void HandleGetServiceStatus(const base::Value::List& args);
  void HandleGetServiceDownloads(const base::Value::List& args);

  // Starts a background download.
  void HandleStartDownload(const base::Value::List& args);

  raw_ptr<download::BackgroundDownloadService> download_service_;

  base::WeakPtrFactory<DownloadInternalsUIMessageHandler> weak_ptr_factory_{
      this};
};

}  // namespace download_internals

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOAD_INTERNALS_DOWNLOAD_INTERNALS_UI_MESSAGE_HANDLER_H_
