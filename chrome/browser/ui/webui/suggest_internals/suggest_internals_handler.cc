// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/suggest_internals/suggest_internals_handler.h"
#include "base/time/time.h"
#include "chrome/browser/autocomplete/remote_suggestions_service_factory.h"

SuggestInternalsHandler::SuggestInternalsHandler(
    mojo::PendingReceiver<suggest_internals::mojom::PageHandler>
        pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents)
    : profile_(profile),
      web_contents_(web_contents),
      page_handler_(this, std::move(pending_page_handler)) {
  remote_suggestions_service_observation_.Observe(
      RemoteSuggestionsServiceFactory::GetForProfile(
          profile_,
          /*create_if_necessary=*/true));
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
  suggest_internals::mojom::RequestPtr request =
      suggest_internals::mojom::Request::New();
  request->id = base::UnguessableToken::Create();
  request->status = suggest_internals::mojom::RequestStatus::kHardcoded;
  request->start_time = base::Time::Now();
  request->response = response;
  std::move(callback).Run(std::move(request));
}

void SuggestInternalsHandler::OnSuggestRequestStarted(
    const base::UnguessableToken& request_id,
    const GURL& url) {
  // Update the page with the request information.
  suggest_internals::mojom::RequestPtr request =
      suggest_internals::mojom::Request::New();
  request->id = request_id;
  request->url = url;
  request->status = suggest_internals::mojom::RequestStatus::kSent;
  request->start_time = base::Time::Now();
  page_->OnSuggestRequestStarted(std::move(request));
}

void SuggestInternalsHandler::OnSuggestRequestCompleted(
    const base::UnguessableToken& request_id,
    const GURL& url,
    const bool response_received,
    const std::unique_ptr<std::string>& response_body) {
  // Update the page with the request information.
  suggest_internals::mojom::RequestPtr request =
      suggest_internals::mojom::Request::New();
  request->id = request_id;
  request->url = url;
  request->status = response_received
                        ? suggest_internals::mojom::RequestStatus::kSucceeded
                        : suggest_internals::mojom::RequestStatus::kFailed;
  request->end_time = base::Time::Now();
  request->response = response_received ? *response_body : "";
  page_->OnSuggestRequestCompleted(std::move(request));

  // If the page has hardcoded a response, override the response.
  if (response_received && !hardcoded_response_.empty()) {
    auto& non_const_response_body =
        const_cast<std::unique_ptr<std::string>&>(response_body);
    *non_const_response_body = hardcoded_response_;
  }
}
