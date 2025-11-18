// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_DRIVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_DRIVER_H_

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "ui/accessibility/ax_tree_id.h"
#include "url/origin.h"

class GURL;

namespace autofill {
class FormData;
struct ParsingResult;
struct PasswordFormGenerationData;
struct PasswordFormFillData;
class AutofillDriver;
}  // namespace autofill

namespace gfx {
class RectF;
}  // namespace gfx

namespace password_manager {

class PasswordAutofillManager;
class PasswordGenerationFrameHelper;
class PasswordManagerInterface;

// Interface that allows PasswordManager core code to interact with its driver
// (i.e., obtain information from it and give information to it).
class PasswordManagerDriver {
 public:
  PasswordManagerDriver() = default;

  PasswordManagerDriver(const PasswordManagerDriver&) = delete;
  PasswordManagerDriver& operator=(const PasswordManagerDriver&) = delete;

  virtual ~PasswordManagerDriver() = default;

  // Returns driver id which is unique in the current tab.
  virtual int GetId() const = 0;

  // Propagates `form_data` to the renderer, in order to store values for
  // filling on account select, or fill on pageload if appliccable.
  virtual void PropagateFillDataOnParsingCompletion(
      const autofill::PasswordFormFillData& form_data) = 0;

  // Informs the driver that there are no saved credentials in the password
  // store for the current page. In certain situations the password manager will
  // show popups (e.g. promo UIs) when there are no saved credentials.
  virtual void InformNoSavedCredentials(
      bool should_show_popup_without_passwords) {}

  // Notifies the driver that a password can be generated on the fields
  // identified by `form`.
  virtual void FormEligibleForGenerationFound(
      const autofill::PasswordFormGenerationData& form) {}

  // Notifies the driver that the user has accepted a generated password.
  // TODO(crbug.com/40615624): delete this method. The UI should call the one
  // below.
  virtual void GeneratedPasswordAccepted(const std::u16string& password) = 0;

  // Notifies the password manager that the user has accepted a generated
  // password. The password manager can bring up some disambiguation UI in
  // response.
  virtual void GeneratedPasswordAccepted(
      const autofill::FormData& form_data,
      autofill::FieldRendererId generation_element_id,
      const std::u16string& password) {}

  // Notifies the driver that the user has rejected the generated password by
  // clicking cancel button.
  virtual void GeneratedPasswordRejected() {}

  // Notifies the driver that the focus should be advanced to the next input
  // field after password fields (assuming that password fields are adjacent
  // in account creation).
  virtual void FocusNextFieldAfterPasswords() {}

  // Tells the renderer to fill the given `value` into the triggering field.
  // Also includes the `FieldPropertiesFlags` used to update the
  // `FieldPropertiesMask` of the filled field. It invokes `success_callback`
  // with true if the filling could be performed and false otherwise.
  virtual void FillField(autofill::FieldRendererId triggering_field_id,
                         const std::u16string& value,
                         autofill::FieldPropertiesFlags field_flags,
                         base::OnceCallback<void(bool)> success_callback) {}

  // Tells the renderer to open the suggestions popup on the login field
  // specified in `field_id`.
  virtual void TriggerPasswordRecoverySuggestions(
      autofill::FieldRendererId field_id) {}

  // Tells the renderer to fill and submit a change password form, specifically
  // `password_element_id` with `old_password` and `new_password_element_id`,
  // `confirm_password_element_id` with `new_password`. Upon completion
  // asynchronously returns `form_data` with filled values.
  virtual void FillChangePasswordForm(
      autofill::FieldRendererId password_element_id,
      autofill::FieldRendererId new_password_element_id,
      autofill::FieldRendererId confirm_password_element_id,
      const std::u16string& old_password,
      const std::u16string& new_password,
      base::OnceCallback<void(const std::optional<autofill::FormData>&)>
          form_data_callback) {}

  // Tells the driver to fill the currently focused form with the `username` and
  // `password`.
  virtual void FillSuggestion(
      const std::u16string& username,
      const std::u16string& password,
      base::OnceCallback<void(bool)> success_callback) = 0;

