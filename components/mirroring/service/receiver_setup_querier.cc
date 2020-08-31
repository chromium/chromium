// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/receiver_setup_querier.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "components/mirroring/service/value_util.h"
#include "components/version_info/version_info.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/raw_event_subscriber_bundle.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace mirroring {

namespace {

// The maximum number of bytes for the receiver's setup info.
constexpr int kMaxSetupResponseSizeBytes = 256 << 10;  // 256KB

// The endpoint for the setup query request.
constexpr char kSetupQueryEndpointFormat[] = "http://%s:8008/setup/eureka_info";

// The method type for the setup query request.
constexpr char kSetupQueryMethod[] = "GET";

// Helper to parse the response for receiver setup info.
bool ParseReceiverSetupInfo(const std::string& response,
                            std::string* build_version,
                            std::string* name) {
  const base::Optional<base::Value> root = base::JSONReader::Read(response);

  return root && root->is_dict() &&
         GetString(*root, "cast_build_revision", build_version) &&
         GetString(*root, "name", name);
}

net::NetworkTrafficAnnotationTag GetAnnotationTag() {
  // NOTE: the network annotation must be a string literal to be validated
  // by the network annotations checker tool.
  return net::DefineNetworkTrafficAnnotation("mirroring_get_setup_info", R"(
          semantics {
            sender: "Mirroring Service"
            description:
              "Mirroring Service sends a request to the receiver to obtain its "
              "setup info such as the build version, the model name, etc. The "
              "data is used to enable/disable feature sets at runtime."
            trigger:
              "A tab/desktop mirroring session starts."
            data: "An HTTP GET request."
            destination: OTHER
            destination_other:
              "A mirroring receiver, such as a ChromeCast device."
          }
          policy {
            cookies_allowed: NO
            setting: "This feature cannot be disabled in settings."
            chrome_policy {
              EnableMediaRouter {
                EnableMediaRouter: false
              }
            }
          })");
}

}  // namespace

ReceiverSetupQuerier::ReceiverSetupQuerier(
    const net::IPAddress& address,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> loader_factory)
    : address_(address), url_loader_factory_(std::move(loader_factory)) {
  Query();
}
ReceiverSetupQuerier::~ReceiverSetupQuerier() = default;

void ReceiverSetupQuerier::Query() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = kSetupQueryMethod;
  resource_request->url = GURL(base::StringPrintf(kSetupQueryEndpointFormat,
                                                  address_.ToString().c_str()));

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), GetAnnotationTag());
  auto* const url_loader_ptr = url_loader.get();

  url_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&ReceiverSetupQuerier::ProcessResponse,
                     weak_factory_.GetWeakPtr(), std::move(url_loader)),
      kMaxSetupResponseSizeBytes);
}

void ReceiverSetupQuerier::ProcessResponse(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    std::unique_ptr<std::string> response) {
  if (url_loader->NetError() != net::OK) {
    VLOG(2) << "Unable to fetch receiver setup info.";
    return;
  }

  if (!ParseReceiverSetupInfo(*response, &build_version_, &friendly_name_)) {
    VLOG(2) << "Unable to parse receiver setup info.";
  }
}

}  // namespace mirroring
