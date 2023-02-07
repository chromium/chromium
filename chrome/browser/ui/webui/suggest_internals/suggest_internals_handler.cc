// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/suggest_internals/suggest_internals_handler.h"

#include "base/time/time.h"
#include "chrome/browser/autocomplete/remote_suggestions_service_factory.h"
#include "components/variations/net/variations_http_headers.h"
#include "services/network/public/cpp/resource_request.h"

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
    HardcodeResponseCallback callback) {
  hardcoded_response_ = response;
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

void SuggestInternalsHandler::OnSuggestRequestStarting(
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
  mojom_request->status = suggest_internals::mojom::RequestStatus::kSent;
  mojom_request->start_time = base::Time::Now();
  page_->OnSuggestRequestStarting(std::move(mojom_request));
}

void SuggestInternalsHandler::OnSuggestRequestCompleted(
    const base::UnguessableToken& request_id,
    const bool response_received,
    const std::unique_ptr<std::string>& response_body) {
  // Update the page with the request information.
  suggest_internals::mojom::RequestPtr mojom_request =
      suggest_internals::mojom::Request::New();
  mojom_request->id = request_id;
  mojom_request->status =
      response_received ? suggest_internals::mojom::RequestStatus::kSucceeded
                        : suggest_internals::mojom::RequestStatus::kFailed;
  mojom_request->end_time = base::Time::Now();
  mojom_request->response = response_received ? *response_body : "";
  page_->OnSuggestRequestCompleted(std::move(mojom_request));

  // If the page has hardcoded a response, override the response.
  if (response_received && !hardcoded_response_.empty()) {
    auto& non_const_response_body =
        const_cast<std::unique_ptr<std::string>&>(response_body);
    *non_const_response_body = hardcoded_response_;
  }
}
