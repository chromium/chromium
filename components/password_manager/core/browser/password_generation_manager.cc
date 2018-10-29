// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_generation_manager.h"

#include "base/optional.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/password_generator.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_requirements_service.h"

using autofill::AutofillField;
using autofill::FieldSignature;
using autofill::FormSignature;
using autofill::FormStructure;

namespace password_manager {

namespace {
using Logger = autofill::SavePasswordProgressLogger;
}

PasswordGenerationManager::PasswordGenerationManager(
    PasswordManagerClient* client,
    PasswordManagerDriver* driver)
    : client_(client), driver_(driver) {
}

PasswordGenerationManager::~PasswordGenerationManager() {
}

void PasswordGenerationManager::PrefetchSpec(const GURL& origin) {
  // IsGenerationEnabled is called multiple times and it is sufficient to
  // log debug data once.
  if (!IsGenerationEnabled(/*log_debug_data=*/false))
    return;

  // It is legit to have no PasswordRequirementsService on some platforms where
  // it has not been implemented.
  PasswordRequirementsService* password_requirements_service =
      client_->GetPasswordRequirementsService();
  if (!password_requirements_service)
    return;

  // Fetch password requirements for the domain.
  password_requirements_service->PrefetchSpec(origin);
}

void PasswordGenerationManager::ProcessPasswordRequirements(
    const std::vector<autofill::FormStructure*>& forms) {
  // IsGenerationEnabled is called multiple times and it is sufficient to
  // log debug data once.
  if (!IsGenerationEnabled(/*log_debug_data=*/false))
    return;

  // It is legit to have no PasswordRequirementsService on some platforms where
  // it has not been implemented.
  PasswordRequirementsService* password_requirements_service =
      client_->GetPasswordRequirementsService();
  if (!password_requirements_service)
    return;

  // Store password requirements from the autofill server.
  for (const autofill::FormStructure* form : forms) {
    for (const auto& field : *form) {
      if (field->password_requirements()) {
        password_requirements_service->AddSpec(
            form->form_signature(), field->GetFieldSignature(),
            field->password_requirements().value());
      }
    }
  }
}

void PasswordGenerationManager::DetectFormsEligibleForGeneration(
    const std::vector<autofill::FormStructure*>& forms) {
  // IsGenerationEnabled is called multiple times and it is sufficient to
  // log debug data once. This is it!
  if (!IsGenerationEnabled(/*log_debug_data=*/true))
    return;

  std::vector<autofill::PasswordFormGenerationData>
      forms_eligible_for_generation;
  for (const FormStructure* form : forms) {
    const AutofillField* generation_field = nullptr;
    const AutofillField* confirmation_field = nullptr;
    for (const std::unique_ptr<AutofillField>& field : *form) {
      if (field->server_type() == autofill::ACCOUNT_CREATION_PASSWORD ||
          field->server_type() == autofill::NEW_PASSWORD) {
        generation_field = field.get();
      } else if (field->server_type() == autofill::CONFIRMATION_PASSWORD) {
        confirmation_field = field.get();
      }
    }
    if (generation_field) {
      autofill::PasswordFormGenerationData data(
          form->form_signature(), generation_field->GetFieldSignature());
      if (confirmation_field != nullptr) {
        data.confirmation_field_signature.emplace(
            confirmation_field->GetFieldSignature());
      }
      forms_eligible_for_generation.push_back(data);
    }
  }
  if (!forms_eligible_for_generation.empty())
    driver_->FormsEligibleForGenerationFound(forms_eligible_for_generation);
}

// In order for password generation to be enabled, we need to make sure:
// (1) Password sync is enabled, and
// (2) Password saving is enabled.
bool PasswordGenerationManager::IsGenerationEnabled(bool log_debug_data) const {
  std::unique_ptr<Logger> logger;
  if (log_debug_data && password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
  }

  if (!client_->IsSavingAndFillingEnabledForCurrentPage()) {
    if (logger)
      logger->LogMessage(Logger::STRING_GENERATION_DISABLED_SAVING_DISABLED);
    return false;
  }

  if (client_->GetPasswordSyncState() != NOT_SYNCING)
    return true;
  if (logger)
    logger->LogMessage(Logger::STRING_GENERATION_DISABLED_NO_SYNC);

  return false;
}

base::string16 PasswordGenerationManager::GeneratePassword(
    const GURL& last_committed_url,
    autofill::FormSignature form_signature,
    autofill::FieldSignature field_signature,
    uint32_t max_length,
    uint32_t* spec_priority) {
  autofill::PasswordRequirementsSpec spec;

  // Lookup password requirements.
  PasswordRequirementsService* password_requirements_service =
      client_->GetPasswordRequirementsService();
  if (password_requirements_service) {
    spec = password_requirements_service->GetSpec(
        last_committed_url.GetOrigin(), form_signature, field_signature);
  }

  if (spec_priority)
    *spec_priority = spec.priority();

  // Choose the password length as the minimum of default length, what website
  // allows, and what the autofill server suggests.
  uint32_t target_length = autofill::kDefaultPasswordLength;
  if (max_length && max_length < target_length)
    target_length = max_length;
  if (spec.has_max_length() && spec.max_length() < target_length)
    target_length = spec.max_length();
  spec.set_max_length(target_length);
  return autofill::GeneratePassword(spec);
}

}  // namespace password_manager
