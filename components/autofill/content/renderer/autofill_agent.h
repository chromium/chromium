// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_H_

#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/form_tracker.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "ui/accessibility/ax_mode.h"

namespace blink {
class WebNode;
class WebView;
class WebFormControlElement;
class WebFormElement;
}  // namespace blink

namespace autofill {

struct FormData;
class FormCache;
class PasswordAutofillAgent;
class PasswordGenerationAgent;
class FieldDataManager;

// AutofillAgent deals with Autofill related communications between WebKit and
// the browser.
//
// Each AutofillAgent is associated with exactly one RenderFrame and
// communicates with exactly one ContentAutofillDriver throughout its entire
// lifetime.
//
// AutofillAgent is deleted asynchronously because it may itself take action
// that (via JavaScript) causes the associated RenderFrame's deletion.
// AutofillAgent is pending deletion between OnDestruct() and ~AutofillAgent().
// To handle this state, care must be taken to check for nullptrs:
// - `unsafe_autofill_driver()`
// - `unsafe_render_frame()`
// - `form_cache_`
//
// This RenderFrame owns all forms and fields in the renderer-browser
// communication:
// - AutofillAgent may assume that forms and fields received in the
//   mojom::AutofillAgent events are owned by that RenderFrame.
// - Conversely, the forms and fields which AutofillAgent passes to
//   mojom::AutofillDriver events must be owned by that RenderFrame.
//
// Note that Autofill encompasses:
// - single text field suggestions, that we usually refer to as Autocomplete,
// - password form fill, refered to as Password Autofill, and
// - entire form fill based on one field entry, referred to as Form Autofill.
class AutofillAgent : public content::RenderFrameObserver,
                      public FormTracker::Observer,
                      public blink::WebAutofillClient,
                      public mojom::AutofillAgent {
 public:
  // PasswordAutofillAgent is guaranteed to outlive AutofillAgent.
  // PasswordGenerationAgent and AutofillAssistantAgent may be nullptr. If they
  // are not, then they are also guaranteed to outlive AutofillAgent.
  AutofillAgent(content::RenderFrame* render_frame,
                PasswordAutofillAgent* password_autofill_agent,
                PasswordGenerationAgent* password_generation_agent,
                blink::AssociatedInterfaceRegistry* registry);

  AutofillAgent(const AutofillAgent&) = delete;
  AutofillAgent& operator=(const AutofillAgent&) = delete;

  ~AutofillAgent() override;

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::AutofillAgent> pending_receiver);

  // Callers must not store the returned value longer than a function scope.
  // unsafe_autofill_driver() is nullptr if unsafe_render_frame() is nullptr and
  // the `autofill_driver_` has not been bound yet.
  mojom::AutofillDriver* unsafe_autofill_driver();
  mojom::PasswordManagerDriver& GetPasswordManagerDriver();

  // mojom::AutofillAgent:
  void TriggerReparse() override;
  void TriggerReparseWithResponse(
      base::OnceCallback<void(bool)> callback) override;
  void FillOrPreviewForm(const FormData& form,
                         mojom::RendererFormDataAction action) override;
  void FieldTypePredictionsAvailable(
      const std::vector<FormDataPredictions>& forms) override;
  void ClearSection() override;
  void ClearPreviewedForm() override;
  void FillFieldWithValue(FieldRendererId field_id,
                          const std::u16string& value) override;
  void PreviewFieldWithValue(FieldRendererId field_id,
                             const std::u16string& value) override;
  void SetSuggestionAvailability(FieldRendererId field_id,
                                 const mojom::AutofillState state) override;
  void AcceptDataListSuggestion(FieldRendererId field_id,
                                const std::u16string& suggested_value) override;
  void FillPasswordSuggestion(const std::u16string& username,
                              const std::u16string& password) override;
  void PreviewPasswordSuggestion(const std::u16string& username,
                                 const std::u16string& password) override;
  void PreviewPasswordGenerationSuggestion(
      const std::u16string& password) override;
  void SetUserGestureRequired(bool required) override;
  void SetSecureContextRequired(bool required) override;
  void SetFocusRequiresScroll(bool require) override;
  void SetQueryPasswordSuggestion(bool required) override;
  void EnableHeavyFormDataScraping() override;
  void SetFieldsEligibleForManualFilling(
      const std::vector<FieldRendererId>& fields) override;
  void GetPotentialLastFourCombinationsForStandaloneCvc(
      base::OnceCallback<void(const std::vector<std::string>&)>
          potential_matches) override;

  void FormControlElementClicked(const blink::WebFormControlElement& element);

