// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/digital_asset_links/digital_asset_links_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/origin.h"

namespace {

// In some cases we get a network change while fetching the digital asset
// links file. See https://crbug.com/987329.
const int kNumNetworkRetries = 1;

// Location on a website where the asset links file can be found, see
// https://developers.google.com/digital-asset-links/v1/getting-started.
const char kAssetLinksAbsolutePath[] = ".well-known/assetlinks.json";

GURL GetUrlForAssetLinks(const url::Origin& origin) {
  return origin.GetURL().Resolve(kAssetLinksAbsolutePath);
}

// An example, well formed asset links file for reference:
//  [{
//    "relation": ["delegate_permission/common.handle_all_urls"],
//    "target": {
//      "namespace": "android_app",
//      "package_name": "com.peter.trustedpetersactivity",
//      "sha256_cert_fingerprints": [
//        "FA:2A:03: ... :9D"
//      ]
//    }
//  }, {
//    "relation": ["delegate_permission/common.handle_all_urls"],
//    "target": {
//      "namespace": "android_app",
//      "package_name": "com.example.firstapp",
//      "sha256_cert_fingerprints": [
//        "64:2F:D4: ... :C1"
//      ]
//    }
//  }]

bool StatementHasMatchingRelationship(const base::Value& statement,
                                      const std::string& target_relation) {
  const base::Value* relations =
      statement.FindKeyOfType("relation", base::Value::Type::LIST);

  if (!relations)
    return false;

  for (const auto& relation : relations->GetListDeprecated()) {
    if (relation.is_string() && relation.GetString() == target_relation)
      return true;
  }

  return false;
}

bool StatementHasMatchingTargetValue(
    const base::Value& statement,
    const std::string& target_key,
    const std::set<std::string>& target_value) {
  const base::Value* package = statement.FindPathOfType(
      {"target", target_key}, base::Value::Type::STRING);

  return package &&
         target_value.find(package->GetString()) != target_value.end();
}

bool StatementHasMatchingFingerprint(const base::Value& statement,
                                     const std::string& target_fingerprint) {
  const base::Value* fingerprints = statement.FindPathOfType(
      {"target", "sha256_cert_fingerprints"}, base::Value::Type::LIST);

  if (!fingerprints)
    return false;

  for (const auto& fingerprint : fingerprints->GetListDeprecated()) {
    if (fingerprint.is_string() &&
        fingerprint.GetString() == target_fingerprint) {
      return true;
    }
  }

  return false;
}

// Shows a warning message in the DevTools console.
void AddMessageToConsole(content::WebContents* web_contents,
                         const std::string& message) {
  if (web_contents) {
    web_contents->GetPrimaryMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning, message);
    return;
  }

  // Fallback to LOG.
  LOG(WARNING) << message;
}

}  // namespace

namespace digital_asset_links {

const char kDigitalAssetLinksCheckResponseKeyLinked[] = "linked";

DigitalAssetLinksHandler::DigitalAssetLinksHandler(
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    content::WebContents* web_contents)
    : shared_url_loader_factory_(std::move(factory)) {
  if (web_contents) {
    web_contents_ = web_contents->GetWeakPtr();
  }
}

DigitalAssetLinksHandler::~DigitalAssetLinksHandler() = default;

void DigitalAssetLinksHandler::OnURLLoadComplete(
    std::string relationship,
    absl::optional<std::string> fingerprint,
    std::map<std::string, std::set<std::string>> target_values,
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();

  if (!response_body || response_code != net::HTTP_OK) {
    int net_error = url_loader_->NetError();
    if (net_error == net::ERR_INTERNET_DISCONNECTED ||
        net_error == net::ERR_NAME_NOT_RESOLVED) {
      AddMessageToConsole(web_contents_.get(),
                          "Digital Asset Links connection failed.");
      std::move(callback_).Run(RelationshipCheckResult::kNoConnection);
      return;
    }

    AddMessageToConsole(
        web_contents_.get(),
        base::StringPrintf(
            "Digital Asset Links endpoint responded with code %d.",
            response_code));
    std::move(callback_).Run(RelationshipCheckResult::kFailure);
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&DigitalAssetLinksHandler::OnJSONParseResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(relationship),
                     std::move(fingerprint), std::move(target_values)));

  url_loader_.reset(nullptr);
}

