// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/optional_ref.h"
#include "base/types/strong_alias.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/form_cache.h"
#include "components/autofill/content/renderer/form_tracker.h"
#include "components/autofill/content/renderer/timing.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_autofill_state.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "ui/accessibility/ax_mode.h"

namespace blink {
class WebFormControlElement;
class WebFormElement;
}  // namespace blink

namespace autofill {

class PasswordAutofillAgent;
class PasswordGenerationAgent;

// AutofillAgent deals with Autofill related communications between Blink and
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
// - `GetDocument()`
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
// - password form fill, referred to as Password Autofill, and
// - entire form fill based on one field entry, referred to as Form Autofill.
class AutofillAgent : public content::RenderFrameObserver,
                      public blink::WebAutofillClient,
                      public mojom::AutofillAgent {
 public:
  static constexpr base::TimeDelta kFormsSeenThrottle = base::Milliseconds(100);

  using ExtractAllDatalists =
      base::StrongAlias<class ExtractAllDatalistsTag, bool>;
  using FocusRequiresScroll =
      base::StrongAlias<class FocusRequiresScrollTag, bool>;
  using QueryPasswordSuggestions =
      base::StrongAlias<class QueryPasswordSuggestionsTag, bool>;
  using SecureContextRequired =
      base::StrongAlias<class SecureContextRequiredTag, bool>;
  using UserGestureRequired = FormTracker::UserGestureRequired;
  using UsesKeyboardAccessoryForSuggestions =
      base::StrongAlias<class UsesKeyboardAccessoryForSuggestionsTag, bool>;

  struct Config {
    // Controls whether or not all datalists shall be extracted into
    // FormFieldData. This feature is enabled when all datalists (instead of
    // only the focused one) shall be extracted and sent to the Android Autofill
    // service when the autofill session is created.
    ExtractAllDatalists extract_all_datalists{false};

    // Controls whether to delay focus handling until scrolling occurs.
    FocusRequiresScroll focus_requires_scroll{true};

    // Controls whether password suggestions are queried programmatically. This
    // is required if the `PasswordAutofillAgent` does not handle password
    // forms and `AutofillDriver` should be informed instead.
    QueryPasswordSuggestions query_password_suggestions{false};

    // Controls whether a secure context is required to query Autofill
    // suggestions.
    SecureContextRequired secure_context_required{false};

    // Controls whether `FormTracker` requires a user gesture in order to pass
    // on information about text field change events to `AutofillAgent`.
    // Bypassing the user gesture check may be required when delegating to
    // Android Autofill, which needs to be notified of every change to the
    // field.
    UserGestureRequired user_gesture_required{true};

    // Is true iff the platform doesn't show any popups but renders the same
    // information in or near the keyboard instead.
    UsesKeyboardAccessoryForSuggestions uses_keyboard_accessory_for_suggestions{
        BUILDFLAG(IS_ANDROID)};
  };

  // PasswordAutofillAgent is guaranteed to outlive AutofillAgent.
  // PasswordGenerationAgent and AutofillAssistantAgent may be nullptr. If they
  // are not, then they are also guaranteed to outlive AutofillAgent.
  AutofillAgent(
      content::RenderFrame* render_frame,
      std::unique_ptr<PasswordAutofillAgent> password_autofill_agent,
      std::unique_ptr<PasswordGenerationAgent> password_generation_agent,
      blink::AssociatedInterfaceRegistry* registry);

  AutofillAgent(const AutofillAgent&) = delete;
  AutofillAgent& operator=(const AutofillAgent&) = delete;

  ~AutofillAgent() override;

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::AutofillAgent> pending_receiver);

  blink::WebDocument GetDocument() const;

  // Callers must not store the returned value longer than a function scope.
  // unsafe_autofill_driver() is nullptr if unsafe_render_frame() is nullptr and
  // the `autofill_driver_` has not been bound yet.
  mojom::AutofillDriver* unsafe_autofill_driver();

  CallTimerState GetCallTimerState(CallTimerState::CallSite call_site) const;

  // mojom::AutofillAgent:
  void TriggerFormExtraction() override;
  void TriggerFormExtractionWithResponse(
      base::OnceCallback<void(bool)> callback) override;
  void ApplyFieldsAction(
      mojom::FormActionType action_type,
      mojom::ActionPersistence action_persistence,
      const std::vector<FormFieldData::FillData>& fields) override;
  void ApplyFieldAction(mojom::FieldActionType action_type,
                        mojom::ActionPersistence action_persistence,
                        FieldRendererId field_id,
                        const std::u16string& value) override;
  void ExtractFormWithField(
      FieldRendererId field_id,
      base::OnceCallback<void(const std::optional<FormData>&)> callback)
      override;
  void ExtractLabeledTextNodeValue(
      const std::u16string& value_regex,
      const std::u16string& label_regex,
      uint32_t number_of_ancestor_levels_to_search,
      base::OnceCallback<void(const std::string&)> callback) override;

