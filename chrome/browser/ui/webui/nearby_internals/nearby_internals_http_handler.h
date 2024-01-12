// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_HTTP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_HTTP_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_http_notifier.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "third_party/nearby/sharing/proto/certificate_rpc.pb.h"
#include "third_party/nearby/sharing/proto/contact_rpc.pb.h"
#include "third_party/nearby/sharing/proto/device_rpc.pb.h"

namespace content {
class BrowserContext;
}  // namespace content

// WebUIMessageHandler for HTTP Messages to pass messages to the
// chrome://nearby-internals HTTP tab.
class NearbyInternalsHttpHandler : public content::WebUIMessageHandler,
                                   public NearbyShareHttpNotifier::Observer {
 public:
  explicit NearbyInternalsHttpHandler(content::BrowserContext* context);
  NearbyInternalsHttpHandler(const NearbyInternalsHttpHandler&) = delete;
  NearbyInternalsHttpHandler& operator=(const NearbyInternalsHttpHandler&) =
      delete;
  ~NearbyInternalsHttpHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // NearbyShareHttpNotifier::Observer:
  void OnUpdateDeviceRequest(
      const nearby::sharing::proto::UpdateDeviceRequest& request) override;
  void OnUpdateDeviceResponse(
      const nearby::sharing::proto::UpdateDeviceResponse& response) override;
  void OnListContactPeopleRequest(
      const nearby::sharing::proto::ListContactPeopleRequest& request) override;
  void OnListContactPeopleResponse(
      const nearby::sharing::proto::ListContactPeopleResponse& response)
      override;
  void OnListPublicCertificatesRequest(
      const nearby::sharing::proto::ListPublicCertificatesRequest& request)
      override;
  void OnListPublicCertificatesResponse(
      const nearby::sharing::proto::ListPublicCertificatesResponse& response)
      override;

 private:
  // Message handler callback that initializes JavaScript.
  void InitializeContents(const base::Value::List& args);

  // Message handler callback that calls Update Device RPC.
  void UpdateDevice(const base::Value::List& args);

  // Message handler callback that calls List Public Certificates RPC.
  void ListPublicCertificates(const base::Value::List& args);

  // Message handler callback that calls List Contacts RPC.
  void ListContactPeople(const base::Value::List& args);

  const raw_ptr<content::BrowserContext> context_;
  base::ScopedObservation<NearbyShareHttpNotifier,
                          NearbyShareHttpNotifier::Observer>
      observation_{this};
  base::WeakPtrFactory<NearbyInternalsHttpHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_HTTP_HANDLER_H_
