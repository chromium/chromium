// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_AUTOFILL_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_AUTOFILL_AGENT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/field_data_manager.h"
#include "components/autofill/content/renderer/form_tracker.h"
#include "components/autofill/content/renderer/html_based_username_detector.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_view_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
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

// Used in UMA histogram, please do NOT reorder.
// Metric: "PasswordManager.FirstRendererFillingResult".
// This metric records whether the PasswordAutofillAgent succeeded in filling
// credentials after being instructed to do so by the browser process.
enum class FillingResult {
  kSuccess = 0,
  // The password element to be filled has not been found.
  kNoPasswordElement = 1,
  // Filling only happens in iframes, if all parent frames PSL match the
  // security origin of the iframe containing the password field.
  kBlockedByFrameHierarchy = 2,
  // Passwords are not filled into fields that are not editable.
  kPasswordElementIsNotAutocompleteable = 3,
  // The username field contains a string that does not match the username of
  // any available credential.
  kUsernamePrefilledWithIncompatibleValue = 4,
  // No credential was filled due to mismatches with the username. This can
  // happen in a number of cases: In case the username field is empty and
  // readonly. In case of a username-first-flow where a user's credentials do
  // contain a username but the form contains only a password field and no
  // username field. In case of change password forms that contain no username
  // field. In case the user name is given on a page but only PSL matched
  // credentials exist for this username. There may be further cases.
  kFoundNoPasswordForUsername = 5,
  // Renderer was instructed to wait until user has manually picked a
  // credential. This happens for example if the session is an incognito
  // session, the credendial's URL matches the mainframe only via the PSL, the
  // site is on HTTP, or the form has no current password field.
  // PasswordManager.FirstWaitForUsernameReason records the root causes.
  kWaitForUsername = 6,
  // No fillable elements were found, only possible for old form parser.
  kNoFillableElementsFound = 7,
  kMaxValue = kNoFillableElementsFound,
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

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::PasswordAutofillAgent>
          pending_receiver);

  void SetAutofillAgent(AutofillAgent* autofill_agent);

  void SetPasswordGenerationAgent(PasswordGenerationAgent* generation_agent);

  const mojo::AssociatedRemote<mojom::PasswordManagerDriver>&
  GetPasswordManagerDriver();

  // mojom::PasswordAutofillAgent:
  void FillPasswordForm(const PasswordFormFillData& form_data) override;
  void InformNoSavedCredentials() override;
  void FillIntoFocusedField(bool is_password,
                            const base::string16& credential) override;
  void SetLoggingState(bool active) override;
  void TouchToFillClosed(bool show_virtual_keyboard) override;
  void AnnotateFieldsWithParsingResult(
      const ParsingResult& parsing_result) override;

  // FormTracker::Observer
  void OnProvisionallySaveForm(const blink::WebFormElement& form,
                               const blink::WebFormControlElement& element,
                               ElementChangeSource source) override;
  void OnProbablyFormSubmitted() override;
  void OnFormSubmitted(const blink::WebFormElement& form) override;
  void OnInferredFormSubmission(mojom::SubmissionSource source) override;

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

  // Sends a reputation check request in case if |element| has type password and
  // no check request were sent from this frame load.
  void MaybeCheckSafeBrowsingReputation(const blink::WebInputElement& element);

  // Returns whether the soft keyboard should be suppressed.
  bool ShouldSuppressKeyboard();

  // Asks the agent to show the touch to fill UI for |control_element|. Returns
  // whether the agent was able to do so.
  bool TryToShowTouchToFill(
      const blink::WebFormControlElement& control_element);

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

  // Called when new form controls are inserted.
  void OnDynamicFormsSeen();

  // Called when the user interacts with the page after a load. This is a
  // signal to make autofilled values of password input elements accessible to
  // JavaScript.
  void UserGestureObserved();

  // Called when the focused node has changed. This is not called if the focus
  // moves outside the frame.
  void FocusedNodeHasChanged(const blink::WebNode& node);

  // Creates a |PasswordForm| from |web_form|.
  std::unique_ptr<PasswordForm> GetPasswordFormFromWebForm(
      const blink::WebFormElement& web_form);

  // Creates a |PasswordForm| from |web_form|, that contains only the
  // |form_data|, the origin and the gaia flags.
  std::unique_ptr<PasswordForm> GetSimplifiedPasswordFormFromWebForm(
      const blink::WebFormElement& web_form);

  // Creates a |PasswordForm| of fields that are not enclosed in any <form> tag.
  std::unique_ptr<PasswordForm> GetPasswordFormFromUnownedInputElements();

  // Creates a |PasswordForm| containing only the |form_data|, origin and gaia
  // flags, for fields that are not enclosed in any <form> tag.
  std::unique_ptr<PasswordForm>
  GetSimplifiedPasswordFormFromUnownedInputElements();

  bool logging_state_active() const { return logging_state_active_; }

  // Determine whether the current frame is allowed to access the password
  // manager. For example, frames with about:blank documents or documents with
  // unique origins aren't allowed access.
  virtual bool FrameCanAccessPasswordManager();

  // RenderFrameObserver:
  void DidFinishDocumentLoad() override;
  void DidFinishLoad() override;
  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;
  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override;
  void OnDestruct() override;

 private:
  // Ways to restrict which passwords are saved in ProvisionallySavePassword.
  enum ProvisionallySaveRestriction {
    RESTRICTION_NONE,
    RESTRICTION_NON_EMPTY_PASSWORD
  };

  // Enumeration representing possible Touch To Fill states. This is used to
  // make sure that Touch To Fill will only be shown in response to the first
  // password form focus during a frame's life time and to suppress the soft
  // keyboard when Touch To Fill is shown.
  enum class TouchToFillState {
    kShouldShow,
    kIsShowing,
    kWasShown,
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

  // Stores information about form field structure.
  struct FormFieldInfo {
    uint32_t unique_renderer_id = FormFieldData::kNotSetFormControlRendererId;
    std::string form_control_type;
    std::string autocomplete_attribute;
    bool is_focusable = false;
  };

  // Stores information about form structure.
  struct FormStructureInfo {
    FormStructureInfo();
    FormStructureInfo(const FormStructureInfo& other);
    FormStructureInfo(FormStructureInfo&& other);
    ~FormStructureInfo();

    FormStructureInfo& operator=(FormStructureInfo&& other);

    uint32_t unique_renderer_id = FormData::kNotSetFormRendererId;
    std::vector<FormFieldInfo> fields;
  };

  // This class ensures that the driver will only receive relevant signals by
  // caching the parameters of the last message sent to the driver.
  class FocusStateNotifier {
   public:
    // Creates a new notifier that uses the agent which owns it to access the
    // real driver implementation.
    explicit FocusStateNotifier(PasswordAutofillAgent* agent);
    ~FocusStateNotifier();

    void FocusedInputChanged(mojom::FocusedFieldType focused_field_type);

   private:
    mojom::FocusedFieldType focused_field_type_ =
        mojom::FocusedFieldType::kUnknown;
    PasswordAutofillAgent* agent_ = nullptr;

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

  // Cleans up the state when document is shut down, e.g. when committing a new
  // document or closing the frame.
  void CleanupOnDocumentShutdown();

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
  // Returns true if the password is filled.
  bool FillUserNameAndPassword(blink::WebInputElement username_element,
                               blink::WebInputElement password_element,
                               const PasswordFormFillData& fill_data,
                               RendererSavePasswordProgressLogger* logger);

  // Logs whether a username value that was prefilled by the website was
  // overridden when trying to fill with an existing credential. This logs
  // only one value per |PasswordAutofillAgent| instance.
  void LogPrefilledUsernameFillOutcome(PrefilledUsernameFillOutcome outcome);

  // Helper function called when form submission is successful.
  void FireSubmissionIfFormDisappear(mojom::SubmissionIndicatorEvent event);

  void OnFrameDetached();
  void OnWillSubmitForm(const blink::WebFormElement& form);

  void HidePopup();

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

  // Records whether filling succeeded for the first attempt to fill on a site.
  // The logging is a bit conservative: It is possible that user-perceived
  // navigations (via dynamic HTML sites) not trigger any actual navigations
  // and therefore, the |recorded_first_filling_result_| never gets reset.
  void LogFirstFillingResult(const PasswordFormFillData& form_data,
                             FillingResult result);

  // Extracts information about form structure.
  static FormStructureInfo ExtractFormStructureInfo(const FormData& form_data);
  // Checks whether the form structure (amount of elements, element types etc)
  // was changed.
  bool WasFormStructureChanged(const FormStructureInfo& form_data) const;
  // Tries to restore |control_elements| values with cached values.
  void TryFixAutofilledForm(
      std::vector<blink::WebFormControlElement>* control_elements) const;

  // Autofills |field| with |value| and updates |gatekeeper_|,
  // |field_data_manager_|, |autofilled_elements_cache_|. |field| should be
  // non-null.
  void AutofillField(const base::string16& value, blink::WebInputElement field);

  void SetLastUpdatedFormAndField(const blink::WebFormElement& form,
                                  const blink::WebFormControlElement& input);

  // The logins we have filled so far with their associated info.
  WebInputToPasswordInfoMap web_input_to_password_info_;
  // A (sort-of) reverse map to |web_input_to_password_info_|.
  PasswordToLoginMap password_to_username_;
  // The chronologically last insertion into |web_input_to_password_info_|.
  WebInputToPasswordInfoMap::iterator last_supplied_password_info_iter_;

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

  // True indicates that a form data has been sent to the browser process. Gets
  // cleared when the form is submitted to indicate that the browser has already
  // processed the form.
  // TODO(crbug.com/949519): double check if we need this variable.
  bool browser_has_form_to_process_ = false;

  // True indicates that a safe browsing reputation check has been triggered.
  bool checked_safe_browsing_reputation_;

  // Records the username typed before suggestions preview.
  base::string16 username_query_prefix_;

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

  mojo::AssociatedRemote<mojom::PasswordManagerDriver> password_manager_driver_;

  mojo::AssociatedReceiver<mojom::PasswordAutofillAgent> receiver_{this};

  bool prefilled_username_metrics_logged_ = false;

  // Keeps autofilled values for the form elements.
  std::map<unsigned /*renderer id*/, blink::WebString>
      autofilled_elements_cache_;
  std::set<unsigned /*renderer id*/> all_autofilled_elements_;
  // Keeps forms structure (amount of elements, element types etc).
  // TODO(crbug/898109): It's too expensive to keep the whole FormData
  // structure. Replace FormData with a smaller structure.
  std::map<unsigned /*renderer id*/, FormStructureInfo> forms_structure_cache_;

  // Flag to prevent that multiple PasswordManager.FirstRendererFillingResult
  // UMA metrics are recorded per page load. This is reset on
  // DidCommitProvisionalLoad() but only for non-same-document-navigations.
  bool recorded_first_filling_result_ = false;

  // Contains renderer id of last updated input element.
  uint32_t last_updated_field_renderer_id_ = FormData::kNotSetFormRendererId;
  // Contains renderer id of the form of the last updated input element.
  uint32_t last_updated_form_renderer_id_ = FormData::kNotSetFormRendererId;

  // Current state of Touch To Fill. This is reset during
  // CleanupOnDocumentShutdown.
  TouchToFillState touch_to_fill_state_ = TouchToFillState::kShouldShow;

  DISALLOW_COPY_AND_ASSIGN(PasswordAutofillAgent);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_AUTOFILL_AGENT_H_