  void ExposeDomNodeIds() override;
  void FieldTypePredictionsAvailable(
      const std::vector<FormDataPredictions>& forms) override;
  // Besides cases that "actually" clear the form, this function needs to be
  // called before all filling operations. This is because filled fields are no
  // longer considered previewed - and any state tied to the preview needs to
  // be reset.
  void ClearPreviewedForm() override;
  void TriggerSuggestions(
      FieldRendererId field_id,
      AutofillSuggestionTriggerSource trigger_source) override;
  void SetSuggestionAvailability(
      FieldRendererId field_id,
      mojom::AutofillSuggestionAvailability suggestion_availability) override;
  void AcceptDataListSuggestion(FieldRendererId field_id,
                                const std::u16string& suggested_value) override;
  void PreviewPasswordSuggestion(const std::u16string& username,
                                 const std::u16string& password) override;
  void PreviewPasswordGenerationSuggestion(
      const std::u16string& password) override;
  void GetPotentialLastFourCombinationsForStandaloneCvc(
      base::OnceCallback<void(const std::vector<std::string>&)>
          potential_matches) override;
  void DispatchEmailVerifiedEvent(
      FieldRendererId field_id,
      const std::string& presentation_token) override;

  // Fires Mojo messages for a given form submission.
  void FireHostSubmitEvents(const FormData& form_data,
                            mojom::SubmissionSource source);

  // Instructs `form_tracker_` to track the autofilled `element`.
  void TrackAutofilledElement(const blink::WebFormControlElement& element);

  // Function that should be called whenever the value of `element` changes due
  // to user input. This is separate from OnTextFieldValueChanged() as that
  // function may trigger UI and should only be called when other UI won't be
  // shown. `form_cache` can be used to optimize form extractions occurring
  // synchronously after this function call.
  void UpdateStateForTextChange(const blink::WebFormControlElement& element,
                                FieldPropertiesFlags flag,
                                const SynchronousFormCache& form_cache);

  // TODO(crbug.com/376628389): Remove.
  void OnTextFieldValueChanged(const blink::WebFormControlElement& element,
                               const SynchronousFormCache& form_cache);
  void OnSelectControlSelectionChanged(
      const blink::WebFormControlElement& element,
      const SynchronousFormCache& form_cache);

  bool IsPrerendering() const;

  blink::WebFormControlElement last_queried_element() const {
    return last_queried_element_.GetField();
  }

  FieldDataManager& field_data_manager() const {
    return *field_data_manager_.get();
  }

  form_util::ButtonTitlesCache* button_titles_cache() {
    return &button_titles_cache_;
  }

 protected:
  // blink::WebAutofillClient:

  // Signals from blink that a form related element changed dynamically,
  // passing the changed element as well as the type of the change.
  // TODO(crbug.com/40281981): Fire the signal for elements that become hidden.
  void DidChangeFormRelatedElementDynamically(
      const blink::WebElement&,
      blink::WebFormRelatedChangeType) override;

  // content::RenderFrameObserver:

  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void DidCreateDocumentElement() override;
  void DidDispatchDOMContentLoadedEvent() override;
  void DidChangeScrollOffset() override;
  void AccessibilityModeChanged(const ui::AXMode& mode) override;
  void OnDestruct() override;

  // This function fires `FocusOnFormField()` xor `FocusOnNonFormField()`:
  // - It calls `FocusOnFormField()` iff the newly focused element is a non-null
  //   `WebFormControlElement` or a non-null contenteditable whose `FormData`
  //   can be extracted.
  // - It calls `FocusOnNonFormField()` iff it does not call
  //   `FocusOnFormField()`.
  // See crbug.com/337690061 for details.
  void FocusedElementChanged(
      const blink::WebElement& new_focused_element) override;

 private:
  class DeferringAutofillDriver;
  friend class AutofillAgentTestApi;

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

  // To be called when all forms are irretrievably gone, e.g., when a new
  // document is loaded.
  void Reset();