void DigitalAssetLinksHandler::OnJSONParseResult(
    std::string relationship,
    absl::optional<std::string> fingerprint,
    std::map<std::string, std::set<std::string>> target_values,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    AddMessageToConsole(
        web_contents_.get(),
        "Digital Asset Links response parsing failed with message: " +
            result.error());
    std::move(callback_).Run(RelationshipCheckResult::kFailure);
    return;
  }

  auto& statement_list = *result;
  if (!statement_list.is_list()) {
    std::move(callback_).Run(RelationshipCheckResult::kFailure);
    AddMessageToConsole(web_contents_.get(), "Statement List is not a list.");
    return;
  }

  // We only output individual statement failures if none match.
  std::vector<std::string> failures;

  for (const auto& statement : statement_list.GetListDeprecated()) {
    if (!statement.is_dict()) {
      failures.push_back("Statement is not a dictionary.");
      continue;
    }

    if (!StatementHasMatchingRelationship(statement, relationship)) {
      failures.push_back("Statement failure matching relationship.");
      continue;
    }

    if (fingerprint &&
        !StatementHasMatchingFingerprint(statement, *fingerprint)) {
      failures.push_back("Statement failure matching fingerprint.");
      continue;
    }

    bool failed_target_check = false;
    for (const auto& key_value : target_values) {
      if (!StatementHasMatchingTargetValue(statement, key_value.first,
                                           key_value.second)) {
        failures.push_back("Statement failure matching " + key_value.first +
                           ".");
        failed_target_check = true;
        break;
      }
    }
    if (failed_target_check)
      continue;

    std::move(callback_).Run(RelationshipCheckResult::kSuccess);
    return;
  }

  for (const auto& failure_reason : failures)
    AddMessageToConsole(web_contents_.get(), failure_reason);

  std::move(callback_).Run(RelationshipCheckResult::kFailure);
}

bool DigitalAssetLinksHandler::CheckDigitalAssetLinkRelationshipForAndroidApp(
    const std::string& web_domain,
    const std::string& relationship,
    const std::string& fingerprint,
    const std::string& package,
    RelationshipCheckResultCallback callback) {
  // TODO(rayankans): Should we also check the namespace here?
  return CheckDigitalAssetLinkRelationship(
      web_domain, relationship, fingerprint, {{"package_name", {package}}},
      std::move(callback));
}

bool DigitalAssetLinksHandler::CheckDigitalAssetLinkRelationshipForWebApk(
    const std::string& web_domain,
    const std::string& manifest_url,
    RelationshipCheckResultCallback callback) {
  return CheckDigitalAssetLinkRelationship(
      web_domain, "delegate_permission/common.query_webapk", absl::nullopt,
      {{"namespace", {"web"}}, {"site", {manifest_url}}}, std::move(callback));
}

bool DigitalAssetLinksHandler::CheckDigitalAssetLinkRelationship(
    const std::string& web_domain,
    const std::string& relationship,
    const absl::optional<std::string>& fingerprint,
    const std::map<std::string, std::set<std::string>>& target_values,
    RelationshipCheckResultCallback callback) {
  // TODO(peconn): Propagate the use of url::Origin backwards to clients.
  GURL request_url = GetUrlForAssetLinks(url::Origin::Create(GURL(web_domain)));

  if (!request_url.is_valid())
    return false;

  // Resetting both the callback and SimpleURLLoader here to ensure
  // that any previous requests will never get a
  // OnURLLoadComplete. This effectively cancels any checks that was
  // done over this handler.
  callback_ = std::move(callback);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("digital_asset_links", R"(
        semantics {
          sender: "Digital Asset Links Handler"
          description:
            "Digital Asset Links APIs allows any caller to check pre declared"
            "relationships between two assets which can be either web domains"
            "or native applications. This requests checks for a specific "
            "relationship declared by a web site with an Android application"
          trigger:
            "When the related application makes a claim to have the queried"
            "relationship with the web domain"
          data: "None"
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "Not user controlled. But the verification is a trusted API"
                   "that doesn't use user data"
          policy_exception_justification:
            "Not implemented, considered not useful as no content is being "
            "uploaded; this request merely downloads the resources on the web."
        })");

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = request_url;

  // Exclude credentials (specifically client certs) from the request.
  request->credentials_mode =
      network::mojom::CredentialsMode::kOmitBug_775438_Workaround;

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  url_loader_->SetRetryOptions(
      kNumNetworkRetries,
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);
  url_loader_->SetTimeoutDuration(timeout_duration_);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      shared_url_loader_factory_.get(),
      base::BindOnce(&DigitalAssetLinksHandler::OnURLLoadComplete,
                     weak_ptr_factory_.GetWeakPtr(), relationship, fingerprint,
                     target_values));

  return true;
}

void DigitalAssetLinksHandler::SetTimeoutDuration(
    base::TimeDelta timeout_duration) {
  timeout_duration_ = timeout_duration;
}

}  // namespace digital_asset_links
