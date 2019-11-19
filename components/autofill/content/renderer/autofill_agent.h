// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_H_

#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/form_cache.h"
#include "components/autofill/content/renderer/form_tracker.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
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
template <typename T>
class WebVector;
}

namespace autofill {

struct FormData;
class PasswordAutofillAgent;
class PasswordGenerationAgent;

// AutofillAgent deals with Autofill related communications between WebKit and
// the browser.  There is one AutofillAgent per RenderFrame.
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
  // PasswordGenerationAgent may be NULL. If it is not, then it is also
  // guaranteed to outlive AutofillAgent.
  AutofillAgent(content::RenderFrame* render_frame,
                PasswordAutofillAgent* password_autofill_manager,
                PasswordGenerationAgent* password_generation_agent,
                blink::AssociatedInterfaceRegistry* registry);
  ~AutofillAgent() override;

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::AutofillAgent> pending_receiver);

  const mojo::AssociatedRemote<mojom::AutofillDriver>& GetAutofillDriver();

  const mojo::AssociatedRemote<mojom::PasswordManagerDriver>&
  GetPasswordManagerDriver();

  // mojom::AutofillAgent:
  void FillForm(int32_t id, const FormData& form) override;
  void PreviewForm(int32_t id, const FormData& form) override;
  void FieldTypePredictionsAvailable(
      const std::vector<FormDataPredictions>& forms) override;
  void ClearSection() override;
  void ClearPreviewedForm() override;
  void FillFieldWithValue(const base::string16& value) override;
  void PreviewFieldWithValue(const base::string16& value) override;
  void SetSuggestionAvailability(const mojom::AutofillState state) override;
  void AcceptDataListSuggestion(const base::string16& value) override;
  void FillPasswordSuggestion(const base::string16& username,
                              const base::string16& password) override;
  void PreviewPasswordSuggestion(const base::string16& username,
                                 const base::string16& password) override;
  void SetUserGestureRequired(bool required) override;
  void SetSecureContextRequired(bool required) override;
  void SetFocusRequiresScroll(bool require) override;
  void SetQueryPasswordSuggestion(bool required) override;
  void GetElementFormAndFieldData(
      const std::vector<std::string>& selectors,
      GetElementFormAndFieldDataCallback callback) override;

  void FormControlElementClicked(const blink::WebFormControlElement& element,
                                 bool was_focused);

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

  FormTracker* form_tracker_for_testing() { return &form_tracker_; }

  void SelectWasUpdated(const blink::WebFormControlElement& element);

 protected:
  // blink::WebAutofillClient:
  void DidAssociateFormControlsDynamically() override;

 private:
  friend class FormControlClickDetectionTest;

  // Functor used as a simplified comparison function for FormData. Only
  // compares forms at a high level (notably name, origin, action).
  struct FormDataCompare {
    bool operator()(const FormData& lhs, const FormData& rhs) const;
  };

  // Flags passed to ShowSuggestions.
  struct ShowSuggestionsOptions {
    // All fields are default initialized to false.
    ShowSuggestionsOptions();

    // Specifies that suggestions should be shown when |element| contains no
    // text.
    bool autofill_on_empty_values;

    // Specifies that suggestions should be shown when the caret is not
    // after the last character in the element.
    bool requires_caret_at_end;

    // Specifies that all of <datalist> suggestions and no autofill suggestions
    // are shown. |autofill_on_empty_values| and |requires_caret_at_end| are
    // ignored if |datalist_only| is true.
    bool datalist_only;

    // Specifies that all autofill suggestions should be shown and none should
    // be elided because of the current value of |element| (relevant for inline
    // autocomplete).
    bool show_full_suggestion_list;

    // Specifies that only show a suggestions box if |element| is part of a
    // password form, otherwise show no suggestions.
    bool show_password_suggestions_only;

    // Specifies that the first suggestion must be auto-selected when the
    // dropdown is shown. Enabled when the user presses ARROW_DOWN on a field.
    bool autoselect_first_suggestion;
  };

  // content::RenderFrameObserver:
  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override;
  void DidFinishDocumentLoad() override;
  void DidChangeScrollOffset() override;
  void FocusedElementChanged(const blink::WebElement& element) override;
  void AccessibilityModeChanged(const ui::AXMode& mode) override;
  void OnDestruct() override;

  // Fires Mojo messages for a given form submission.
  void FireHostSubmitEvents(const blink::WebFormElement& form,
                            bool known_success,
                            mojom::SubmissionSource source);
  void FireHostSubmitEvents(const FormData& form_data,
                            bool known_success,
                            mojom::SubmissionSource source);

  // Shuts the AutofillAgent down on RenderFrame deletion. Safe to call multiple
  // times.
  void Shutdown();

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
  void DidCompleteFocusChangeInFrame() override;
  void DidReceiveLeftMouseDownOrGestureTapInNode(
      const blink::WebNode& node) override;
  void SelectFieldOptionsChanged(
      const blink::WebFormControlElement& element) override;
  void SelectControlDidChange(
      const blink::WebFormControlElement& element) override;
  bool ShouldSuppressKeyboard(
      const blink::WebFormControlElement& element) override;

  void HandleFocusChangeComplete();

  // Helper method which collects unowned elements (i.e., those not inside a
  // form tag) and writes them into |output|. Returns true if the process is
  // successful, and all conditions for firing events are true.
  bool CollectFormlessElements(FormData* output);
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
  void QueryAutofillSuggestions(const blink::WebFormControlElement& element,
                                bool autoselect_first_suggestion);

  // Sets the element value to reflect the selected |suggested_value|.
  void DoAcceptDataListSuggestion(const base::string16& suggested_value);

  // Set |node| to display the given |value|.
  void DoFillFieldWithValue(const base::string16& value,
                            blink::WebInputElement* node);

  // Set |node| to display the given |value| as a preview.  The preview is
  // visible on screen to the user, but not visible to the page via the DOM or
  // JavaScript.
  void DoPreviewFieldWithValue(const base::string16& value,
                               blink::WebInputElement* node);

  // Notifies browser of new fillable forms in |render_frame|.
  void ProcessForms();

  // Hides any currently showing Autofill popup.
  void HidePopup();

  // Attempt to get submitted FormData from last_interacted_form_ or
  // provisionally_saved_form_, return true if |form| is set.
  bool GetSubmittedForm(FormData* form);

  void ResetLastInteractedElements();
  void UpdateLastInteractedForm(blink::WebFormElement form);

  // Called when current form is no longer submittable, submitted_forms_ is
  // cleared in this method.
  void OnFormNoLongerSubmittable();

  // For no name forms, and unowned elements, try to see if there is a unique
  // element in the updated form that corresponds to the |original_element|.
  // Returns false if more than one element matches the |original_element|.
  // Sets the matching element to |matching_element| and updates the
  // |potential_match_encountered|, based on the search result. Returns false if
  // more than one element match the name and section, therefore finding a
  // unique match is impossible.
  bool FindTheUniqueNewVersionOfOldElement(
      const blink::WebVector<blink::WebFormControlElement>& elements,
      bool& potential_match_encountered,
      blink::WebFormControlElement& matching_element,
      const blink::WebFormControlElement& original_element);

  // Check whether |element_| was removed or replaced dynamically on the page.
  // If so, looks for the same element in the updated |form| and replaces the
  // |element_| with it if it's found.
  void ReplaceElementIfNowInvalid(const FormData& form);

  // Trigger a refill if needed for dynamic forms. A refill is needed if some
  // properties of the form (name, number of fields), or fields (name, id,
  // label, visibility, control type) have changed after an autofill.
  void TriggerRefillIfNeeded(const FormData& form);

  // Find the unique element given by |selectors| in the associated web frame.
  // Empty blink::WebElement is returned if there is no matching element or
  // there are multiple matching elements.
  blink::WebElement FindUniqueWebElement(
      const std::vector<std::string>& selectors);

  // Formerly cached forms for all frames, now only caches forms for the current
  // frame.
  FormCache form_cache_;

  PasswordAutofillAgent* password_autofill_agent_;      // Weak reference.
  PasswordGenerationAgent* password_generation_agent_;  // Weak reference.

  // The ID of the last request sent for form field Autofill.  Used to ignore
  // out of date responses.
  int autofill_query_id_;

  // The element corresponding to the last request sent for form field Autofill.
  blink::WebFormControlElement element_;

  // The form element currently requesting an interactive autocomplete.
  blink::WebFormElement in_flight_request_form_;

  // Last form which was interacted with by the user.
  blink::WebFormElement last_interacted_form_;

  // When dealing with forms that don't use a <form> tag, we keep track of the
  // elements the user has modified so we can determine when submission occurs.
  std::set<blink::WebFormControlElement> formless_elements_user_edited_;

  // The form user interacted, it is used if last_interacted_form_ or formless
  // form can't be converted to FormData at the time of form submission.
  std::unique_ptr<FormData> provisionally_saved_form_;

  // Keeps track of the forms for which form submitted event has been sent to
  // AutofillDriver. We use it to avoid fire duplicated submission event when
  // WILL_SEND_SUBMIT_EVENT and form submitted are both fired for same form.
  // The submitted_forms_ is cleared when we know no more submission could
  // happen for that form.
  // We use a simplified comparison function.
  std::set<FormData, FormDataCompare> submitted_forms_;

  // The query node autofill state prior to previewing the form.
  blink::WebAutofillState query_node_autofill_state_;

  // Whether or not to ignore text changes.  Useful for when we're committing
  // a composition when we are defocusing the WebView and we don't want to
  // trigger an autofill popup to show.
  bool ignore_text_changes_;

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
  bool was_focused_before_now_ = false;
  blink::WebFormControlElement last_clicked_form_control_element_for_testing_;
  bool last_clicked_form_control_element_was_focused_for_testing_ = false;

  FormTracker form_tracker_;

  // Whether or not we delay focus handling until scrolling occurs.
  bool focus_requires_scroll_ = true;

  mojo::AssociatedReceiver<mojom::AutofillAgent> receiver_{this};

  mojo::AssociatedRemote<mojom::AutofillDriver> autofill_driver_;

  bool was_last_action_fill_ = false;

  base::OneShotTimer on_select_update_timer_;

  // Will be set when accessibility mode changes, depending on what the new mode
  // is.
  bool is_screen_reader_enabled_ = false;

  base::WeakPtrFactory<AutofillAgent> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AutofillAgent);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_H_
