// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_DRIVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_DRIVER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content {
class RenderFrameHost;
}

namespace password_manager {

// There is one ContentPasswordManagerDriver per RenderFrameHost.
// The lifetime is managed by the ContentPasswordManagerDriverFactory.
class ContentPasswordManagerDriver final
    : public PasswordManagerDriver,
      public autofill::mojom::PasswordManagerDriver {
 public:
  ContentPasswordManagerDriver(content::RenderFrameHost* render_frame_host,
                               PasswordManagerClient* client);

  ContentPasswordManagerDriver(const ContentPasswordManagerDriver&) = delete;
  ContentPasswordManagerDriver& operator=(const ContentPasswordManagerDriver&) =
      delete;

  ~ContentPasswordManagerDriver() override;

  // Gets the driver for `render_frame_host`.
  static ContentPasswordManagerDriver* GetForRenderFrameHost(
      content::RenderFrameHost* render_frame_host);

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<autofill::mojom::PasswordManagerDriver>
          pending_receiver);
  void DidNavigate();

  // PasswordManagerDriver implementation.
  int GetId() const override;
  void SetPasswordFillData(
      const autofill::PasswordFormFillData& form_data) override;
  void InformNoSavedCredentials(
      bool should_show_popup_without_passwords) override;
  void FormEligibleForGenerationFound(
      const autofill::PasswordFormGenerationData& form) override;
  void GeneratedPasswordAccepted(const std::u16string& password) override;
  void GeneratedPasswordAccepted(
      const autofill::FormData& form_data,
      autofill::FieldRendererId generation_element_id,
      const std::u16string& password) override;
  void FocusNextFieldAfterPasswords() override;
  void FillField(const std::u16string& value) override;
  void FillSuggestion(const std::u16string& username,
                      const std::u16string& password) override;
  void FillSuggestionById(autofill::FieldRendererId username_element_id,
                          autofill::FieldRendererId password_element_id,
                          const std::u16string& username,
                          const std::u16string& password) override;
  void FillIntoFocusedField(bool is_password,
                            const std::u16string& credential) override;
#if BUILDFLAG(IS_ANDROID)
  void KeyboardReplacingSurfaceClosed(
      ToShowVirtualKeyboard show_virtual_keyboard) override;
  void TriggerFormSubmission() override;
#endif
  void PreviewField(autofill::FieldRendererId field_id,
                    const std::u16string& value) override;
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
  void SendLoggingAvailability() override;
  bool IsInPrimaryMainFrame() const override;
  bool CanShowAutofillUi() const override;
  int GetFrameId() const override;
  const GURL& GetLastCommittedURL() const override;
  void AnnotateFieldsWithParsingResult(
      const autofill::ParsingResult& parsing_result) override;
  base::WeakPtr<password_manager::PasswordManagerDriver> AsWeakPtr() override;

  base::WeakPtr<ContentPasswordManagerDriver> AsWeakPtrImpl() {
    return weak_factory_.GetWeakPtr();
  }

  // Notify the renderer that the user wants to trigger password generation.
  void GeneratePassword(autofill::mojom::PasswordGenerationAgent::
                            TriggeredGeneratePasswordCallback callback);

  // Returns true if the field is any of the following cases:
  // 1) The field has type="password".
  // 2) The field was type="password" field at some point of time.
  // 3) The field has a variation of the word "password" in name/id attributes.
  // 4) The server predicts the field as new password field.
  bool IsPasswordFieldForPasswordManager(
      autofill::FieldRendererId field_renderer_id,
      const content::ContextMenuParams& params);

  content::RenderFrameHost* render_frame_host() const {
    return render_frame_host_;
  }

#if defined(UNIT_TEST)
  // Exposed to allow browser tests to hook the driver.
  mojo::AssociatedReceiver<autofill::mojom::PasswordManagerDriver>&
  ReceiverForTesting() {
    return password_manager_receiver_;
  }
#endif

 protected:
  // autofill::mojom::PasswordManagerDriver:
  // Note that these messages received from a potentially compromised renderer.
  // For that reason, any access to form data should be validated via
  // bad_message::CheckChildProcessSecurityPolicy. Further, messages should not
  // be sent from a prerendered page, so we will similarly validate calls to
  // ensure that this is not the case.
  void PasswordFormsParsed(
      const std::vector<autofill::FormData>& forms_data) override;
  void PasswordFormsRendered(
      const std::vector<autofill::FormData>& visible_forms_data) override;
  void PasswordFormSubmitted(const autofill::FormData& form_data) override;
  void InformAboutUserInput(const autofill::FormData& form_data) override;
  void DynamicFormSubmission(autofill::mojom::SubmissionIndicatorEvent
                                 submission_indication_event) override;
  void PasswordFormCleared(const autofill::FormData& form_data) override;
  void RecordSavePasswordProgress(const std::string& log) override;
  void UserModifiedPasswordField() override;
  void UserModifiedNonPasswordField(autofill::FieldRendererId renderer_id,
                                    const std::u16string& value,
                                    bool autocomplete_attribute_has_username,
                                    bool is_likely_otp) override;
  void ShowPasswordSuggestions(
      const autofill::PasswordSuggestionRequest& request) override;
#if BUILDFLAG(IS_ANDROID)
  void ShowKeyboardReplacingSurface(
      autofill::mojom::SubmissionReadinessState submission_readiness,
      bool is_webauthn_form) override;
#endif
  void CheckSafeBrowsingReputation(const GURL& form_action,
                                   const GURL& frame_url) override;
  void FocusedInputChanged(
      autofill::FieldRendererId focused_field_id,
      autofill::mojom::FocusedFieldType focused_field_type) override;
  void LogFirstFillingResult(autofill::FormRendererId form_renderer_id,
                             int32_t result) override;

 private:
  void LogFilledFieldType();

  const mojo::AssociatedRemote<autofill::mojom::AutofillAgent>&
  GetAutofillAgent();

  const mojo::AssociatedRemote<autofill::mojom::PasswordAutofillAgent>&
  GetPasswordAutofillAgent();

  const mojo::AssociatedRemote<autofill::mojom::PasswordGenerationAgent>&
  GetPasswordGenerationAgent();

  const raw_ptr<content::RenderFrameHost> render_frame_host_;
  const raw_ptr<PasswordManagerClient> client_;
  PasswordGenerationFrameHelper password_generation_helper_;
  PasswordAutofillManager password_autofill_manager_;

  int id_;
  autofill::FieldRendererId last_triggering_field_id_;

  mojo::AssociatedRemote<autofill::mojom::PasswordAutofillAgent>
      password_autofill_agent_;
  const mojo::AssociatedRemote<autofill::mojom::PasswordAutofillAgent>
      password_autofill_agent_unbound_;

  mojo::AssociatedRemote<autofill::mojom::PasswordGenerationAgent>
      password_gen_agent_;

  mojo::AssociatedReceiver<autofill::mojom::PasswordManagerDriver>
      password_manager_receiver_{this};

  base::WeakPtrFactory<ContentPasswordManagerDriver> weak_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_DRIVER_H_
