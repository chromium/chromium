// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_relationship_verification/digital_asset_links_handler.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
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
// Traffic annotation for requests made by the DigitalAssetLinksHandler
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("digital_asset_links", R"(
      semantics {
        sender: "Digital Asset Links Handler"
        description:
          "Digital Asset Links APIs allows any caller to check pre declared "
          "relationships between two assets which can be either web domains "
          "or native applications. This requests checks for a specific "
          "relationship declared by a web site with an Android application"
        trigger:
          "When the related application makes a claim to have the queried "
          "relationship with the web domain"
        data: "None"
        destination: WEBSITE
        internal {
          contacts {
            owners: "//components/content_relationship_verification/OWNERS"
          }
        }
        user_data {
          type: NONE
        }
        last_reviewed: "2024-09-03"
      }
      policy {
        cookies_allowed: NO
        setting: "Not user controlled. But the verification is a trusted API "
                 "that doesn't use user data"
        policy_exception_justification:
          "Not implemented, considered not useful as no content is being "
          "uploaded; this request merely downloads the resources on the web."
      })");

// Location on a website where the asset links file can be found, see
// https://developers.google.com/digital-asset-links/v1/getting-started.
const char kAssetLinksAbsolutePath[] = ".well-known/assetlinks.json";

void RecordNumFingerprints(size_t num_fingerprints) {
  base::UmaHistogramExactLinear("DigitalAssetLinks.NumFingerprints",
                                num_fingerprints, 5);
}

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

bool StatementHasMatchingRelationship(const base::Value::Dict& statement,
                                      const std::string& target_relation) {
  const base::Value::List* relations = statement.FindList("relation");
  if (!relations) {
    return false;
  }

  for (const auto& relation : *relations) {
    if (relation.is_string() && relation.GetString() == target_relation) {
      return true;
    }
  }

  return false;
}

bool StatementHasMatchingTargetValue(
    const base::Value::Dict& statement,
    const std::string& target_key,
    const std::set<std::string>& target_value) {
  const base::Value::Dict* target = statement.FindDict("target");
  if (!target) {
    return false;
  }

  const std::string* package = target->FindString(target_key);

  return package && target_value.find(*package) != target_value.end();
}

bool StatementHasMatchingFingerprint(
    const base::Value::Dict& statement,
    const std::vector<std::string>& target_fingerprints) {
  const base::Value::List* fingerprints =
      statement.FindListByDottedPath("target.sha256_cert_fingerprints");

  if (!fingerprints) {
    return false;
  }

  RecordNumFingerprints(fingerprints->size());
  for (const std::string& target_fingerprint : target_fingerprints) {
    bool verified_fingerprint = false;
    for (const auto& fingerprint : *fingerprints) {
      if (fingerprint.is_string() &&
          fingerprint.GetString() == target_fingerprint) {
        verified_fingerprint = true;
        break;
      }
    }
    if (!verified_fingerprint) {
      return false;
    }
  }

  return true;
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

namespace content_relationship_verification {

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
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    std::string relationship,
    std::optional<std::vector<std::string>> fingerprints,
    std::map<std::string, std::set<std::string>> target_values,
    RelationshipCheckResultCallback callback,
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
    response_code = url_loader->ResponseInfo()->headers->response_code();
  }

  if (!response_body || response_code != net::HTTP_OK) {
    int net_error = url_loader->NetError();
    if (net_error == net::ERR_INTERNET_DISCONNECTED ||
        net_error == net::ERR_NAME_NOT_RESOLVED) {
      AddMessageToConsole(web_contents_.get(),
                          "Digital Asset Links connection failed.");
      std::move(callback).Run(RelationshipCheckResult::kNoConnection);
      return;
    }

    AddMessageToConsole(
        web_contents_.get(),
        base::StringPrintf(
            "Digital Asset Links endpoint responded with code %d.",
            response_code));
    std::move(callback).Run(RelationshipCheckResult::kFailure);
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&DigitalAssetLinksHandler::OnJSONParseResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(relationship),
                     std::move(fingerprints), std::move(target_values),
                     std::move(callback)));
}

