// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/utils/network_utils.h"

#include "base/time/time.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/resource_request.h"

namespace ash::report::utils {

const char kFresnelBaseUrl[] = "https://crosfresnel-pa.googleapis.com";

std::string GetAPIKey() {
  return google_apis::GetFresnelAPIKey();
}

GURL GetOprfRequestURL() {
  return GURL(std::string(kFresnelBaseUrl) + "/v1/fresnel/psmRlweOprf");
}

GURL GetQueryRequestURL() {
  return GURL(std::string(kFresnelBaseUrl) + "/v1/fresnel/psmRlweQuery");
}

GURL GetImportRequestURL() {
  return GURL(std::string(kFresnelBaseUrl) + "/v1/fresnel/psmRlweImport");
}

base::TimeDelta GetOprfRequestTimeout() {
  return base::Seconds(15);
}

base::TimeDelta GetQueryRequestTimeout() {
  return base::Seconds(65);
}

base::TimeDelta GetImportRequestTimeout() {
  return base::Seconds(15);
}

size_t GetMaxFresnelResponseSizeBytes() {
  // 5 MiB
  return 5 << 20;
}

std::unique_ptr<network::ResourceRequest> GenerateResourceRequest(
    const GURL& url) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->headers.SetHeader("x-goog-api-key", GetAPIKey());
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/x-protobuf");
  return resource_request;
}

}  // namespace ash::report::utils