  // Tries to show the given `passwords_request` for the given fields and update
  // `is_popup_possibly_visible` accordingly. Returns true if the password agent
  // handles the request.
  bool TryShowPasswordSuggestions(
      const blink::WebInputElement& input,
      IsPasswordRequestManuallyTriggered manually_triggered_password_request,
      base::optional_ref<const PasswordSuggestionRequest> password_request);

  // blink::WebAutofillClient:
  void TextFieldCleared(const blink::WebFormControlElement&) override;
  void TextFieldDidEndEditing(const blink::WebInputElement& element) override;
  void TextFieldValueChanged(
      const blink::WebFormControlElement& element) override;
  void ContentEditableDidChange(const blink::WebElement& element) override;
  void TextFieldDidReceiveKeyDown(
      const blink::WebInputElement& element,
      const blink::WebKeyboardEvent& event) override;
  void OpenTextDataListChooser(const blink::WebInputElement& element) override;
  void DataListOptionsChanged(const blink::WebInputElement& element) override;
  void UserGestureObserved() override;
  void AjaxSucceeded() override;
  void JavaScriptChangedValue(blink::WebFormControlElement element,
                              const blink::WebString& old_value,
                              bool was_autofilled) override;
  void DidCompleteFocusChangeInFrame() override;
  void DidReceiveLeftMouseDownOrGestureTapInNode(
      const blink::WebNode& node) override;
  void SelectFieldOptionsChanged(
      const blink::WebFormControlElement& element) override;
  void SelectControlSelectionChanged(
      const blink::WebFormControlElement& element) override;
  void FormElementReset(const blink::WebFormElement& form) override;
  void PasswordFieldReset(const blink::WebInputElement& element) override;
  void OnDevToolsSessionConnectionChanged(bool attached) override;
  void EmitFormIssuesToDevtools() override;

  // Starts observing the caret in the given element. Previous observers are
  // implicitly deleted. The event handler is HandleCaretMovedInFormField().
  void ObserveCaret(blink::WebElement element);

  // Calls CaretMovedInFormField() in a throttled manner.
  //
  // If HandleCaretMovedInFormField() has been called in the past 100 ms,
  // CaretMovedInFormField() is (re-)scheduled to be fired in 100 ms. Otherwise,
  // it is fired synchronously.
  //
  // Thus, the first event is fired immediately, but fast successive events are
  // throttled until no event has been fired for 200 ms.
  void HandleCaretMovedInFormField(blink::WebElement element,
                                   blink::WebDOMEvent event);

  void HandleFocusChangeComplete(bool focused_node_was_last_clicked,
                                 const SynchronousFormCache& form_cache);

  void DidChangeScrollOffsetImpl(FieldRendererId element_id);

  // At least on Android, multiple AskForValuesToFill() events may be fired in
  // short succession. Since getting the event handling right in AutofillAgent
  // is difficult we ignore duplicate AskForValuesToFill() as a workaround.
  // See crbug.com/40284788 for details.
  bool ShouldThrottleAskForValuesToFill(FieldRendererId field);

  // Shows Password Manager, password generation, or Autofill suggestions for
  // `element`. This call is asynchronous and may or may not lead to the showing
  // of a suggestion popup (no popup is shown if there are no available
  // suggestions). `form_cache` can be used to optimize form extractions
  // occurring synchronously after this function call.
  void ShowSuggestions(
      const blink::WebFormControlElement& element,
      AutofillSuggestionTriggerSource trigger_source,
      const SynchronousFormCache& form_cache,
      const std::optional<PasswordSuggestionRequest>& password_request);

  // Shows Autofill suggestions for `element` if `element` is a contenteditable.
  void ShowSuggestionsForContentEditable(
      const blink::WebElement& element,
      AutofillSuggestionTriggerSource trigger_source);

  // Set `element` to display the given `value`.
  void DoFillFieldWithValue(std::u16string_view value,
                            blink::WebFormControlElement& element,
                            blink::WebAutofillState autofill_state);

  // Notifies the AutofillDriver in the browser process of new and/or removed
  // forms, modulo throttling.
  //
  // Throttling means that the actual work -- that is, extracting the forms and
  // invoking AutofillDriver::FormsSeen() -- is delayed by (at least) 100 ms.
  // All subsequent calls within the next (at least) 100 ms return early.
  //
  // Calls `callback(true)` asynchronously after the timer is completed.
  // Otherwise, calls `callback(false)` immediately.
  void ExtractForms(base::OneShotTimer& timer,
                    base::OnceCallback<void(bool)> callback);

