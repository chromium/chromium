// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/suggest_internals/suggest_internals_handler.h"

#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/autocomplete/remote_suggestions_service_factory.h"
#include "components/variations/net/variations_http_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

SuggestInternalsHandler::SuggestInternalsHandler(
    mojo::PendingReceiver<suggest_internals::mojom::PageHandler>
        pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents)
    : profile_(profile),
      web_contents_(web_contents),
      page_handler_(this, std::move(pending_page_handler)) {
  if (auto* remote_suggestions_service =
          RemoteSuggestionsServiceFactory::GetForProfile(
              profile_,
              /*create_if_necessary=*/true)) {
    remote_suggestions_service_observation_.Observe(remote_suggestions_service);
  }
}

SuggestInternalsHandler::~SuggestInternalsHandler() = default;

void SuggestInternalsHandler::SetPage(
    mojo::PendingRemote<suggest_internals::mojom::Page> pending_page) {
  page_.Bind(std::move(pending_page));
}

void SuggestInternalsHandler::HardcodeResponse(
    const std::string& response,
    base::TimeDelta delay,
    HardcodeResponseCallback callback) {
  hardcoded_response_and_delay_ = std::make_pair(response, delay);
  if (auto* remote_suggestions_service =
          RemoteSuggestionsServiceFactory::GetForProfile(
              profile_,
              /*create_if_necessary=*/true)) {
    remote_suggestions_service->SetDelegate(weak_ptr_factory_.GetWeakPtr());
  }

  // Update the page with a synthetic request representing the hardcoded
  // response.
  suggest_internals::mojom::RequestPtr mojom_request =
      suggest_internals::mojom::Request::New();
  mojom_request->id = base::UnguessableToken::Create();
  mojom_request->status = suggest_internals::mojom::RequestStatus::kHardcoded;
  mojom_request->start_time = base::Time::Now();
  mojom_request->response = response;
  std::move(callback).Run(std::move(mojom_request));
}

void SuggestInternalsHandler::OnSuggestRequestCreated(
    const base::UnguessableToken& request_id,
    const network::ResourceRequest* request) {
  // Update the page with the request information.
  suggest_internals::mojom::RequestPtr mojom_request =
      suggest_internals::mojom::Request::New();
  mojom_request->id = request_id;
  mojom_request->url = request->url;
  std::string variations_header;
  variations::GetVariationsHeader(*request, &variations_header);
  mojom_request->data[variations::kClientDataHeader] = variations_header;
  mojom_request->data[request->method] = request->url.spec();
  mojom_request->status = suggest_internals::mojom::RequestStatus::kCreated;
  page_->OnSuggestRequestCreated(std::move(mojom_request));
}

void SuggestInternalsHandler::OnSuggestRequestStarted(
    const base::UnguessableToken& request_id,
    network::SimpleURLLoader* loader,
    const std::string& request_body) {
  // Update the page with the request information.
  suggest_internals::mojom::RequestPtr mojom_request =
      suggest_internals::mojom::Request::New();
  mojom_request->id = request_id;
  mojom_request->data["Request-Body"] = request_body;
  mojom_request->status = suggest_internals::mojom::RequestStatus::kSent;
  mojom_request->start_time = base::Time::Now();
  page_->OnSuggestRequestStarted(std::move(mojom_request));
}

void SuggestInternalsHandler::OnSuggestRequestCompleted(
    const base::UnguessableToken& request_id,
    const int response_code,
    const std::unique_ptr<std::string>& response_body) {
  // Update the page with the request information.
  suggest_internals::mojom::RequestPtr mojom_request =
      suggest_internals::mojom::Request::New();
  mojom_request->id = request_id;
  mojom_request->data["Response-Code"] = base::NumberToString(response_code);
  const bool response_received = response_code == 200;
  mojom_request->status =
      response_received ? suggest_internals::mojom::RequestStatus::kSucceeded
                        : suggest_internals::mojom::RequestStatus::kFailed;
  mojom_request->end_time = base::Time::Now();
  mojom_request->response = response_received ? *response_body : "";
  page_->OnSuggestRequestCompleted(std::move(mojom_request));
}

void SuggestInternalsHandler::OnSuggestRequestCompleted(
    const network::SimpleURLLoader* source,
    const int response_code,
    std::unique_ptr<std::string> response_body,
    RemoteSuggestionsService::CompletionCallback completion_callback) {
  CHECK(hardcoded_response_and_delay_);
  const auto [hardcoded_response, delay] = *hardcoded_response_and_delay_;

  // Override the response with the hardcoded response given by the page.
  if (response_code == 200) {
    *response_body = hardcoded_response;
  }

  // Call the completion callback after the delay given by the page.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(completion_callback), source, response_code,
                     std::move(response_body)),
      delay);
}
