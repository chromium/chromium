// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/aim_eligibility/aim_eligibility_page_handler.h"

#include <utility>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/contextual_search/pref_names.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "third_party/omnibox_proto/input_type.pb.h"
#include "third_party/omnibox_proto/searchbox_config.pb.h"

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

#if !BUILDFLAG(IS_ANDROID)
  drive_disclaimer_controller_ =
      std::make_unique<drive_picker::DriveDisclaimerController>(
          contextual_search::FpopService::Create(
              IdentityManagerFactory::GetForProfile(profile_),
              profile_->GetDefaultStoragePartition()
                  ->GetURLLoaderFactoryForBrowserProcess()));
#endif
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

#if !BUILDFLAG(IS_ANDROID)
void AimEligibilityPageHandler::OnDisclaimerStatusChecked(
    drive_picker::DriveDisclaimerController::DisclaimerStatus status) {
  aim_eligibility::mojom::DisclaimerState disclaimer_state;
  switch (status) {
    case drive_picker::DriveDisclaimerController::DisclaimerStatus::kAccepted:
      disclaimer_state = aim_eligibility::mojom::DisclaimerState::kAccepted;
      break;
    case drive_picker::DriveDisclaimerController::DisclaimerStatus::
        kNotAccepted:
      disclaimer_state = aim_eligibility::mojom::DisclaimerState::kNotAccepted;
      break;
    case drive_picker::DriveDisclaimerController::DisclaimerStatus::kRestricted:
      disclaimer_state = aim_eligibility::mojom::DisclaimerState::kRestricted;
      break;
  }
  disclaimer_check_started_ = false;
  page_->OnDriveStatusChanged(QueryDriveStatus(disclaimer_state));
}
#endif

aim_eligibility::mojom::EligibilityStatePtr
AimEligibilityPageHandler::QueryEligibilityState() {
  auto state = aim_eligibility::mojom::EligibilityState::New();

  state->is_eligible = aim_eligibility_service_->IsAimEligible();
  state->is_eligible_by_policy =
      AimEligibilityService::IsAimAllowedByPolicy(pref_service_);
  state->is_eligible_by_dse = aim_eligibility_service_->IsAimAllowedByDse();
  state->is_server_eligibility_enabled =
      aim_eligibility_service_->IsServerEligibilityEnabled();
  if (state->is_server_eligibility_enabled) {
    const auto& response = aim_eligibility_service_->GetMostRecentResponse();
    state->is_eligible_by_server = response.is_eligible();
    std::string response_string;
    response.SerializeToString(&response_string);
    state->eligibility_response_base64_encoded =
        base::Base64Encode(response_string);
    state->eligibility_response_source =
        AimEligibilityService::EligibilityResponseSourceToString(
            aim_eligibility_service_->GetMostRecentResponseSource());
    switch (aim_eligibility_service_->GetMostRecentResponseAuthMethod()) {
      case AimEligibilityService::AuthenticationMethod::kOauth:
        state->eligibility_response_auth_type = "OAuth";
        break;
      case AimEligibilityService::AuthenticationMethod::kCookie:
        state->eligibility_response_auth_type = "Cookie";
        break;
      case AimEligibilityService::AuthenticationMethod::kNone:
        break;
    }
    if (response.has_searchbox_config()) {
      std::string config_string;
      response.searchbox_config().SerializeToString(&config_string);
      base::Base64UrlEncode(config_string,
                            base::Base64UrlEncodePolicy::OMIT_PADDING,
                            &state->searchbox_config_base64_url_encoded);
    }
  }

  #if !BUILDFLAG(IS_ANDROID)
  if (!disclaimer_check_started_) {
    disclaimer_check_started_ = true;
    drive_disclaimer_controller_->CheckDisclaimerStatusAsync(
        base::BindOnce(&AimEligibilityPageHandler::OnDisclaimerStatusChecked,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  #endif

  // Drive status is populated asynchronously via OnDriveStatusChanged.
  state->drive_status = nullptr;

  state->last_updated = base::Time::Now();
  return state;
}

aim_eligibility::mojom::DriveStatusPtr
AimEligibilityPageHandler::QueryDriveStatus(
    aim_eligibility::mojom::DisclaimerState disclaimer_state) {
  auto drive_status = aim_eligibility::mojom::DriveStatus::New();
  drive_status->is_incognito = profile_->IsOffTheRecord();

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  drive_status->is_identity_match =
      identity_manager &&
      !identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
           .IsEmpty();

  const auto* config = aim_eligibility_service_->GetSearchboxConfig();
  drive_status->is_pec_eligible = false;
  if (config) {
    for (const auto& input_type_config : config->input_type_configs()) {
      if (input_type_config.input_type() == omnibox::INPUT_TYPE_DRIVE) {
        drive_status->is_pec_eligible = true;
        break;
      }
    }
  }

  drive_status->is_feature_flag_enabled =
      base::FeatureList::IsEnabled(omnibox::kComposeboxDriveContextMenuOption);
  drive_status->is_force_drive_disclaimer_accepted =
      base::FeatureList::IsEnabled(omnibox::kForceDriveDisclaimerAccepted);

  int search_sharing_value = pref_service_->GetInteger(
      contextual_search::kSearchContentSharingSettings);
  drive_status->is_search_content_sharing_enabled =
      search_sharing_value ==
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kEnabled);

  drive_status->disclaimer_state = disclaimer_state;

  // Replicate logic from InputStateModel::IsDriveSupported()
  drive_status->is_drive_supported =
      drive_status->is_pec_eligible && drive_status->is_identity_match &&
      !drive_status->is_incognito && drive_status->is_feature_flag_enabled &&
      (drive_status->disclaimer_state ==
           aim_eligibility::mojom::DisclaimerState::kAccepted ||
       drive_status->is_force_drive_disclaimer_accepted);

  return drive_status;
}