  // This function can be implemented through the one above, but it exists to
  // avoid memory allocation for the OnceCallback state. Allocation and
  // destruction of this callback in the hot path (when timer is already
  // running) is expensive.
  // Called when `element` is added/reassociated dynamically in the DOM.
  void ExtractFormsAndNotifyPasswordAutofillAgent(
      base::OneShotTimer& timer,
      const blink::WebElement& element);

  void ExtractFormsUnthrottled(base::OnceCallback<void(bool)> callback,
                               const CallTimerState& timer_state);

  // Hides any currently showing Autofill popup.
  void HidePopup();

  // Helpers for SelectFieldOptionsChanged() and
  // DataListOptionsChanged(), which get called after a timer that is restarted
  // when another event of the same type started.
  void BatchSelectOptionChange(FieldRendererId element_id);
  void BatchDataListOptionChange(FieldRendererId element_id);

  // Stores immutable configuration this agent was created with. It contains
  // features and settings that are specific to the client using this agent.
  const Config config_;

  // Contains the forms of the document.
  FormCache form_cache_{this};

  std::unique_ptr<PasswordAutofillAgent> password_autofill_agent_;
  std::unique_ptr<PasswordGenerationAgent> password_generation_agent_;

  // The element corresponding to the last request sent for form field Autofill.
  FieldRef last_queried_element_;

  // List of elements that are currently being previewed, along with their
  // autofill state before the preview.
  std::vector<std::pair<FieldRendererId, blink::WebAutofillState>>
      previewed_elements_;

  // Whether the Autofill popup is possibly visible.  This is tracked as a
  // performance improvement, so that the IPC channel isn't flooded with
  // messages to close the Autofill popup when it can't possibly be showing.
  bool is_popup_possibly_visible_ = false;

  bool last_left_mouse_down_or_gesture_tap_in_node_caused_focus_ = false;

  // This is never null, it is created at construction time and is not changed
  // until destruction time.
  std::unique_ptr<FormTracker> form_tracker_;

  mojo::AssociatedReceiver<mojom::AutofillAgent> receiver_{this};

  mojo::AssociatedRemote<mojom::AutofillDriver> autofill_driver_;

  // For deferring messages to the browser process while prerendering.
  std::unique_ptr<DeferringAutofillDriver> deferring_autofill_driver_;

  bool was_last_action_fill_ = false;

  // Timers for throttling handling of frequent events.
  std::map<FieldRendererId, base::OneShotTimer>
      select_option_change_batch_timer_;
  base::OneShotTimer datalist_option_change_batch_timer_;
  // TODO(crbug.com/40267764): Merge some or all of these timers?
  base::OneShotTimer process_forms_after_dynamic_change_timer_;
  base::OneShotTimer process_forms_form_extraction_timer_;
  base::OneShotTimer process_forms_form_extraction_with_response_timer_;

  // True iff DidDispatchDOMContentLoadedEvent() fired since the last
  // navigation.
  bool is_dom_content_loaded_ = false;

  // Will be set when accessibility mode changes, depending on what the new mode
  // is.
  bool is_screen_reader_enabled_ = false;

  // Map WebFormControlElement to the pair of:
  // 1) The most recent text that user typed or autofilled in input elements.
  // Used for storing credit card number/username/password before JavaScript
  // changes them.
  // 2) Field properties mask, i.e. whether the field was autofilled, modified
  // by user, etc. (see FieldPropertiesMask).
  scoped_refptr<FieldDataManager> field_data_manager_ =
      base::MakeRefCounted<FieldDataManager>();

  // Stores the mapping from a form element's ID to results of button titles
  // heuristics for that form.
  form_util::ButtonTitlesCache button_titles_cache_;

  // State for, and only for, HandleFocusChangeComplete().
  struct Caret {
   private:
    friend void AutofillAgent::ObserveCaret(blink::WebElement element);
    friend void AutofillAgent::HandleCaretMovedInFormField(
        blink::WebElement element,
        blink::WebDOMEvent event);
    // Removes the caret listener from the currently observed WebElement (if
    // any).
    base::ScopedClosureRunner remove_listener;
    // The last time a CaretMovedInFormField().
    base::Time time_of_last_event;
    // The timer for the next CaretMovedInFormField().
    base::OneShotTimer timer;
  } caret_state_;

  struct {
    base::TimeTicks last_autofill_agent_reset = base::TimeTicks::Now();
    base::TimeTicks last_dom_content_loaded;
  } timing_;

  struct {
    base::TimeTicks time;
    FieldRendererId field = {};
  } last_ask_for_values_to_fill_;

  const bool replace_form_element_observer_ = false;

  base::WeakPtrFactory<AutofillAgent> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_H_
