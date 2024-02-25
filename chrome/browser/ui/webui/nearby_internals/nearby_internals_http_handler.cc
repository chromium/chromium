// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_http_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_http_notifier.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "chrome/browser/nearby_sharing/logging/proto_to_dictionary_conversion.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "components/cross_device/logging/logging.h"

namespace {

// This enum class needs to stay in sync with the Rpc definition in
// chrome/browser/resources/nearby_internals/types.js.
enum class Rpc {
  kCertificate = 0,
  kContact = 1,
  kDevice = 2,
  kDeviceState = 3
};

// This enum class needs to stay in sync with the Direction definition in
// chrome/browser/resources/nearby_internals/types.js.
enum class Direction { kRequest = 0, kResponse = 1 };

std::string FormatAsJSON(const base::Value::Dict& value) {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

base::Value GetJavascriptTimestamp() {
  return base::Value(
      base::Time::Now().InMillisecondsFSinceUnixEpochIgnoringNull());
}

// FireWebUIListener message to notify the JavaScript of HTTP message addition.
const char kHttpMessageAdded[] = "http-message-added";

// Keys in the JSON representation of a Http Message
const char kHttpMessageBodyKey[] = "body";
const char kHttpMessageTimeKey[] = "time";
const char kHttpMessageRpcKey[] = "rpc";
const char kHttpMessageDirectionKey[] = "direction";

// Converts a RPC request/response to a raw dictionary value used as a
// JSON argument to JavaScript functions.
base::Value::Dict HttpMessageToDictionary(const base::Value::Dict& message,
                                          Direction dir,
                                          Rpc rpc) {
  base::Value::Dict dictionary;
  dictionary.Set(kHttpMessageBodyKey, FormatAsJSON(message));
  dictionary.Set(kHttpMessageTimeKey, GetJavascriptTimestamp());
  dictionary.Set(kHttpMessageRpcKey, static_cast<int>(rpc));
  dictionary.Set(kHttpMessageDirectionKey, static_cast<int>(dir));
  return dictionary;
}

}  // namespace

NearbyInternalsHttpHandler::NearbyInternalsHttpHandler(
    content::BrowserContext* context)
    : context_(context) {}

NearbyInternalsHttpHandler::~NearbyInternalsHttpHandler() = default;

void NearbyInternalsHttpHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initializeHttp",
      base::BindRepeating(&NearbyInternalsHttpHandler::InitializeContents,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "updateDevice",
      base::BindRepeating(&NearbyInternalsHttpHandler::UpdateDevice,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "listContactPeople",
      base::BindRepeating(&NearbyInternalsHttpHandler::ListContactPeople,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "listPublicCertificates",
      base::BindRepeating(&NearbyInternalsHttpHandler::ListPublicCertificates,
                          base::Unretained(this)));
}

void NearbyInternalsHttpHandler::OnJavascriptAllowed() {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (service_) {
    observation_.Observe(service_->GetHttpNotifier());
  } else {
    CD_LOG(ERROR, Feature::NS) << "No NearbyShareService instance to call.";
  }
}

void NearbyInternalsHttpHandler::OnJavascriptDisallowed() {
  observation_.Reset();
}

void NearbyInternalsHttpHandler::InitializeContents(
    const base::Value::List& args) {
  AllowJavascript();
}

void NearbyInternalsHttpHandler::UpdateDevice(const base::Value::List& args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (service_) {
    service_->GetLocalDeviceDataManager()->DownloadDeviceData();
  } else {
    CD_LOG(ERROR, Feature::NS) << "No NearbyShareService instance to call.";
  }
}

void NearbyInternalsHttpHandler::ListPublicCertificates(
    const base::Value::List& args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (service_) {
    service_->GetCertificateManager()->DownloadPublicCertificates();
  } else {
    CD_LOG(ERROR, Feature::NS) << "No NearbyShareService instance to call.";
  }
}

void NearbyInternalsHttpHandler::ListContactPeople(
    const base::Value::List& args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (service_) {
    service_->GetContactManager()->DownloadContacts();
  } else {
    CD_LOG(ERROR, Feature::NS) << "No NearbyShareService instance to call.";
  }
}

void NearbyInternalsHttpHandler::OnUpdateDeviceRequest(
    const nearby::sharing::proto::UpdateDeviceRequest& request) {
  FireWebUIListener(
      kHttpMessageAdded,
      HttpMessageToDictionary(UpdateDeviceRequestToReadableDictionary(request),
                              Direction::kRequest, Rpc::kDevice));
}

void NearbyInternalsHttpHandler::OnUpdateDeviceResponse(
    const nearby::sharing::proto::UpdateDeviceResponse& response) {
  FireWebUIListener(kHttpMessageAdded,
                    HttpMessageToDictionary(
                        UpdateDeviceResponseToReadableDictionary(response),
                        Direction::kResponse, Rpc::kDevice));
}

void NearbyInternalsHttpHandler::OnListContactPeopleRequest(
    const nearby::sharing::proto::ListContactPeopleRequest& request) {
  FireWebUIListener(kHttpMessageAdded,
                    HttpMessageToDictionary(
                        ListContactPeopleRequestToReadableDictionary(request),
                        Direction::kRequest, Rpc::kContact));
}

void NearbyInternalsHttpHandler::OnListContactPeopleResponse(
    const nearby::sharing::proto::ListContactPeopleResponse& response) {
  FireWebUIListener(kHttpMessageAdded,
                    HttpMessageToDictionary(
                        ListContactPeopleResponseToReadableDictionary(response),
                        Direction::kResponse, Rpc::kContact));
}

void NearbyInternalsHttpHandler::OnListPublicCertificatesRequest(
    const nearby::sharing::proto::ListPublicCertificatesRequest& request) {
  FireWebUIListener(
      kHttpMessageAdded,
      HttpMessageToDictionary(
          ListPublicCertificatesRequestToReadableDictionary(request),
          Direction::kRequest, Rpc::kCertificate));
}

void NearbyInternalsHttpHandler::OnListPublicCertificatesResponse(
    const nearby::sharing::proto::ListPublicCertificatesResponse& response) {
  FireWebUIListener(
      kHttpMessageAdded,
      HttpMessageToDictionary(
          ListPublicCertificatesResponseToReadableDictionary(response),
          Direction::kResponse, Rpc::kCertificate));
}
