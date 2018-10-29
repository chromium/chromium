// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_AUTOFILL_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_AUTOFILL_AGENT_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/autofill/content/common/autofill_agent.mojom.h"
#include "components/autofill/content/common/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/field_data_manager.h"
#include "components/autofill/content/renderer/form_tracker.h"
#include "components/autofill/content/renderer/html_based_username_detector.h"
#include "components/autofill/content/renderer/provisionally_saved_password_form.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_field_prediction_map.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_view_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_input_element.h"

#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "components/autofill/content/renderer/page_passwords_analyser.h"
#endif

namespace blink {
class WebInputElement;
}

namespace autofill {
// Used in UMA histograms, please do NOT reorder.
// Metric: "PasswordManager.PrefilledUsernameFillOutcome".
enum class PrefilledUsernameFillOutcome {
  // This value is reported if all of the following three conditions are met:
  // 1) the page has a username input element whose value was prefilled by the
  //    website itself.
  // 2) the prefilled value was found in a list of known placeholder values
  //    (e.g. "username or email").
  // 3) the user had a credential stored and the field content was overridden
  //    with the username of this credential due to 2).
  kPrefilledPlaceholderUsernameOverridden = 0,
  // This value is reported if all of the following conditions are met:
  // 1) as above.
  // 2) the prefilled value was NOT found in the list of known placeholder
  //    values.
  // 3) the user had a credential stored for this site but the field content
  //    was NOT overridden due to 2).
  kPrefilledUsernameNotOverridden = 1,
  kMaxValue = kPrefilledUsernameNotOverridden,
};

// Names of HTML attributes to show form and field signatures for debugging.
extern const char kDebugAttributeForFormSignature[];
extern const char kDebugAttributeForFieldSignature[];

class FieldDataManager;
class RendererSavePasswordProgressLogger;
class PasswordGenerationAgent;

// This class is responsible for filling password forms.
class PasswordAutofillAgent : public content::RenderFrameObserver,
                              public FormTracker::Observer,
                              public mojom::PasswordAutofillAgent {
 public:
  PasswordAutofillAgent(content::RenderFrame* render_frame,
                        blink::AssociatedInterfaceRegistry* registry);
  ~PasswordAutofillAgent() override;

  void BindRequest(mojom::PasswordAutofillAgentAssociatedRequest request);

  void SetAutofillAgent(AutofillAgent* autofill_agent);

  void SetPasswordGenerationAgent(PasswordGenerationAgent* generation_agent);

  const mojom::PasswordManagerDriverAssociatedPtr& GetPasswordManagerDriver();

  // mojom::PasswordAutofillAgent:
  void FillPasswordForm(const PasswordFormFillData& form_data) override;
  void FillIntoFocusedField(bool is_password,
                            const base::string16& credential,
                            FillIntoFocusedFieldCallback callback) override;
  void SetLoggingState(bool active) override;
  void AutofillUsernameAndPasswordDataReceived(
      const FormsPredictionsMap& predictions) override;

  // FormTracker::Observer
  void OnProvisionallySaveForm(const blink::WebFormElement& form,
                               const blink::WebFormControlElement& element,
                               ElementChangeSource source) override;
  void OnProbablyFormSubmitted() override;
  void OnFormSubmitted(const blink::WebFormElement& form) override;
  void OnInferredFormSubmission(SubmissionSource source) override;

  // WebLocalFrameClient editor related calls forwarded by AutofillAgent.
  // If they return true, it indicates the event was consumed and should not
  // be used for any other autofill activity.
  bool TextDidChangeInTextField(const blink::WebInputElement& element);

  // Event forwarded by AutofillAgent from WebAutofillClient, informing that
  // the text field editing has ended, which means that the field is not
  // focused anymore.
  void DidEndTextFieldEditing();

  // Function that should be called whenever the value of |element| changes due
  // to user input. This is separate from TextDidChangeInTextField() as that
  // function may trigger UI and should only be called when other UI won't be
  // shown.
  void UpdateStateForTextChange(const blink::WebInputElement& element);

  // Fills the username and password fields of this form with the given values.
  // Returns true if the fields were filled, false otherwise.
  bool FillSuggestion(const blink::WebFormControlElement& control_element,
                      const base::string16& username,
                      const base::string16& password);

  // Previews the username and password fields of this form with the given
  // values. Returns true if the fields were previewed, false otherwise.
  bool PreviewSuggestion(const blink::WebFormControlElement& node,
                         const blink::WebString& username,
                         const blink::WebString& password);

  // Clears the preview for the username and password fields, restoring both to
  // their previous filled state. Return false if no login information was
  // found for the form.
  bool DidClearAutofillSelection(
      const blink::WebFormControlElement& control_element);

  // Returns whether the element is a username or password textfield.
  bool IsUsernameOrPasswordField(const blink::WebInputElement& element);

  // Shows an Autofill popup with username suggestions for |element|. If
  // |show_all| is |true|, will show all possible suggestions for that element,
  // otherwise shows suggestions based on current value of |element|.
  // If |generation_popup_showing| is true, this function will return false
  // as both UIs should not be shown at the same time. This function should
  // still be called in this situation so that UMA stats can be logged.
  // Returns true if any suggestions were shown, false otherwise.
  bool ShowSuggestions(const blink::WebInputElement& element,
                       bool show_all,
                       bool generation_popup_showing);

  // Shows an Autofill-style popup with a warning that the form is not secure.
  // This UI is shown when a username or password field is autofilled or edited
  // on a non-secure page.
  void ShowNotSecureWarning(const blink::WebInputElement& element);

  // Called when new form controls are inserted.
  void OnDynamicFormsSeen();

  // Called when the user interacts with the page after a load. This is a
  // signal to make autofilled values of password input elements accessible to
  // JavaScript.
  void UserGestureObserved();

  // Given password form data |form_data| returns a set of WebInputElements in
  // |elements|, which must be non-null, that the password manager has values
  // for filling. Also takes an optional logger |logger| for logging password
  // autofill behavior.
  void GetFillableElementFromFormData(
      const PasswordFormFillData& form_data,
      RendererSavePasswordProgressLogger* logger,
      std::vector<blink::WebInputElement>* elements);

  // Called when the focused node has changed. This is not called if the focus
  // moves outside the frame.
  void FocusedNodeHasChanged(const blink::WebNode& node);

  // Creates a |PasswordForm| from |web_form|.
  std::unique_ptr<PasswordForm> GetPasswordFormFromWebForm(
      const blink::WebFormElement& web_form);

  // Creates a |PasswordForm| of fields that are not enclosed in any <form> tag.
  std::unique_ptr<PasswordForm> GetPasswordFormFromUnownedInputElements();

  bool logging_state_active() const { return logging_state_active_; }

  // Determine whether the current frame is allowed to access the password
  // manager. For example, frames with about:blank documents or documents with
  // unique origins aren't allowed access.
  virtual bool FrameCanAccessPasswordManager();

 private:
  // Ways to restrict which passwords are saved in ProvisionallySavePassword.
  enum ProvisionallySaveRestriction {
    RESTRICTION_NONE,
    RESTRICTION_NON_EMPTY_PASSWORD
  };

  struct PasswordInfo {
    blink::WebInputElement password_field;
    PasswordFormFillData fill_data;
    // The user manually edited the password more recently than the username was
    // changed.
    bool password_was_edited_last = false;
    // The user accepted a suggestion from a dropdown on a password field.
    bool password_field_suggestion_was_accepted = false;
  };
  using WebInputToPasswordInfoMap =
      std::map<blink::WebInputElement, PasswordInfo>;
  using PasswordToLoginMap =
      std::map<blink::WebInputElement, blink::WebInputElement>;

  // This class ensures that the driver will only receive relevant signals by
  // caching the parameters of the last message sent to the driver.
  class FocusStateNotifier {
   public:
    // Creates a new notifier that uses the agent which owns it to access the
    // real driver implementation.
    explicit FocusStateNotifier(PasswordAutofillAgent* agent);
    ~FocusStateNotifier();

    void FocusedInputChanged(bool is_fillable, bool is_password_field);

   private:
    bool was_fillable_;
    bool was_password_field_;
    PasswordAutofillAgent* agent_;

    DISALLOW_COPY_AND_ASSIGN(FocusStateNotifier);
  };

  // This class keeps track of autofilled password input elements and makes sure
  // the autofilled password value is not accessible to JavaScript code until
  // the user interacts with the page.
  class PasswordValueGatekeeper {
   public:
    PasswordValueGatekeeper();
    ~PasswordValueGatekeeper();

    // Call this for every autofilled password field, so that the gatekeeper
    // protects the value accordingly.
    void RegisterElement(blink::WebInputElement* element);

    // Call this to notify the gatekeeper that the user interacted with the
    // page.
    void OnUserGesture();

    // Call this to reset the gatekeeper on a new page navigation.
    void Reset();

   private:
    // Make the value of |element| accessible to JavaScript code.
    void ShowValue(blink::WebInputElement* element);

    bool was_user_gesture_seen_;
    std::vector<blink::WebInputElement> elements_;

    DISALLOW_COPY_AND_ASSIGN(PasswordValueGatekeeper);
  };

  // RenderFrameObserver:
  void DidFinishDocumentLoad() override;
  void DidFinishLoad() override;
  void DidStartProvisionalLoad(blink::WebDocumentLoader* document_loader,
                               bool is_content_initiated) override;
  void WillCommitProvisionalLoad() override;
  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override;
  void OnDestruct() override;

  // Scans the given frame for password forms and sends them up to the browser.
  // If |only_visible| is true, only forms visible in the layout are sent.
  void SendPasswordForms(bool only_visible);

  // Instructs the browser to show a pop-up suggesting which credentials could
  // be filled. |show_in_password_field| should indicate whether the pop-up is
  // to be shown on the password field instead of on the username field. If the
  // username exists, it should be passed as |user_input|. If there is no
  // username, pass the password field in |user_input|. In the latter case, no
  // username value will be shown in the pop-up.
  bool ShowSuggestionPopup(const PasswordInfo& password_info,
                           const blink::WebInputElement& user_input,
                           bool show_all,
                           bool show_on_password_field);

  // Finds the PasswordInfo, username and password fields corresponding to the
  // passed in |element|, which can refer to either a username or a password
  // element. If a PasswordInfo was found, returns |true| and assigns the
  // corresponding username, password elements and PasswordInfo into
  // |username_element|, |password_element| and |pasword_info|, respectively.
  // Note, that |username_element->IsNull()| can be true if |element| is a
  // password.
  bool FindPasswordInfoForElement(const blink::WebInputElement& element,
                                  blink::WebInputElement* username_element,
                                  blink::WebInputElement* password_element,
                                  PasswordInfo** password_info);

  // Invoked when the frame is closing.
  void FrameClosing();

  // Clears the preview for the username and password fields, restoring both to
  // their previous filled state.
  void ClearPreview(blink::WebInputElement* username,
                    blink::WebInputElement* password);

  // Checks that a given input field is valid before filling the given |input|
  // with the given |credential| and marking the field as auto-filled.
  void FillField(blink::WebInputElement* input,
                 const base::string16& credential);

  // Uses |FillField| to fill the given |credential| into the |password_input|.
  // Saves the password for its associated form.
  void FillPasswordFieldAndSave(blink::WebInputElement* password_input,
                                const base::string16& credential);

  // Saves |form| and |input| in |provisionally_saved_form_|, as long as it
  // satisfies |restriction|. |form| and |input| are the elements user has just
  // been interacting with before the form save. |form| or |input| can be null
  // but not both at the same time. For example: if the form is unowned, |form|
  // will be null; if the user has submitted the form, |input| will be null.
  void ProvisionallySavePassword(const blink::WebFormElement& form,
                                 const blink::WebInputElement& input,
                                 ProvisionallySaveRestriction restriction);

  // This function attempts to fill |username_element| and |password_element|
  // with values from |fill_data|. The |username_element| and |password_element|
  // will only have the suggestedValue set. If a match is found, return true and
  // |field_data_manager| will be modified with the autofilled credentials and
  // |FieldPropertiesFlags::AUTOFILLED| flag.
  // If |username_may_use_prefilled_placeholder| then this function may
  // overwrite the value of username field.
  bool FillUserNameAndPassword(blink::WebInputElement* username_element,
                               blink::WebInputElement* password_element,
                               const PasswordFormFillData& fill_data,
                               bool exact_username_match,
                               bool username_may_use_prefilled_placeholder,
                               FieldDataManager* field_data_manager,
                               RendererSavePasswordProgressLogger* logger);

  // Logs whether a username value that was prefilled by the website was
  // overridden when trying to fill with an existing credential. This logs
  // only one value per |PasswordAutofillAgent| instance.
  void LogPrefilledUsernameFillOutcome(PrefilledUsernameFillOutcome outcome);

  // Attempts to fill |username_element| and |password_element| with the
  // |fill_data|. Will use the data corresponding to the preferred username,
  // unless the |username_element| already has a value set. In that case,
  // attempts to fill the password matching the already filled username, if
  // such a password exists. The |password_element| will have the
  // |suggestedValue| set. Returns true if the password is filled.
  bool FillFormOnPasswordReceived(const PasswordFormFillData& fill_data,
                                  blink::WebInputElement username_element,
                                  blink::WebInputElement password_element,
                                  FieldDataManager* field_data_manager,
                                  RendererSavePasswordProgressLogger* logger);

  // Helper function called when form submission is successful.
  void FireSubmissionIfFormDisappear(
      PasswordForm::SubmissionIndicatorEvent event);

  void OnFrameDetached();
  void OnWillSubmitForm(const blink::WebFormElement& form);

  void HidePopup();

  // TODO(https://crbug.com/831123): Rename to FillPasswordForm when browser
  // form parsing is launched.
  void FillUsingRendererIDs(const PasswordFormFillData& form_data);

  // Returns pair(username_element, password_element) based on renderer ids from
  // |username_field| and |password_field| from |form_data|.
  std::pair<blink::WebInputElement, blink::WebInputElement>
  FindUsernamePasswordElements(const PasswordFormFillData& form_data);

  // Populates |web_input_to_password_info_| and |password_to_username_| in
  // order to provide fill on account select on |username_element| and
  // |password_element| with credentials from |form_data|.
  void StoreDataForFillOnAccountSelect(const PasswordFormFillData& form_data,
                                       blink::WebInputElement username_element,
                                       blink::WebInputElement password_element);

  // In case when |web_input_to_password_info_| is empty (i.e. no fill on
  // account select data yet) this function populates
  // |web_input_to_password_info_| in order to provide fill on account select on
  // any password field (aka filling fallback) with credentials from
  // |form_data|.
  void MaybeStoreFallbackData(const PasswordFormFillData& form_data);

  // The logins we have filled so far with their associated info.
  WebInputToPasswordInfoMap web_input_to_password_info_;
  // A (sort-of) reverse map to |web_input_to_password_info_|.
  PasswordToLoginMap password_to_username_;
  // The chronologically last insertion into |web_input_to_password_info_|.
  WebInputToPasswordInfoMap::iterator last_supplied_password_info_iter_;

  // Set if the user might be submitting a password form on the current page,
  // but the submit may still fail (i.e. doesn't pass JavaScript validation).
  ProvisionallySavedPasswordForm provisionally_saved_form_;

  // Map WebFormControlElement to the pair of:
  // 1) The most recent text that user typed or PasswordManager autofilled in
  // input elements. Used for storing username/password before JavaScript
  // changes them.
  // 2) Field properties mask, i.e. whether the field was autofilled, modified
  // by user, etc. (see FieldPropertiesMask).
  FieldDataManager field_data_manager_;

  PasswordValueGatekeeper gatekeeper_;

  // The currently focused input field. Not null if its a valid input that can
  // be filled with a suggestions.
  blink::WebInputElement focused_input_element_;

  // True indicates that user debug information should be logged.
  bool logging_state_active_;

  // Indicates whether the field is filled, previewed, or not filled by
  // autofill.
  blink::WebAutofillState username_autofill_state_;
  // Indicates whether the field is filled, previewed, or not filled by
  // autofill.
  blink::WebAutofillState password_autofill_state_;

  // True indicates that a request for credentials has been sent to the store.
  bool sent_request_to_store_;

  // True indicates that a safe browsing reputation check has been triggered.
  bool checked_safe_browsing_reputation_;

  // Records the username typed before suggestions preview.
  base::string16 username_query_prefix_;

  // Contains server predictions for username, password and/or new password
  // fields for individual forms.
  FormsPredictionsMap form_predictions_;

  // The HTML based username detector's cache which maps form elements to
  // username predictions.
  UsernameDetectorCache username_detector_cache_;

  // This notifier is used to avoid sending redundant messages to the password
  // manager driver mojo interface.
  FocusStateNotifier focus_state_notifier_;

  base::WeakPtr<AutofillAgent> autofill_agent_;

  PasswordGenerationAgent* password_generation_agent_;  // Weak reference.

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  PagePasswordsAnalyser page_passwords_analyser_;
#endif

  mojom::PasswordManagerDriverAssociatedPtr password_manager_driver_;

  mojo::AssociatedBinding<mojom::PasswordAutofillAgent> binding_;

  bool prefilled_username_metrics_logged_ = false;
  DISALLOW_COPY_AND_ASSIGN(PasswordAutofillAgent);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_AUTOFILL_AGENT_H_
