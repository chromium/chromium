// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/search_and_assistant_enabled_checker.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/shared/constants.h"
#include "net/base/url_util.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace {

// String to prepend to JSON responses to prevent XSSI. See http://go/xssi.
constexpr char kJsonSafetyPrefix[] = ")]}'\n";

constexpr int kMaxBodySize = 1024;

constexpr net::NetworkTrafficAnnotationTag
    kSearchAndAssistantEnabledCheckerNetworkTag =
        net::DefineNetworkTrafficAnnotation(
            "search_and_assistant_enabled_checker",
            R"(
        semantics {
          sender: "Search and Assistant Enabled Checker"
          description:
            "HTTP request used to check if Search and Assistant (SAA) "
            "service has been disabled by dasher admin. When service is "
            "disabled, SAA will provide offline experiences for user."
          trigger:
            "User logs in, switches active profile, enables SAA or "
            "updates account information so that assistant is allowed. "
            "Also triggered when dasher admin enables SAA service for "
            "user. "
          data: "User GAIA Credentials"
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
         cookies_allowed: YES
         cookies_store: "user"
         chrome_policy {
           AssistantVoiceMatchEnabledDuringOobe {
             AssistantVoiceMatchEnabledDuringOobe : false
           }
         }
         setting:
           "User can turn on/off SAA in ChromeOS Settings under the "
           "Search and Assistant menu. For managed devices, dasher "
           "admin can disable SAA from the Additional Services menu."
        })");

bool HasJsonSafetyPrefix(std::string& json_body) {
  return base::StartsWith(json_body, kJsonSafetyPrefix,
                          base::CompareCase::SENSITIVE);
}

}  // namespace

SearchAndAssistantEnabledChecker::SearchAndAssistantEnabledChecker(
    network::mojom::URLLoaderFactory* url_loader_factory,
    Delegate* delegate)
    : url_loader_factory_(url_loader_factory), delegate_(delegate) {}

SearchAndAssistantEnabledChecker::~SearchAndAssistantEnabledChecker() {}

void SearchAndAssistantEnabledChecker::SyncSearchAndAssistantState() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url =
      net::AppendOrReplaceQueryParameter(
          GURL(chromeos::assistant::kServiceIdEndpoint),
          chromeos::assistant::kPayloadParamName,
          chromeos::assistant::kServiceIdRequestPayload);
  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kSearchAndAssistantEnabledCheckerNetworkTag);
  url_loader_->DownloadToString(
      url_loader_factory_,
      base::BindOnce(
          &SearchAndAssistantEnabledChecker::OnSimpleURLLoaderComplete,
          weak_factory_.GetWeakPtr()),
      kMaxBodySize);
}

void SearchAndAssistantEnabledChecker::OnSimpleURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  if (!response_body || url_loader_->NetError() != net::OK ||
      !url_loader_->ResponseInfo() || !url_loader_->ResponseInfo()->headers) {
    LOG(ERROR) << "Network error. Failed to get response.";
    delegate_->OnError();
    return;
  }

  if (!HasJsonSafetyPrefix(*response_body)) {
    LOG(ERROR) << "Invalid response.";
    delegate_->OnError();
    return;
  }

  // Strip the JsonSafetyPrefix and parse the response.
  data_decoder::DataDecoder::ParseJsonIsolated(
      response_body->substr(strlen(kJsonSafetyPrefix)),
      base::BindOnce(&SearchAndAssistantEnabledChecker::OnJsonParsed,
                     weak_factory_.GetWeakPtr()));
}

void SearchAndAssistantEnabledChecker::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError response) {
  if (!response.has_value() || !response->is_dict()) {
    LOG(ERROR) << "JSON parsing failed: " << response.error();
    delegate_->OnError();
    return;
  }

  // |result| is true if the Search and Assistant bit is disabled.
  auto is_disabled = response->GetDict().FindBool("result");

  delegate_->OnSearchAndAssistantStateReceived(is_disabled.value());
}
