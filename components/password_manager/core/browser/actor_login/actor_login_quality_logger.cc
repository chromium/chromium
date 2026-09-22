// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/variations/service/variations_service.h"

ActorLoginQualityLogger::ActorLoginQualityLogger(
    variations::VariationsService* variations_service,
    optimization_guide::ModelQualityLogsUploaderService* mqls_uploader)
    : log_entry_(std::make_unique<optimization_guide::ModelQualityLogEntry>(
          mqls_uploader ? mqls_uploader->GetWeakPtr() : nullptr)) {
  if (variations_service) {
    quality().set_location(
        base::ToUpperASCII(variations_service->GetLatestCountry()));
  }
}

ActorLoginQualityLogger::~ActorLoginQualityLogger() = default;

optimization_guide::proto::ActorLoginQuality&
ActorLoginQualityLogger::quality() {
  return *log_entry_->log_ai_data_request()
              ->mutable_actor_login()
              ->mutable_quality();
}

void ActorLoginQualityLogger::SetDomainAndLanguage(
    translate::TranslateManager* translate_manager,
    const GURL& url) {
  // This should only be set once per log entry, by the first
  // request.
  if (quality().has_domain()) {
    return;
  }
  quality().set_domain(
      affiliations::GetExtendedTopLevelDomain(url,
                                              /*psl_extensions=*/{}));
  if (translate_manager) {
    quality().set_language(
        translate_manager->GetLanguageState()->source_language());
  }
}

void ActorLoginQualityLogger::SetGetCredentialsDetails(
    optimization_guide::proto::ActorLoginQuality_GetCredentialsDetails
        get_credentials_details) {
  quality()
      .mutable_get_credentials_details()
      // Merge instead of copy to avoid overwriting the federated get
      // credentials details.
      ->MergeFrom(get_credentials_details);
}

void ActorLoginQualityLogger::SetFederatedGetCredentialsDetails(
    optimization_guide::proto::ActorLoginQuality_FederatedGetCredentialsDetails
        federated_get_credentials_details) {
  quality()
      .mutable_get_credentials_details()
      ->mutable_federated_get_credentials_details()
      ->CopyFrom(federated_get_credentials_details);
}

void ActorLoginQualityLogger::AddAttemptLoginDetails(
    optimization_guide::proto::ActorLoginQuality_AttemptLoginDetails
        attempt_login_details) {
  quality().add_attempt_login_details()->CopyFrom(attempt_login_details);
}

void ActorLoginQualityLogger::SetPermissionPicked(
    optimization_guide::proto::ActorLoginQuality_PermissionOption
        permission_option) {
  quality().set_permission_picked(permission_option);
}