  // Similar to `FillSuggestion` but also passes the FieldRendererIds of the
  // elements to be filled.
  // Also includes the `suggestion_source`, used to update the
  // `FieldPropertiesMask` of the filled field.
  virtual void FillSuggestionById(
      autofill::FieldRendererId username_element_id,
      autofill::FieldRendererId password_element_id,
      const std::u16string& username,
      const std::u16string& password,
      autofill::AutofillSuggestionTriggerSource suggestion_source) = 0;

  // Tells the renderer to fill the given credential into the focused element.
  // Always calls `completed_callback` with a status indicating success/error.
  virtual void FillIntoFocusedField(
      bool is_password,
      const std::u16string& user_provided_credential) {}

#if BUILDFLAG(IS_ANDROID)
  // Triggers form submission on the last interacted web input element.
  virtual void TriggerFormSubmission() {}
#endif

  // Tells the renderer to preview the given `value` into the field identified
  // by the `field_id`.
  virtual void PreviewField(autofill::FieldRendererId field_id,
                            const std::u16string& value) {}

  // Tells the driver to preview filling form with the `username` and
  // `password`.
  virtual void PreviewSuggestion(const std::u16string& username,
                                 const std::u16string& password) = 0;

  // Similar to `PreviewSuggestion` but also passes the FieldRendererIds of the
  // elements to be previewed.
  virtual void PreviewSuggestionById(
      autofill::FieldRendererId username_element_id,
      autofill::FieldRendererId password_element_id,
      const std::u16string& username,
      const std::u16string& password) = 0;

  // Tells the driver to preview a password generation suggestion.
  virtual void PreviewGenerationSuggestion(const std::u16string& password) = 0;

  // Tells the driver to clear previewed password and username fields.
  virtual void ClearPreviewedForm() = 0;

  // Updates the autofill suggestion availability of the DOM node with
  // `generation_element_id`. It is critical for a11y to keep it updated
  // to make proper announcements.
  virtual void SetSuggestionAvailability(
      autofill::FieldRendererId element_id,
      autofill::mojom::AutofillSuggestionAvailability
          suggestion_availability) = 0;

  // Returns the PasswordGenerationFrameHelper associated with this instance.
  virtual PasswordGenerationFrameHelper* GetPasswordGenerationHelper() = 0;

  // Returns the PasswordManager associated with this instance.
  virtual PasswordManagerInterface* GetPasswordManager() = 0;

  // Returns the PasswordAutofillManager associated with this instance.
  virtual PasswordAutofillManager* GetPasswordAutofillManager() = 0;

  // Sends a message to the renderer whether logging to
  // chrome://password-manager-internals is available.
  virtual void SendLoggingAvailability() {}

  // Returns true if the driver corresponds to a frame who's
  // parent is in the main frame. If the frame has no parent
  // it returns `false`.
  // TODO(crbug.com/456636505): Refactor this code since it doesn't seem
  // relevant to other password manager code.
  virtual bool IsDirectChildOfPrimaryMainFrame() const = 0;

  // Return true iff the driver corresponds to the main frame.
  virtual bool IsInPrimaryMainFrame() const = 0;

  // Return true if the driver corresponds to a fenced frame or to
  // a frame nested in a fenced frame.
  virtual bool IsNestedWithinFencedFrame() const = 0;

  // Returns true iff a popup can be shown on the behalf of the associated
  // frame.
  virtual bool CanShowAutofillUi() const = 0;

  // Returns the frame ID of the frame associated with this driver.
  virtual int GetFrameId() const = 0;

  // Returns the last committed URL of the frame.
  virtual const GURL& GetLastCommittedURL() const = 0;

  // Returns the last committed origin of the frame.
  virtual const url::Origin& GetLastCommittedOrigin() const = 0;

  // Annotate password related (username, password) DOM input elements with
  // corresponding HTML attributes. It is used only for debugging.
  virtual void AnnotateFieldsWithParsingResult(
      const autofill::ParsingResult& parsing_result) {}

  virtual gfx::RectF TransformToRootCoordinates(
      const gfx::RectF& bounds_in_frame_coordinates) = 0;

  // Checks if the view area of the field is visible.
  virtual void CheckViewAreaVisible(autofill::FieldRendererId field_id,
                                    base::OnceCallback<void(bool)>) = 0;

  virtual autofill::AutofillDriver* GetAutofillDriver() const = 0;

  // Get a WeakPtr to the instance.
  virtual base::WeakPtr<PasswordManagerDriver> AsWeakPtr() = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_DRIVER_H_
