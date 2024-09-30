// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_AUTOFILL_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_AUTOFILL_AGENT_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/form_tracker.h"
#include "components/autofill/content/renderer/html_based_username_detector.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_input_element.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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

class FieldDataManager;
class RendererSavePasswordProgressLogger;
class PasswordGenerationAgent;

// This class is responsible for filling password forms.
// TODO(crbug.com/40281981): Remove FormTracker::Observer after launching
// kAutofillUnifyAndFixFormTracking.
class PasswordAutofillAgent : public content::RenderFrameObserver,
                              public FormTracker::Observer,
                              public mojom::PasswordAutofillAgent {
 public:
  using EnableHeavyFormDataScraping =
      base::StrongAlias<class EnableHeavyFormDataScrapingTag, bool>;
  using UseFallbackData = base::StrongAlias<class UseFallbackDataTag, bool>;

  PasswordAutofillAgent(
      content::RenderFrame* render_frame,
      blink::AssociatedInterfaceRegistry* registry,
      EnableHeavyFormDataScraping enable_heavy_form_data_scraping);

  PasswordAutofillAgent(const PasswordAutofillAgent&) = delete;
  PasswordAutofillAgent& operator=(const PasswordAutofillAgent&) = delete;

  ~PasswordAutofillAgent() override;

  // Must be called prior to calling other methods.
  void Init(AutofillAgent* autofill_agent);

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::PasswordAutofillAgent>
          pending_receiver);

  void SetPasswordGenerationAgent(PasswordGenerationAgent* generation_agent);

  // Callers should not store the returned value longer than a function scope.
  mojom::PasswordManagerDriver& GetPasswordManagerDriver();

  // mojom::PasswordAutofillAgent:
  void SetPasswordFillData(const PasswordFormFillData& form_data) override;
  void FillPasswordSuggestion(const std::u16string& username,
                              const std::u16string& password) override;
  void FillPasswordSuggestionById(FieldRendererId username_element_id,
                                  FieldRendererId password_element_id,
                                  const std::u16string& username,
                                  const std::u16string& password) override;
  void PreviewPasswordSuggestionById(FieldRendererId username_element_id,
                                     FieldRendererId password_element_id,
                                     const std::u16string& username,
                                     const std::u16string& password) override;
  void InformNoSavedCredentials(
      bool should_show_popup_without_passwords) override;
  void FillIntoFocusedField(bool is_password,
                            const std::u16string& credential) override;
  void PreviewField(FieldRendererId field_id,
                    const std::u16string& value) override;
  void FillField(FieldRendererId field_id,
                 const std::u16string& value) override;
  void SetLoggingState(bool active) override;
  void AnnotateFieldsWithParsingResult(
      const ParsingResult& parsing_result) override;
#if BUILDFLAG(IS_ANDROID)
  void KeyboardReplacingSurfaceClosed(bool show_virtual_keyboard) override;
  void TriggerFormSubmission() override;
#endif

  // FormTracker::Observer
  void OnProvisionallySaveForm(const blink::WebFormElement& form,
                               const blink::WebFormControlElement& element,
                               SaveFormReason source) override;
  void OnProbablyFormSubmitted() override;
  void OnFormSubmitted(const blink::WebFormElement& form) override;
  void OnInferredFormSubmission(mojom::SubmissionSource source) override;

  // WebLocalFrameClient editor related calls forwarded by AutofillAgent.
  // If they return true, it indicates the event was consumed and should not
  // be used for any other autofill activity.
  bool TextDidChangeInTextField(const blink::WebInputElement& element);

  // Called from AutofillAgent::UpdateStateForTextChange() to do
  // password-manager specific work.
  void UpdatePasswordStateForTextChange(const blink::WebInputElement& element);

  // Instructs `autofill_agent_` to track the autofilled `element`.
  void TrackAutofilledElement(const blink::WebFormControlElement& element);

  // Previews the username and password fields of this form with the given
  // values.
  void PreviewSuggestion(const blink::WebFormControlElement& node,
                         const std::u16string& username,
                         const std::u16string& password);

  // Clears all the previously previewed fields.
  void ClearPreviewedForm();

  // Sends a reputation check request in case if `element` has type password and
  // no check request were sent from this frame load.
  void MaybeCheckSafeBrowsingReputation(const blink::WebInputElement& element);