  base::WeakPtr<AutofillAgent> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // FormTracker::Observer
  void OnProvisionallySaveForm(const blink::WebFormElement& form,
                               const blink::WebFormControlElement& element,
                               ElementChangeSource source) override;
  void OnProbablyFormSubmitted() override;
  void OnFormSubmitted(const blink::WebFormElement& form) override;
  void OnInferredFormSubmission(mojom::SubmissionSource source) override;

  void AddFormObserver(Observer* observer);
  void RemoveFormObserver(Observer* observer);

  // Instructs `form_tracker_` to track the autofilled `element`.
  void TrackAutofilledElement(const blink::WebFormControlElement& element);

  FormTracker* form_tracker_for_testing() { return &form_tracker_; }

  bool is_heavy_form_data_scraping_enabled() {
    return is_heavy_form_data_scraping_enabled_;
  }

  bool IsPrerendering() const;

  const blink::WebFormControlElement& focused_element() const {
    return element_;
  }

 protected:
  // blink::WebAutofillClient:
  void DidAddOrRemoveFormRelatedElementsDynamically() override;

 private:
  class DeferringAutofillDriver;
  friend class FormControlClickDetectionTest;
  friend class AutofillAgentTestApi;

  // Flags passed to ShowSuggestions.
  struct ShowSuggestionsOptions {
    // Specifies that suggestions should be shown when |element| contains no
    // text.
    bool autofill_on_empty_values{false};

    // Specifies that suggestions should be shown when the caret is not
    // after the last character in the element.
    bool requires_caret_at_end{false};

    // Specifies that all autofill suggestions should be shown and none should
    // be elided because of the current value of |element| (relevant for inline
    // autocomplete).
    bool show_full_suggestion_list{false};

    // Specifies that the first suggestion must be auto-selected when the
    // dropdown is shown. Enabled when the user presses ARROW_DOWN on a field.
    AutoselectFirstSuggestion autoselect_first_suggestion{false};

    // Signals that suggestions are triggered due to a click on an input
    // element. The signal is used to understand whether other surfaces (e.g.
    // TouchToFill, FastCheckout) can be triggered.
    FormElementWasClicked form_element_was_clicked{false};
  };

  // This class ensures that the driver will only receive notifications only
  // when a focused field or its type (FocusedFieldType) change.
  class FocusStateNotifier {
   public:
    // Creates a new notifier that uses the agent which owns it to access the
    // real driver implementation.
    explicit FocusStateNotifier(AutofillAgent* agent);

    FocusStateNotifier(const FocusStateNotifier&) = delete;
    FocusStateNotifier& operator=(const FocusStateNotifier&) = delete;

    ~FocusStateNotifier();

    // Notifies the driver about focusing the node.
    void FocusedInputChanged(const blink::WebNode& node);
    // Notifies the password manager driver about removing the focus from the
    // currently focused node (with no setting it to a new one).
    void ResetFocus();

   private:
    mojom::FocusedFieldType GetFieldType(
        const blink::WebFormControlElement& node);
    void NotifyIfChanged(mojom::FocusedFieldType new_focused_field_type,
                         FieldRendererId new_focused_field_id);

    FieldRendererId focused_field_id_;
    mojom::FocusedFieldType focused_field_type_ =
        mojom::FocusedFieldType::kUnknown;
    AutofillAgent& agent_;
  };

  // content::RenderFrameObserver:
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void DidDispatchDOMContentLoadedEvent() override;
  void DidChangeScrollOffset() override;
  void FocusedElementChanged(const blink::WebElement& element) override;
  void AccessibilityModeChanged(const ui::AXMode& mode) override;
  void OnDestruct() override;

  // The RenderFrame* is nullptr while the AutofillAgent is pending deletion,
  // between OnDestruct() and ~AutofillAgent().
  content::RenderFrame* unsafe_render_frame() const {
    return content::RenderFrameObserver::render_frame();
  }

  // Use unsafe_render_frame() instead.
  template <typename T = int>
  content::RenderFrame* render_frame(T* = 0) const {
    static_assert(
        std::is_void_v<T>,
        "Beware that the RenderFrame may become nullptr by OnDestruct() "
        "because AutofillAgent destructs itself asynchronously. Use "
        "unsafe_render_frame() instead and make test that it is non-nullptr.");
  }

  // Fires Mojo messages for a given form submission.
  void FireHostSubmitEvents(const blink::WebFormElement& form,
                            bool known_success,
                            mojom::SubmissionSource source);
  void FireHostSubmitEvents(const FormData& form_data,
                            bool known_success,
                            mojom::SubmissionSource source);

