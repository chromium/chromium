// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/autofill_assistant_onboarding_fetcher.h"

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "components/autofill_assistant/browser/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace autofill_assistant {
namespace {

bool ExtractStrings(const base::Value& json,
                    AutofillAssistantOnboardingFetcher::StringMap& string_map) {
  for (const auto intent_it : json.DictItems()) {
    const auto& intent = intent_it.first;
    if (!intent_it.second.is_dict()) {
      return false;
    }
    base::flat_map<std::string, std::string> strings;
    for (const auto string_it : intent_it.second.DictItems()) {
      const auto& string_id = string_it.first;
      if (!string_it.second.is_string()) {
        return false;
      }
      strings[string_id] = string_it.second.GetString();
    }
    string_map[intent] = strings;
  }
  return true;
}

}  // namespace

constexpr char kDefaultOnboardingDataUrlPattern[] =
    "https://www.gstatic.com/autofill_assistant/$1/onboarding_definition.json";

constexpr int kMaxDownloadSizeInBytes = 10 * 1024;

AutofillAssistantOnboardingFetcher::AutofillAssistantOnboardingFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

AutofillAssistantOnboardingFetcher::~AutofillAssistantOnboardingFetcher() =
    default;

void AutofillAssistantOnboardingFetcher::FetchOnboardingDefinition(
    const std::string& intent,
    const std::string& locale,
    int timeout_ms,
    ResponseCallback callback) {
  pending_callbacks_.emplace_back(
      base::BindOnce(&AutofillAssistantOnboardingFetcher::RunCallback,
                     base::Unretained(this), intent, std::move(callback)));
  StartFetch(locale, timeout_ms);
}

void AutofillAssistantOnboardingFetcher::StartFetch(const std::string& locale,
                                                    int timeout_ms) {
  static const base::TimeDelta kFetchTimeout(base::Milliseconds(timeout_ms));
  if (url_loader_) {
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(base::ReplaceStringPlaceholders(
      kDefaultOnboardingDataUrlPattern, {locale}, /* offset= */ nullptr));
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gstatic_onboarding_definition",
                                          R"(
          semantics {
            sender: "Autofill Assistant"
            description:
              "A JSON file hosted by gstatic containing a definition of "
              "content for onboarding."
            trigger:
              "When Autofill Assistant starts for a user that has not "
              "previously accepted the onboarding."
            data:
              "The request body is empty. No user data is included."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            policy_exception_justification:
              "TODO(crbug.com/1231780): Add this field."
          }
      )");
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->SetTimeoutDuration(kFetchTimeout);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&AutofillAssistantOnboardingFetcher::OnFetchComplete,
                     base::Unretained(this)),
      kMaxDownloadSizeInBytes);
}

void AutofillAssistantOnboardingFetcher::OnFetchComplete(
    std::unique_ptr<std::string> response_body) {
  url_loader_.reset();
  Metrics::OnboardingFetcherResultStatus result_status =
      ParseResponse(std::move(response_body));
  Metrics::RecordOnboardingFetcherResult(result_status);
  for (auto& callback : pending_callbacks_) {
    std::move(callback).Run();
  }
  pending_callbacks_.clear();
}

Metrics::OnboardingFetcherResultStatus
AutofillAssistantOnboardingFetcher::ParseResponse(
    std::unique_ptr<std::string> response_body) {
  onboarding_strings_.clear();

  if (!response_body) {
    return Metrics::OnboardingFetcherResultStatus::kNoBody;
  }

  auto data = base::JSONReader::ReadAndReturnValueWithError(*response_body);

  if (!data.has_value()) {
    DVLOG(1) << "Parse error: " << data.error().message;
    return Metrics::OnboardingFetcherResultStatus::kInvalidJson;
  }
  if (!data->is_dict()) {
    return Metrics::OnboardingFetcherResultStatus::kInvalidData;
  }
  return ExtractStrings(*data, onboarding_strings_)
             ? Metrics::OnboardingFetcherResultStatus::kOk
             : Metrics::OnboardingFetcherResultStatus::kInvalidData;
}

void AutofillAssistantOnboardingFetcher::RunCallback(
    const std::string& intent,
    ResponseCallback callback) {
  std::move(callback).Run(onboarding_strings_[intent]);
}

}  // namespace autofill_assistant