void DigitalAssetLinksHandler::OnJSONParseResult(
    std::string relationship,
    std::optional<std::vector<std::string>> fingerprints,
    std::map<std::string, std::set<std::string>> target_values,
    RelationshipCheckResultCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    AddMessageToConsole(
        web_contents_.get(),
        "Digital Asset Links response parsing failed with message: " +
            result.error());
    std::move(callback).Run(RelationshipCheckResult::kFailure);
    return;
  }

  base::Value::List* statement_list = result->GetIfList();
  if (!statement_list) {
    AddMessageToConsole(web_contents_.get(), "Statement List is not a list.");
    std::move(callback).Run(RelationshipCheckResult::kFailure);
    return;
  }

  // We only output individual statement failures if none match.
  std::vector<std::string> failures;

  for (const base::Value& statement : *statement_list) {
    const base::Value::Dict* statement_dict = statement.GetIfDict();
    if (!statement_dict) {
      failures.push_back("Statement is not a dictionary.");
      continue;
    }

    if (!StatementHasMatchingRelationship(*statement_dict, relationship)) {
      failures.push_back("Statement failure matching relationship.");
      continue;
    }

    if (fingerprints &&
        !StatementHasMatchingFingerprint(*statement_dict, *fingerprints)) {
      failures.push_back("Statement failure matching fingerprint.");
      continue;
    }

    bool failed_target_check = false;
    for (const auto& key_value : target_values) {
      if (!StatementHasMatchingTargetValue(*statement_dict, key_value.first,
                                           key_value.second)) {
        failures.push_back("Statement failure matching " + key_value.first +
                           ".");
        failed_target_check = true;
        break;
      }
    }
    if (failed_target_check) {
      continue;
    }

    std::move(callback).Run(RelationshipCheckResult::kSuccess);
    return;
  }

  for (const auto& failure_reason : failures) {
    AddMessageToConsole(web_contents_.get(), failure_reason);
  }

  std::move(callback).Run(RelationshipCheckResult::kFailure);
}

bool DigitalAssetLinksHandler::CheckDigitalAssetLinkRelationshipForAndroidApp(
    const url::Origin& web_domain,
    const std::string& relationship,
    std::vector<std::string> fingerprints,
    const std::string& package,
    RelationshipCheckResultCallback callback) {
  // TODO(rayankans): Should we also check the namespace here?
  return CheckDigitalAssetLinkRelationship(
      web_domain, relationship, std::move(fingerprints),
      {{"package_name", {package}}}, std::move(callback));
}

bool DigitalAssetLinksHandler::CheckDigitalAssetLinkRelationshipForWebApk(
    const url::Origin& web_domain,
    const std::string& manifest_url,
    RelationshipCheckResultCallback callback) {
  return CheckDigitalAssetLinkRelationship(
      web_domain, "delegate_permission/common.query_webapk", std::nullopt,
      {{"namespace", {"web"}}, {"site", {manifest_url}}}, std::move(callback));
}

bool DigitalAssetLinksHandler::CheckDigitalAssetLinkRelationship(
    const url::Origin& web_domain,
    const std::string& relationship,
    std::optional<std::vector<std::string>> fingerprints,
    const std::map<std::string, std::set<std::string>>& target_values,
    RelationshipCheckResultCallback callback) {
  GURL request_url = GetUrlForAssetLinks(web_domain);

  if (!request_url.is_valid()) {
    return false;
  }

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = request_url;

  // Exclude credentials (cookies and client certs) from the request.
  request->credentials_mode =
      network::mojom::CredentialsMode::kOmitBug_775438_Workaround;

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(request), kTrafficAnnotation);
  url_loader->SetRetryOptions(
      kNumNetworkRetries,
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);

  // Grab a raw pointer before moving the unique_ptr into the bound callback.
  network::SimpleURLLoader* raw_url_loader = url_loader.get();
  raw_url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      shared_url_loader_factory_.get(),
      base::BindOnce(&DigitalAssetLinksHandler::OnURLLoadComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(url_loader),
                     relationship, std::move(fingerprints), target_values,
                     std::move(callback)));

  return true;
}

}  // namespace content_relationship_verification
