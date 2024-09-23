// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_generation_frame_helper.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/generation/password_generator.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "url/gurl.h"

using autofill::AutofillType;
using autofill::CalculateFieldSignatureForField;
using autofill::CalculateFormSignature;
using autofill::FieldSignature;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormSignature;
using autofill::password_generation::PasswordGenerationType;

namespace password_manager {

namespace {
using Logger = autofill::SavePasswordProgressLogger;
}

PasswordGenerationFrameHelper::PasswordGenerationFrameHelper(
    PasswordManagerClient* client,
    PasswordManagerDriver* driver)
    : client_(client), driver_(driver) {}

PasswordGenerationFrameHelper::~PasswordGenerationFrameHelper() = default;

void PasswordGenerationFrameHelper::PrefetchSpec(const GURL& origin) {
  // IsGenerationEnabled is called multiple times and it is sufficient to
  // log debug data once.
  if (!IsGenerationEnabled(/*log_debug_data=*/false)) {
    return;
  }

  // It is legit to have no PasswordRequirementsService on some platforms where
  // it has not been implemented.
  PasswordRequirementsService* password_requirements_service =
      client_->GetPasswordRequirementsService();
  if (!password_requirements_service) {
    return;
  }

  // Fetch password requirements for the domain.
  password_requirements_service->PrefetchSpec(origin);
}

void PasswordGenerationFrameHelper::ProcessPasswordRequirements(
    const FormData& form,
    const base::flat_map<autofill::FieldGlobalId,
                         AutofillType::ServerPrediction>& predictions) {
  // IsGenerationEnabled is called multiple times and it is sufficient to
  // log debug data once.
  if (!IsGenerationEnabled(/*log_debug_data=*/false)) {
    return;
  }

  // It is legit to have no PasswordRequirementsService on some platforms where
  // it has not been implemented.
  PasswordRequirementsService* password_requirements_service =
      client_->GetPasswordRequirementsService();
  if (!password_requirements_service) {
    return;
  }

  // Store password requirements from the autofill server.
  FormSignature form_signature = autofill::CalculateFormSignature(form);
  for (const FormFieldData& field : form.fields()) {
    if (auto it = predictions.find(field.global_id());
        it != predictions.end() && it->second.password_requirements) {
      password_requirements_service->AddSpec(
          form.url().DeprecatedGetOriginAsURL(), form_signature,
          CalculateFieldSignatureForField(field),
          *it->second.password_requirements);
    }
  }
}

// In order for password generation to be enabled, we need to make sure:
// (1) Password sync is enabled, and
// (2) Password saving is enabled
// (3) The current page is not *.google.com.
bool PasswordGenerationFrameHelper::IsGenerationEnabled(
    bool log_debug_data) const {
  std::unique_ptr<Logger> logger;
  if (log_debug_data && password_manager_util::IsLoggingActive(client_)) {
    logger = std::make_unique<BrowserSavePasswordProgressLogger>(
        client_->GetLogManager());
  }

  GURL url = driver_->GetLastCommittedURL();
  if (url.DomainIs("google.com")) {
    return false;
  }

  if (!password_manager_util::IsAbleToSavePasswords(client_)) {
    if (logger) {
      logger->LogMessage(
          Logger::STRING_GENERATION_DISABLED_NOT_ABLE_TO_SAVE_PASSWORDS);
    }
    return false;
  }

  if (!client_->IsSavingAndFillingEnabled(url)) {
    if (logger) {
      logger->LogMessage(Logger::STRING_GENERATION_DISABLED_SAVING_DISABLED);
    }
    return false;
  }

#if BUILDFLAG(IS_ANDROID)
  if (client_->GetPasswordFeatureManager()->ShouldUpdateGmsCore()) {
    if (logger) {
      logger->LogMessage(
          Logger::STRING_GENERATION_DISABLED_CHROME_DOES_NOT_SYNC_PASSWORDS);
    }
    return false;
  }
#endif

  if (client_->GetPasswordFeatureManager()->IsGenerationEnabled()) {
    return true;
  }
  if (logger) {
    logger->LogMessage(Logger::STRING_GENERATION_DISABLED_NO_SYNC);
  }

  return false;
}

bool PasswordGenerationFrameHelper::IsManualGenerationEnabledField(
    autofill::FieldRendererId field_renderer_id) const {
  return generation_enabled_fields_.contains(field_renderer_id);
}

void PasswordGenerationFrameHelper::AddManualGenerationEnabledField(
    autofill::FieldRendererId field_renderer_id) {
  generation_enabled_fields_.insert(field_renderer_id);
}

std::u16string PasswordGenerationFrameHelper::GeneratePassword(
    const GURL& last_committed_url,
    PasswordGenerationType generation_type,
    autofill::FormSignature form_signature,
    autofill::FieldSignature field_signature,
    uint64_t max_length) {
  autofill::PasswordRequirementsSpec spec;

  // Lookup password requirements.
  PasswordRequirementsService* password_requirements_service =
      client_->GetPasswordRequirementsService();
  if (password_requirements_service) {
    spec = password_requirements_service->GetSpec(
        last_committed_url.DeprecatedGetOriginAsURL(), form_signature,
        field_signature);
  }

  // Choose the password length as the minimum of default length, what website
  // allows, and what the autofill server suggests.
  uint32_t target_length = autofill::kDefaultPasswordLength;
  if (max_length && max_length < target_length) {
    target_length = max_length;
  }
  // Ignore crowdsourced password length when generation is triggered on the
  // manual fallback.
  if ((generation_type != PasswordGenerationType::kManual) &&
      spec.has_max_length() && spec.max_length() < target_length) {
    target_length = spec.max_length();
  }
  spec.set_max_length(target_length);

  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogPasswordRequirements(
        last_committed_url.DeprecatedGetOriginAsURL(), form_signature,
        field_signature, spec);
  }

  return autofill::GeneratePassword(spec);
}

}  // namespace password_manager
