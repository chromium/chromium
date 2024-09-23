// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_DRIVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_DRIVER_H_

#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_manager_driver.h"

namespace password_manager {

// Use this class as a base for mock or test drivers to avoid stubbing
// uninteresting pure virtual methods. All the implemented methods are just
// trivial stubs. Do NOT use in production, only use in tests.
class StubPasswordManagerDriver : public PasswordManagerDriver {
 public:
  StubPasswordManagerDriver();

  StubPasswordManagerDriver(const StubPasswordManagerDriver&) = delete;
  StubPasswordManagerDriver& operator=(const StubPasswordManagerDriver&) =
      delete;

  ~StubPasswordManagerDriver() override;

  // PasswordManagerDriver:
  int GetId() const override;
  void SetPasswordFillData(
      const autofill::PasswordFormFillData& form_data) override;
  void GeneratedPasswordAccepted(const std::u16string& password) override;
  void FocusNextFieldAfterPasswords() override;
  void FillSuggestion(const std::u16string& username,
                      const std::u16string& password) override;
  void FillSuggestionById(autofill::FieldRendererId username_element_id,
                          autofill::FieldRendererId password_element_id,
                          const std::u16string& username,
                          const std::u16string& password) override;
#if BUILDFLAG(IS_ANDROID)
  void TriggerFormSubmission() override;
#endif
  void PreviewSuggestion(const std::u16string& username,
                         const std::u16string& password) override;
  void PreviewSuggestionById(autofill::FieldRendererId username_element_id,
                             autofill::FieldRendererId password_element_id,
                             const std::u16string& username,
                             const std::u16string& password) override;
  void PreviewGenerationSuggestion(const std::u16string& password) override;
  void ClearPreviewedForm() override;
  void SetSuggestionAvailability(autofill::FieldRendererId element_id,
                                 autofill::mojom::AutofillSuggestionAvailability
                                     suggestion_availability) override;
  PasswordGenerationFrameHelper* GetPasswordGenerationHelper() override;
  PasswordManagerInterface* GetPasswordManager() override;
  PasswordAutofillManager* GetPasswordAutofillManager() override;
  bool IsInPrimaryMainFrame() const override;
  bool CanShowAutofillUi() const override;
  int GetFrameId() const override;
  const GURL& GetLastCommittedURL() const override;
  base::WeakPtr<password_manager::PasswordManagerDriver> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<StubPasswordManagerDriver> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_DRIVER_H_