#if BUILDFLAG(IS_ANDROID)
  // Returns whether the soft keyboard should be suppressed.
  bool ShouldSuppressKeyboard();

  // Asks the agent to show the keyboard replacing surface for
  // `control_element`. Returns whether the agent was able to do so.
  bool TryToShowKeyboardReplacingSurface(
      const blink::WebFormControlElement& control_element);
#endif

  // Queries password suggestions for the given `element` and `trigger_source`.
  // If `generation_popup_showing` is true, this function will return false
  // as both UIs should not be shown at the same time. This function should
  // still be called in this situation so that UMA stats can be logged.
  // Returns true if any suggestions were shown, false otherwise.
  bool ShowSuggestions(const blink::WebInputElement& element,
                       AutofillSuggestionTriggerSource trigger_source);

  // Called when new form controls are inserted.
  void OnDynamicFormsSeen();

  // Called when the user interacts with the page after a load. This is a
  // signal to make autofilled values of password input elements accessible to
  // JavaScript.
  void UserGestureObserved();

  std::optional<FormData> GetFormDataFromWebForm(
      const blink::WebFormElement& web_form);

  std::optional<FormData> GetFormDataFromUnownedInputElements();

  // Notification that form element was cleared by HTMLFormElement::reset()
  // method. This can be used as a signal of a successful submission for change
  // password forms.
  void InformAboutFormClearing(const blink::WebFormElement& form);

  // Notification that input element was cleared by HTMLInputValue::SetValue()
  // method by setting an empty value. This can be used as a signal of a
  // successful submission for change password forms.
  void InformAboutFieldClearing(const blink::WebInputElement& element);

  bool logging_state_active() const { return logging_state_active_; }

  void FireHostSubmitEvent(FormRendererId form_id,
                           mojom::SubmissionSource source);

  // `form` and `input` are the elements user has just been interacting with
  // before the form save. `form` or `input` can be null but not both at the
  // same time. For example: if the form is unowned, `form` will be null; if the
  // user has submitted the form, `input` will be null.
  void InformBrowserAboutUserInput(const blink::WebFormElement& form,
                                   const blink::WebInputElement& input);

  // Determine whether the current frame is allowed to access the password
  // manager. For example, frames with about:blank documents or documents with
  // unique origins aren't allowed access.
  virtual bool FrameCanAccessPasswordManager();

  // RenderFrameObserver:
  void DidDispatchDOMContentLoadedEvent() override;
  void DidFinishLoad() override;
  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void OnDestruct() override;

  bool IsPrerendering() const;

  // Check if the given element is a username input field.
  bool IsUsernameInputField(const blink::WebInputElement& input_element) const;

  blink::WebFormControlElement last_queried_element() const {
    CHECK(autofill_agent_);
    return autofill_agent_->last_queried_element();
  }

  AutofillAgent& autofill_agent() { return *autofill_agent_; }

  // Notifies the driver about focusing the node.
  //
  // If `element` is null, notifies the password manager driver about removing
  // the focus from the currently focused node (with no setting it to a new
  // one).
  //
  // TODO: crbug.com/370301890 - Fire this in
  // RenderFrameObserver::FocusedElementChanged() and remove the plumbing from
  // AutofillAgent?
  void FocusedElementChangedWithCustomSemantics(
      const blink::WebElement& element,
      base::PassKey<AutofillAgent> pass_key) {
    focus_state_notifier_.FocusedElementChanged(element);
  }

 private:
  class DeferringPasswordManagerDriver;

  // This class ensures that the driver will only receive notifications only
  // when a focused field or its type (FocusedFieldType) change.
  class FocusStateNotifier {
   public:
    // Creates a new notifier that uses the agent which owns it to access the
    // real driver implementation.
    explicit FocusStateNotifier(PasswordAutofillAgent* agent);

    FocusStateNotifier(const FocusStateNotifier&) = delete;
    FocusStateNotifier& operator=(const FocusStateNotifier&) = delete;

    ~FocusStateNotifier();

    // Notifies the driver about focusing the node.
    void FocusedElementChanged(const blink::WebElement& element);

    // Notifies the password manager driver about removing the focus from the
    // currently focused node (with no setting it to a new one).
    void ResetFocus();

    mojom::FocusedFieldType GetFieldType(
        const blink::WebFormControlElement& node);
    void NotifyIfChanged(mojom::FocusedFieldType new_focused_field_type,
                         FieldRendererId new_focused_field_id);

    FieldRendererId focused_field_id_;
    mojom::FocusedFieldType focused_field_type_ =
        mojom::FocusedFieldType::kUnknown;
    const raw_ref<PasswordAutofillAgent> agent_;
  };

  // Enumeration representing possible keyboard replacing surface states. A
  // keyboard replacing surface can be either Touch To Fill UI or Android
  // Credential Manager UI. This is used to make sure that keyboard replacing
  // surface will only be shown in response to the first password form focus
  // during a frame's life time and to suppress the soft keyboard when
  // credential selector sheet is shown.
  enum class KeyboardReplacingSurfaceState {
    kShouldShow,
    kIsShowing,
    kWasShown,
  };

  struct PasswordInfo {
    FieldRef password_field;
    PasswordFormFillData fill_data;
    // The user accepted a suggestion from a dropdown on a password field.
    bool password_field_suggestion_was_accepted = false;
  };

  // Stores information about form field structure.
  struct FormFieldInfo {
    FieldRendererId renderer_id;
    autofill::FormControlType form_control_type;
    std::string autocomplete_attribute;
    bool is_focusable = false;
  };

  // Stores information about form structure.
  struct FormStructureInfo {
    FormStructureInfo();
    FormStructureInfo(const FormStructureInfo& other);
    FormStructureInfo& operator=(const FormStructureInfo& other);
    FormStructureInfo(FormStructureInfo&& other);
    FormStructureInfo& operator=(FormStructureInfo&& other);
    ~FormStructureInfo();

    FormRendererId renderer_id;
    std::vector<FormFieldInfo> fields;
  };

  // Stores information about fields previewed by this agent.
  struct PreviewInfo {
    FieldRendererId field_id;
    blink::WebAutofillState autofill_state =
        blink::WebAutofillState::kNotFilled;
    bool is_password = false;
  };

  // This class keeps track of autofilled username and password input elements
  // and ensures that the autofilled values are not accessible to JavaScript
  // code until the user interacts with the page. This restriction improves
  // privacy (crbug.com/798492) and reduces attack surface (crbug.com/777272).
  class PasswordValueGatekeeper {
   public:
    PasswordValueGatekeeper();

    PasswordValueGatekeeper(const PasswordValueGatekeeper&) = delete;
    PasswordValueGatekeeper& operator=(const PasswordValueGatekeeper&) = delete;

    ~PasswordValueGatekeeper();

    // Call this for every autofilled username and password field, so that
    // the gatekeeper protects the value accordingly.
    void RegisterElement(blink::WebInputElement element);

    // Call this to notify the gatekeeper that the user interacted with the
    // page.
    void OnUserGesture();

    // Call this to reset the gatekeeper on a new page navigation.
    void Reset();

   private:
    // Make the value of `element` accessible to JavaScript code.
    void ShowValue(blink::WebInputElement element);

    bool was_user_gesture_seen_;
    std::vector<FieldRef> elements_;
  };

  // Annotate `forms` and all fields in the current frame with form and field
  // signatures as HTML attributes. Used by
  // chrome://flags/#enable-show-autofill-signatures only.
  void AnnotateFormsAndFieldsWithSignatures(
      blink::WebVector<blink::WebFormElement>& forms);

  // Scans the given frame for password forms and sends them up to the browser.
  // If `only_visible` is true, only forms visible in the layout are sent.
  void SendPasswordForms(bool only_visible);

  // Performs necessary feasibility checks and triggers password suggestions
  // for the current domain on the `element`. `trigger_source` is used to
  // distinguish between the ways of how Autofill was triggered.
  bool ShowSuggestionsForDomain(const blink::WebInputElement& element,
                                AutofillSuggestionTriggerSource trigger_source);

  // Performs necessary feasibility checks and triggers manual fallback
  // suggestion on the provided `element`.
  bool ShowManualFallbackSuggestions(const blink::WebInputElement& element);

  // Instructs the browser to show a pop-up suggesting which credentials could
  // be filled. If the username exists, it should be passed as `user_input`. If
  // there is no username, pass the password field in `user_input`. In the
  // latter case, no username value will be shown in the pop-up.
  // Suggestion will be shown only on editable fields.
  void ShowSuggestionPopup(const std::u16string& typed_username,
                           const blink::WebInputElement& user_input,
                           AutofillSuggestionTriggerSource trigger_source);

  // Finds the PasswordInfo, username and password fields corresponding to the
  // passed in `element`, which can refer to either a username or a password
  // element. If a PasswordInfo was found, returns `true` and assigns the
  // corresponding username, password elements and PasswordInfo into
  // `username_element`, `password_element` and `pasword_info`, respectively.
  // Note, that `username_element->IsNull()` can be true if `element` is a
  // password. Callers have the chance to restrict the usage of fallback data
  // by setting `use_fallback_data` to false. In that case data provided via
  // MaybeStoreFallbackData will be ignored and the function returns early.
  bool FindPasswordInfoForElement(const blink::WebInputElement& element,
                                  UseFallbackData use_fallback_data,
                                  blink::WebInputElement* username_element,
                                  blink::WebInputElement* password_element,
                                  PasswordInfo** password_info);

  // Returns true if at least one of the found username or password field is
  // fillable.
  bool IsUsernameOrPasswordFillable(
      const blink::WebInputElement& username_element,
      const blink::WebInputElement& password_element,
      PasswordInfo* password_info);

  // Finds the PasswordInfo, username and password fields corresponding to the
  // passed in `element` and returns true if there is at least one field that
  // can be filled by a password suggestion.
  bool HasElementsToFill(const blink::WebInputElement& trigger_element,
                         UseFallbackData use_fallback_data,
                         blink::WebInputElement* username_element,
                         blink::WebInputElement* password_element,
                         PasswordInfo** password_info);

  // Cleans up the state when document is shut down, e.g. when committing a new
  // document or closing the frame.
  void CleanupOnDocumentShutdown();

  // Sets suggested value of the `input` to `credential`. Persists the
  // information about `input` to clear the previewed value in the future.
  void DoPreviewField(blink::WebInputElement input,
                      const std::u16string& credential,
                      bool is_password);

  // Checks that a given input field is valid before filling the given `input`
  // with the given `credential` and marking the field as auto-filled.
  void DoFillField(blink::WebInputElement input,
                   const std::u16string& credential);

  // Given `username_element` and `password_element`, previews `username` and
  // `password` respectively into them.
  void PreviewUsernameAndPasswordElements(
      blink::WebInputElement username_element,
      blink::WebInputElement password_element,
      const std::u16string& username,
      const std::u16string& password);

  // Given `username_element` and `password_element`, fills `username` and
  // `password` respectively into them.
  void FillUsernameAndPasswordElements(blink::WebInputElement username_element,
                                       blink::WebInputElement password_element,
                                       const std::u16string& username,
                                       const std::u16string& password);

  // Uses `FillField` to fill the given `credential` into the `password_input`.
  // Saves the password for its associated form.
  void FillPasswordFieldAndSave(blink::WebInputElement password_input,
                                const std::u16string& credential);

  // This function attempts to fill `username_element` and `password_element`
  // with values from `fill_data`. The `username_element` and `password_element`
  // will only have the suggestedValue set. If a match is found, return true and
  // Returns true if the password is filled.
  bool FillUserNameAndPassword(blink::WebInputElement username_element,
                               blink::WebInputElement password_element,
                               const PasswordFormFillData& fill_data,
                               RendererSavePasswordProgressLogger* logger);

  // Logs whether a username value that was prefilled by the website was
  // overridden when trying to fill with an existing credential. This logs
  // only one value per `PasswordAutofillAgent` instance.
  void LogPrefilledUsernameFillOutcome(PrefilledUsernameFillOutcome outcome);

  void HidePopup();

  // Returns pair(username_element, password_element) based on renderer ids from
  // `username_field` and `password_field` from `form_data`.
  std::pair<blink::WebInputElement, blink::WebInputElement>
  FindUsernamePasswordElements(const PasswordFormFillData& form_data);

  // Populates `web_input_to_password_info_` and `password_to_username_` in
  // order to provide fill on account select on `username_element` and
  // `password_element` with credentials from `form_data`.
  void StoreDataForFillOnAccountSelect(const PasswordFormFillData& form_data,
                                       blink::WebInputElement username_element,
                                       blink::WebInputElement password_element);

  // In case when `web_input_to_password_info_` is empty (i.e. no fill on
  // account select data yet) this function populates
  // `web_input_to_password_info_` in order to provide fill on account select on
  // any password field (aka filling fallback) with credentials from
  // `form_data`.
  void MaybeStoreFallbackData(const PasswordFormFillData& form_data);

  // Records whether filling succeeded for the first attempt to fill on a site.
  // The logging is a bit conservative: It is possible that user-perceived
  // navigations (via dynamic HTML sites) not trigger any actual navigations
  // and therefore, the `recorded_first_filling_result_` never gets reset.
  void LogFirstFillingResult(const PasswordFormFillData& form_data,
                             FillingResult result);

  // Extracts information about form structure.
  static FormStructureInfo ExtractFormStructureInfo(const FormData& form_data);
  // Checks whether the form structure (amount of elements, element types etc)
  // was changed.
  bool WasFormStructureChanged(const FormStructureInfo& form_data) const;
  // Tries to restore `control_elements` values with cached values.
  void TryFixAutofilledForm(
      std::vector<blink::WebFormControlElement>& control_elements) const;

  // Autofills `field` with `value` and updates `gatekeeper_`,
  // `field_data_manager_`, `autofilled_elements_cache_`. `field` should be
  // non-null.
  void AutofillField(const std::u16string& value, blink::WebInputElement field);

  void SetLastUpdatedFormAndField(const blink::WebFormElement& form,
                                  const blink::WebFormControlElement& input);

  bool CanShowPopupWithoutPasswords(
      const blink::WebInputElement& password_element) const;

  // Returns true if the element is of type 'password' and has either user typed
  // input or input autofilled on user trigger.
  bool IsPasswordFieldFilledByUser(
      const blink::WebFormControlElement& element) const;

  // Extracts and sends the form data of `cleared_form` to PasswordManager.
  void NotifyPasswordManagerAboutClearedForm(
      const blink::WebFormElement& cleared_form);

  // Notifies the PasswordManager about a field modification.
  void NotifyPasswordManagerAboutFieldModification(
      const blink::WebInputElement& element);

  // Shows suggestions on the focused element if it was focused before the form
  // was processed by the password manager.
  void MaybeTriggerSuggestionsOnFocusedElement(
      const blink::WebInputElement& username_element,
      const blink::WebInputElement& password_element);

  FieldDataManager& field_data_manager() const {
    return autofill_agent_->field_data_manager();
  }

  // Controls heavy scraping of form data (e.g., button titles for unowned
  // forms) is enabled.
  EnableHeavyFormDataScraping enable_heavy_form_data_scraping_;

  // A map from WebInput elements to `PasswordInfo` for all elements that
  // password manager has fill information for.
  //
  // After any mutation, `last_supplied_password_info_iter_` must be updated.
  std::map<FieldRef, PasswordInfo> web_input_to_password_info_;

  // A (sort-of) reverse map to `web_input_to_password_info_`.
  std::map<FieldRef, FieldRef> password_to_username_;

  // The chronologically last insertion into `web_input_to_password_info_`.
  // This iterator always points to `web_input_to_password_info_`.
  std::map<FieldRef, PasswordInfo>::iterator last_supplied_password_info_iter_ =
      web_input_to_password_info_.end();

  // Set of fields that are reliably identified as non-credential fields.
  base::flat_set<FieldRendererId> suggestion_banned_fields_;

  bool should_show_popup_without_passwords_ = false;

  PasswordValueGatekeeper gatekeeper_;

  // True indicates that user debug information should be logged.
  bool logging_state_active_ = false;

  std::vector<PreviewInfo> previewed_elements_;

  // True indicates that a request for credentials has been sent to the store.
  bool sent_request_to_store_ = false;

  // True indicates that a safe browsing reputation check has been triggered.
  bool checked_safe_browsing_reputation_ = false;

  raw_ptr<AutofillAgent> autofill_agent_ = nullptr;

  raw_ptr<PasswordGenerationAgent> password_generation_agent_ = nullptr;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  PagePasswordsAnalyser page_passwords_analyser_;
