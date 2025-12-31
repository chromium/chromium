// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/aim_eligibility/aim_eligibility_page_handler.h"

#include <utility>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "content/public/browser/web_contents.h"

AimEligibilityPageHandler::AimEligibilityPageHandler(
    Profile* profile,
    mojo::PendingReceiver<aim_eligibility::mojom::PageHandler> receiver,
    mojo::PendingRemote<aim_eligibility::mojom::Page> page)
    : profile_(profile),
      pref_service_(profile->GetPrefs()),
      aim_eligibility_service_(
          AimEligibilityServiceFactory::GetForProfile(profile)),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  CHECK(aim_eligibility_service_);
  eligibility_changed_subscription_ =
      aim_eligibility_service_->RegisterEligibilityChangedCallback(
          base::BindRepeating(&AimEligibilityPageHandler::OnEligibilityChanged,
                              weak_ptr_factory_.GetWeakPtr()));
}

AimEligibilityPageHandler::~AimEligibilityPageHandler() = default;

void AimEligibilityPageHandler::GetEligibilityState(
    GetEligibilityStateCallback callback) {
  std::move(callback).Run(QueryEligibilityState());
}

void AimEligibilityPageHandler::RequestServerEligibilityForDebugging() {
  aim_eligibility_service_->StartServerEligibilityRequestForDebugging();
}

void AimEligibilityPageHandler::SetEligibilityResponseForDebugging(
    const std::string& base64_encoded_response,
    SetEligibilityResponseForDebuggingCallback callback) {
  bool success = aim_eligibility_service_->SetEligibilityResponseForDebugging(
      base64_encoded_response);
  std::move(callback).Run(success);
}

void AimEligibilityPageHandler::OnEligibilityChanged() {
  page_->OnEligibilityStateChanged(QueryEligibilityState());
}

aim_eligibility::mojom::EligibilityStatePtr
AimEligibilityPageHandler::QueryEligibilityState() {
  auto state = aim_eligibility::mojom::EligibilityState::New();

  state->is_eligible = aim_eligibility_service_->IsAimEligible();
  state->is_eligible_by_policy =
      AimEligibilityService::IsAimAllowedByPolicy(pref_service_);
  state->is_eligible_by_dse = aim_eligibility_service_->IsAimAllowedByDse();
  state->is_server_eligibility_enabled =
      aim_eligibility_service_->IsServerEligibilityEnabled();
  state->is_eligible_by_server = false;
  state->server_response_base64_encoded = "";
  state->server_response_base64_url_encoded = "";
  state->server_response_source = "";
  if (state->is_server_eligibility_enabled) {
    const auto& response = aim_eligibility_service_->GetMostRecentResponse();
    state->is_eligible_by_server = response.is_eligible();
    std::string response_string;
    response.SerializeToString(&response_string);
    state->server_response_base64_encoded = base::Base64Encode(response_string);
    base::Base64UrlEncode(response_string,
                          base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &state->server_response_base64_url_encoded);
    state->server_response_source =
        AimEligibilityService::EligibilityResponseSourceToString(
            aim_eligibility_service_->GetMostRecentResponseSource());
  }
  state->last_updated = base::Time::Now();
  return state;
}