  // blink::WebAutofillClient:
  void TextFieldDidEndEditing(const blink::WebInputElement& element) override;
  void TextFieldDidChange(const blink::WebFormControlElement& element) override;
  void TextFieldDidReceiveKeyDown(
      const blink::WebInputElement& element,
      const blink::WebKeyboardEvent& event) override;
  void OpenTextDataListChooser(const blink::WebInputElement& element) override;
  void DataListOptionsChanged(const blink::WebInputElement& element) override;
  void UserGestureObserved() override;
  void AjaxSucceeded() override;
  void JavaScriptChangedAutofilledValue(
      const blink::WebFormControlElement& element,
      const blink::WebString& old_value) override;
  void DidCompleteFocusChangeInFrame() override;
  void DidReceiveLeftMouseDownOrGestureTapInNode(
      const blink::WebNode& node) override;
  void SelectFieldOptionsChanged(
      const blink::WebFormControlElement& element) override;
  void SelectControlDidChange(
      const blink::WebFormControlElement& element) override;
  bool ShouldSuppressKeyboard(
      const blink::WebFormControlElement& element) override;
  void FormElementReset(const blink::WebFormElement& form) override;
  void PasswordFieldReset(const blink::WebInputElement& element) override;
  std::vector<blink::WebAutofillClient::FormIssue>
  ProccessFormsAndReturnIssues() override;

  void HandleFocusChangeComplete();
  void SendFocusedInputChangedNotificationToBrowser(
      const blink::WebElement& node);

  // Helper method which collects unowned elements (i.e., those not inside a
  // form tag) and writes them into |output|. Returns true if the process is
  // successful, and all conditions for firing events are true.
  bool CollectFormlessElements(FormData* output) const;
  FRIEND_TEST_ALL_PREFIXES(FormAutocompleteTest, CollectFormlessElements);

  void OnTextFieldDidChange(const blink::WebInputElement& element);
  void DidChangeScrollOffsetImpl(const blink::WebFormControlElement& element);

  // Shows the autofill suggestions for |element|. This call is asynchronous
  // and may or may not lead to the showing of a suggestion popup (no popup is
  // shown if there are no available suggestions).
  void ShowSuggestions(const blink::WebFormControlElement& element,
                       const ShowSuggestionsOptions& options);

  // Queries the browser for Autocomplete and Autofill suggestions for the given
  // |element|.
  void QueryAutofillSuggestions(
      const blink::WebFormControlElement& element,
      AutoselectFirstSuggestion autoselect_first_suggestion,
      FormElementWasClicked form_element_was_clicked);

  // Sets the selected value of the the field identified by |field_id| to
  // |suggested_value|.
  void DoAcceptDataListSuggestion(FieldRendererId field_id,
                                  const std::u16string& suggested_value);

  // Set `element` to display the given `value`.
  void DoFillFieldWithValue(const std::u16string& value,
                            blink::WebFormControlElement& element,
                            blink::WebAutofillState autofill_state);

  // Set |node| to display the given |value| as a preview.  The preview is
  // visible on screen to the user, but not visible to the page via the DOM or
  // JavaScript.
  void DoPreviewFieldWithValue(const std::u16string& value,
                               blink::WebInputElement& node);

  // Notifies the AutofillDriver in the browser process of new and/or removed
  // forms, modulo throttling.
  //
  // Throttling means that the actual work -- that is, extracting the forms and
  // invoking AutofillDriver::FormsSeen() -- is delayed by (at least) 100 ms.
  // All subsequent calls within the next (at least) 100 ms return early.
  //
  // Calls `callback(true)` asynchronously after the timer is completed.
  // Otherwise, calls `callback(false)` immediately.
  void ProcessForms(base::OneShotTimer& timer,
                    base::OnceCallback<void(bool)> callback);

  // Extracts new and/or removed forms and triggers AutofillDriver::FormsSeen().
  void ProcessFormsUnthrottled(base::OnceCallback<void(bool)> callback);

  // Hides any currently showing Autofill popup.
  void HidePopup();

  // Attempt to get submitted FormData from last_interacted_form_ or
  // provisionally_saved_form_, return true if |form| is set.
  absl::optional<FormData> GetSubmittedForm() const;

  // Pushes the value of GetSubmittedForm() to the AutofillDriver.
  void SendPotentiallySubmittedFormToBrowser();

  void ResetLastInteractedElements();
  void UpdateLastInteractedForm(const blink::WebFormElement& form);

  // Called when current form is no longer submittable, submitted_forms_ is
  // cleared in this method.
  void OnFormNoLongerSubmittable();

  // Trigger a refill if the `form` has just changed dynamically (other than the
  // field values). The refill is triggered by informing the browser process
  // about the form. The browser process makes the final decision whether or not
  // to execute a refill.
  void TriggerRefillIfNeeded(const FormData& form);

