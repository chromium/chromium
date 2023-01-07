// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/web_bundle_utils.h"

#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace web_package {

namespace {

constexpr char kContentTypeOptionsHeaderName[] = "x-content-type-options";
constexpr char kCrLf[] = "\r\n";
constexpr char kNoSniffHeaderValue[] = "nosniff";

}  // namespace

network::mojom::URLResponseHeadPtr CreateResourceResponse(
    const web_package::mojom::BundleResponsePtr& response) {
  return CreateResourceResponseFromHeaderString(CreateHeaderString(response));
}

std::string CreateHeaderString(
    const web_package::mojom::BundleResponsePtr& response) {
  std::vector<std::string> header_strings;
  header_strings.push_back("HTTP/1.1 ");
  header_strings.push_back(base::NumberToString(response->response_code));
  header_strings.push_back(" ");
  header_strings.push_back(net::GetHttpReasonPhrase(
      static_cast<net::HttpStatusCode>(response->response_code)));
  header_strings.push_back(kCrLf);
  for (const auto& it : response->response_headers) {
    header_strings.push_back(it.first);
    header_strings.push_back(": ");
    header_strings.push_back(it.second);
    header_strings.push_back(kCrLf);
  }
  header_strings.push_back(kCrLf);
  return net::HttpUtil::AssembleRawHeaders(
      base::JoinString(header_strings, ""));
}

network::mojom::URLResponseHeadPtr CreateResourceResponseFromHeaderString(
    const std::string& header_string) {
  auto response_head = network::mojom::URLResponseHead::New();

  response_head->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(header_string);
  response_head->headers->GetMimeTypeAndCharset(&response_head->mime_type,
                                                &response_head->charset);
  return response_head;
}

bool HasNoSniffHeader(const network::mojom::URLResponseHead& response) {
  std::string content_type_options;
  response.headers->EnumerateHeader(nullptr, kContentTypeOptionsHeaderName,
                                    &content_type_options);
  return base::EqualsCaseInsensitiveASCII(content_type_options,
                                          kNoSniffHeaderValue);
}

bool IsValidUuidInPackageURL(const GURL& url) {
  std::string spec = url.spec();
  return base::StartsWith(
             spec, "uuid-in-package:", base::CompareCase::INSENSITIVE_ASCII) &&
         base::GUID::ParseCaseInsensitive(base::StringPiece(spec).substr(16))
             .is_valid();
}

}  // namespace web_package
