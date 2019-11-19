// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/download_internals/download_internals_ui_message_handler.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/values.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/download/public/background_service/download_params.h"
#include "components/download/public/background_service/download_service.h"
#include "content/public/browser/web_ui.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace download_internals {

DownloadInternalsUIMessageHandler::DownloadInternalsUIMessageHandler()
    : download_service_(nullptr) {}

DownloadInternalsUIMessageHandler::~DownloadInternalsUIMessageHandler() {
  if (download_service_)
    download_service_->GetLogger()->RemoveObserver(this);
}

void DownloadInternalsUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getServiceStatus",
      base::BindRepeating(
          &DownloadInternalsUIMessageHandler::HandleGetServiceStatus,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getServiceDownloads",
      base::BindRepeating(
          &DownloadInternalsUIMessageHandler::HandleGetServiceDownloads,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "startDownload",
      base::BindRepeating(
          &DownloadInternalsUIMessageHandler::HandleStartDownload,
          weak_ptr_factory_.GetWeakPtr()));

  Profile* profile = Profile::FromWebUI(web_ui());
  download_service_ =
      DownloadServiceFactory::GetForKey(profile->GetProfileKey());
  download_service_->GetLogger()->AddObserver(this);
}

void DownloadInternalsUIMessageHandler::OnServiceStatusChanged(
    const base::Value& service_status) {
  if (!IsJavascriptAllowed())
    return;

  FireWebUIListener("service-status-changed", service_status);
}

void DownloadInternalsUIMessageHandler::OnServiceDownloadsAvailable(
    const base::Value& service_downloads) {
  if (!IsJavascriptAllowed())
    return;

  FireWebUIListener("service-downloads-available", service_downloads);
}

void DownloadInternalsUIMessageHandler::OnServiceDownloadChanged(
    const base::Value& service_download) {
  if (!IsJavascriptAllowed())
    return;

  FireWebUIListener("service-download-changed", service_download);
}

void DownloadInternalsUIMessageHandler::OnServiceDownloadFailed(
    const base::Value& service_download) {
  if (!IsJavascriptAllowed())
    return;

  FireWebUIListener("service-download-failed", service_download);
}

void DownloadInternalsUIMessageHandler::OnServiceRequestMade(
    const base::Value& service_request) {
  if (!IsJavascriptAllowed())
    return;

  FireWebUIListener("service-request-made", service_request);
}

void DownloadInternalsUIMessageHandler::HandleGetServiceStatus(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  ResolveJavascriptCallback(*callback_id,
                            download_service_->GetLogger()->GetServiceStatus());
}

void DownloadInternalsUIMessageHandler::HandleGetServiceDownloads(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  ResolveJavascriptCallback(
      *callback_id, download_service_->GetLogger()->GetServiceDownloads());
}

void DownloadInternalsUIMessageHandler::HandleStartDownload(
    const base::ListValue* args) {
  CHECK_GT(args->GetList().size(), 1u) << "Missing argument download URL.";
  GURL url = GURL(args->GetList()[1].GetString());
  if (!url.is_valid()) {
    LOG(WARNING) << "Can't parse download URL, try to enter a valid URL.";
    return;
  }

  download::DownloadParams params;
  params.guid = base::GenerateGUID();
  params.client = download::DownloadClient::DEBUGGING;
  params.request_params.method = "GET";
  params.request_params.url = url;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("download_internals_webui_source", R"(
          semantics {
            sender: "Download Internals Page"
            description:
              "Starts a download with background download service in WebUI."
            trigger:
              "User clicks on the download button in "
              "chrome://download-internals."
            data: "None"
            destination: WEBSITE
          }
          policy {
            cookies_allowed: YES
            cookies_store: "user"
            setting: "This feature cannot be disabled by settings."
            policy_exception_justification: "Not implemented."
          })");

  params.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(traffic_annotation);

  DCHECK(download_service_);
  download_service_->StartDownload(params);
}

}  // namespace download_internals