  // Helpers for SelectFieldOptionsChanged() and DataListOptionsChanged(), which
  // get called after a timer that is restarted when another event of the same
  // type started.
  void BatchSelectOptionChange(const blink::WebFormControlElement& element);
  void BatchDataListOptionChange(const blink::WebFormControlElement& element);

  // Return the next web node of `current_node` in the DOM. `next` determines
  // the direction to traverse in.
  blink::WebNode NextWebNode(const blink::WebNode& current_node, bool next);

  // Contains the form of the document. Does not survive navigations and is
  // reset when the AutofillAgent is pending deletion.
  std::unique_ptr<FormCache> form_cache_;

  PasswordAutofillAgent* password_autofill_agent_;      // Weak reference.
  PasswordGenerationAgent* password_generation_agent_;  // Weak reference.

  // The element corresponding to the last request sent for form field Autofill.
  blink::WebFormControlElement element_;

  // The elements that currently are being previewed.
  std::vector<blink::WebFormControlElement> previewed_elements_;

  // Last form which was interacted with by the user.
  blink::WebFormElement last_interacted_form_;

  // When dealing with an unowned form, we keep track of the unowned fields
  // the user has modified so we can determine when submission occurs.
  // An additional sufficient condition for the form submission detection is
  // that the form has been autofilled.
  std::set<FieldRendererId> formless_elements_user_edited_;
  bool formless_elements_were_autofilled_ = false;

  // The form the user interacted with last. It is used if last_interacted_form_
  // or a formless form can't be converted to FormData at the time of form
  // submission (e.g. because they have been removed from the DOM).
  absl::optional<FormData> provisionally_saved_form_;

  // Keeps track of the forms for which form submitted event has been sent to
  // AutofillDriver. We use it to avoid fire duplicated submission event when
  // WILL_SEND_SUBMIT_EVENT and form submitted are both fired for same form.
  // The submitted_forms_ is cleared when we know no more submission could
  // happen for that form.
  std::set<FormRendererId> submitted_forms_;

  // The query node autofill state prior to previewing the form.
  blink::WebAutofillState query_node_autofill_state_;

  // Whether the Autofill popup is possibly visible.  This is tracked as a
  // performance improvement, so that the IPC channel isn't flooded with
  // messages to close the Autofill popup when it can't possibly be showing.
  bool is_popup_possibly_visible_;

  // If the generation popup is possibly visible. This is tracked to prevent
  // generation UI from displaying at the same time as password manager UI.
  // This is needed because generation is shown on field focus vs. field click
  // for the password manager. TODO(gcasto): Have both UIs show on focus.
  bool is_generation_popup_possibly_visible_;

  // Whether or not a user gesture is required before notification of a text
  // field change. Default to true.
  bool is_user_gesture_required_;

  // Whether or not the secure context is required to query autofill suggestion.
  // Default to false.
  bool is_secure_context_required_;

  // This flag denotes whether or not password suggestions need to be
  // programatically queried. This is needed on Android WebView because it
  // doesn't use PasswordAutofillAgent to handle password form.
  bool query_password_suggestion_ = false;

  bool focused_node_was_last_clicked_ = false;
  FieldRendererId last_clicked_form_control_element_for_testing_;

  FormTracker form_tracker_;

  // Whether or not we delay focus handling until scrolling occurs.
  bool focus_requires_scroll_ = true;

  mojo::AssociatedReceiver<mojom::AutofillAgent> receiver_{this};

  mojo::AssociatedRemote<mojom::AutofillDriver> autofill_driver_;

  // For deferring messages to the browser process while prerendering.
  std::unique_ptr<DeferringAutofillDriver> deferring_autofill_driver_;

  bool was_last_action_fill_ = false;

  // Timers for throttling handling of frequent events.
  base::OneShotTimer select_option_change_batch_timer_;
  base::OneShotTimer datalist_option_change_batch_timer_;
  // TODO(crbug.com/1444566): Merge some or all of these timers?
  base::OneShotTimer process_forms_after_dynamic_change_timer_;
  base::OneShotTimer process_forms_reparse_timer_;
  base::OneShotTimer process_forms_reparse_with_response_timer_;

  // True iff DidDispatchDOMContentLoadedEvent() fired.
  bool is_dom_content_loaded_ = false;

  // Will be set when accessibility mode changes, depending on what the new mode
  // is.
  bool is_screen_reader_enabled_ = false;

  // Whether agents should enable heavy scraping of form data (e.g., button
  // titles for unowned forms).
  bool is_heavy_form_data_scraping_enabled_ = false;

  const scoped_refptr<FieldDataManager> field_data_manager_;

  // This notifier is used to avoid sending redundant messages to the password
  // manager driver mojo interface.
  FocusStateNotifier focus_state_notifier_;

  base::WeakPtrFactory<AutofillAgent> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_H_
