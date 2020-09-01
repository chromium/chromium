// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/components/quick_answers/result_loader.h"

#include "base/bind.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/search_result_loader.h"
#include "chromeos/components/quick_answers/utils/quick_answers_metrics.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace chromeos {
namespace quick_answers {
namespace {

using network::ResourceRequest;
using network::mojom::URLLoaderFactory;

// TODO(llin): Update the policy detail after finalizing on the consent check.
constexpr net::NetworkTrafficAnnotationTag kNetworkTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("quick_answers_loader", R"(
          semantics: {
            sender: "ChromeOS Quick Answers"
            description:
              "ChromeOS requests quick answers based on the currently selected "
              "text."
            trigger:
              "Right click to trigger context menu."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy: {
            cookies_allowed: YES
            setting:
              "Quick Answers can be enabled/disabled in Chrome Settings and is "
              "subject to eligibility requirements. The user may also "
              "separately opt out of sharing screen context with Assistant."
          })");

}  // namespace

ResultLoader::ResultLoader(URLLoaderFactory* url_loader_factory,
                           ResultLoaderDelegate* delegate)
    : network_loader_factory_(url_loader_factory), delegate_(delegate) {}

ResultLoader::~ResultLoader() = default;

// static
std::unique_ptr<ResultLoader> ResultLoader::Create(
    IntentType intent_type,
    URLLoaderFactory* url_loader_factory,
    ResultLoader::ResultLoaderDelegate* delegate) {
  // TODO(llin): Add TranslationResultLoader if the intent type is translation.
  return std::make_unique<SearchResultLoader>(url_loader_factory, delegate);
}

void ResultLoader::Fetch(const std::string& selected_text) {
  DCHECK(network_loader_factory_);
  DCHECK(!selected_text.empty());

  // Load the resource.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = BuildRequestUrl(selected_text);
  loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                             kNetworkTrafficAnnotationTag);

  fetch_start_time_ = base::TimeTicks::Now();
  loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      network_loader_factory_,
      base::BindOnce(&ResultLoader::OnSimpleURLLoaderComplete,
                     base::Unretained(this)));
}

void ResultLoader::OnSimpleURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  base::TimeDelta duration = base::TimeTicks::Now() - fetch_start_time_;

  if (!response_body || loader_->NetError() != net::OK ||
      !loader_->ResponseInfo() || !loader_->ResponseInfo()->headers) {
    RecordLoadingStatus(LoadStatus::kNetworkError, duration);
    delegate_->OnNetworkError();
    return;
  }

  ProcessResponse(std::move(response_body),
                  base::BindOnce(&ResultLoader::OnResultParserComplete,
                                 base::Unretained(this)));
}

void ResultLoader::OnResultParserComplete(
    std::unique_ptr<QuickAnswer> quick_answer) {
  // Record quick answer result.
  base::TimeDelta duration = base::TimeTicks::Now() - fetch_start_time_;
  RecordLoadingStatus(
      quick_answer ? LoadStatus::kSuccess : LoadStatus::kNoResult, duration);
  RecordResult(quick_answer ? quick_answer->result_type : ResultType::kNoResult,
               duration);
  delegate_->OnQuickAnswerReceived(std::move(quick_answer));
}
}  // namespace quick_answers
}  // namespace chromeos
