// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_MANAGER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/run_loop.h"
#include "components/autofill/core/browser/autofill_manager.h"

using base::TimeTicks;

namespace network {
class SharedURLLoaderFactory;
}

namespace autofill {

namespace payments {
class TestPaymentsClient;
}  // namespace payments

class AutofillClient;
class AutofillDriver;
class FormStructure;
class TestFormDataImporter;
class TestPersonalDataManager;

class TestAutofillManager : public AutofillManager {
 public:
  // Called by AutofillManagerTest and AutofillMetricsTest.
  TestAutofillManager(AutofillDriver* driver,
                      AutofillClient* client,
                      TestPersonalDataManager* personal_data);
  // Called by CreditCardSaveManagerTest and LocalCardMigrationManagerTest.
  TestAutofillManager(
      AutofillDriver* driver,
      AutofillClient* client,
      TestPersonalDataManager* personal_data,
      std::unique_ptr<CreditCardSaveManager> credit_card_save_manager,
      payments::TestPaymentsClient* payments_client,
      std::unique_ptr<LocalCardMigrationManager> local_card_migration_manager =
          nullptr);
  ~TestAutofillManager() override;

  // AutofillManager overrides.
  bool IsAutofillEnabled() const override;
  bool IsProfileAutofillEnabled() const override;
  bool IsCreditCardAutofillEnabled() const override;
  void UploadFormData(const FormStructure& submitted_form,
                      bool observed_submission) override;
  bool MaybeStartVoteUploadProcess(
      std::unique_ptr<FormStructure> form_structure,
      const base::TimeTicks& timestamp,
      bool observed_submission) override;
  void UploadFormDataAsyncCallback(const FormStructure* submitted_form,
                                   const base::TimeTicks& interaction_time,
                                   const base::TimeTicks& submission_time,
                                   bool observed_submission) override;

  // Unique to TestAutofillManager:

  int GetPackedCreditCardID(int credit_card_id);

  void AddSeenForm(const FormData& form,
                   const std::vector<ServerFieldType>& heuristic_types,
                   const std::vector<ServerFieldType>& server_types);

  void AddSeenFormStructure(std::unique_ptr<FormStructure> form_structure);

  void ClearFormStructures();

  const std::string GetSubmittedFormSignature();

  void SetAutofillEnabled(bool autofill_enabled);

  void SetProfileEnabled(bool profile_enabled);

  void SetCreditCardEnabled(bool credit_card_enabled);

  void SetExpectedSubmittedFieldTypes(
      const std::vector<ServerFieldTypeSet>& expected_types);

  void SetExpectedObservedSubmission(bool expected);

  void SetCallParentUploadFormData(bool value);

 private:
  TestPersonalDataManager* personal_data_;                  // Weak reference.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  TestFormDataImporter* test_form_data_importer_ = nullptr;
  bool autofill_enabled_ = true;
  bool profile_enabled_ = true;
  bool credit_card_enabled_ = true;
  bool call_parent_upload_form_data_ = false;
  base::Optional<bool> expected_observed_submission_;

  std::unique_ptr<base::RunLoop> run_loop_;

  std::string submitted_form_signature_;
  std::vector<ServerFieldTypeSet> expected_submitted_field_types_;
  AutofillClient* client_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_MANAGER_H_