#endif

  mojo::AssociatedRemote<mojom::PasswordManagerDriver> password_manager_driver_;

  // Used for deferring messages while prerendering.
  std::unique_ptr<DeferringPasswordManagerDriver>
      deferring_password_manager_driver_;

  mojo::AssociatedReceiver<mojom::PasswordAutofillAgent> receiver_{this};

  bool prefilled_username_metrics_logged_ = false;

  // Keeps autofilled values for the form elements until a user gesture
  // is observed. At that point, the map is cleared.
  std::map<FieldRendererId, blink::WebString> autofilled_elements_cache_;
  base::flat_set<FieldRendererId> all_autofilled_elements_;
  // Keeps forms structure (amount of elements, element types etc).
  // TODO(crbug.com/41422255): It's too expensive to keep the whole FormData
  // structure. Replace FormData with a smaller structure.
  std::map<FormRendererId, FormStructureInfo> forms_structure_cache_;

  // The HTML based username detector's cache which maps form elements to
  // username predictions.
  UsernameDetectorCache username_detector_cache_;

  // Stores the mapping from a form element's ID to results of button titles
  // heuristics for that form.
  form_util::ButtonTitlesCache button_titles_cache_;

  // Flag to prevent that multiple PasswordManager.FirstRendererFillingResult
  // UMA metrics are recorded per page load. This is reset on
  // DidCommitProvisionalLoad() but only for non-same-document-navigations.
  bool recorded_first_filling_result_ = false;

  // Contains render id of the field where a form submission should be
  // triggered.
  FieldRendererId field_renderer_id_to_submit_;

  // Tracks how many times PasswordFormFillData was received from the browser
  // for every form.
  // Can be used to estimate how many times forms are actually reparsed
  // during their lifetime.
  std::map<FormRendererId, size_t> times_received_fill_data_;

  // This notifier is used to avoid sending redundant messages to the password
  // manager driver mojo interface.
  FocusStateNotifier focus_state_notifier_{this};

#if BUILDFLAG(IS_ANDROID)
  // Current state of the keyboard replacing surface. This is reset during
  // CleanupOnDocumentShutdown.
  KeyboardReplacingSurfaceState keyboard_replacing_surface_state_ =
      KeyboardReplacingSurfaceState::kShouldShow;
#endif
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_AUTOFILL_AGENT_H_
