// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_TRACKER_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_TRACKER_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/web/web_input_element.h"

namespace blink {
class WebFormElementObserver;
}

namespace autofill {

// TODO(crbug.com/785531): Track the select and checkbox change.
// This class is used to track user's change of form or WebFormControlElement,
// notifies observers of form's change and submission.
class FormTracker : public content::RenderFrameObserver {
 public:
  // The interface implemented by observer to get notification of form's change
  // and submission.
  class Observer {
   public:
    enum class ElementChangeSource {
      TEXTFIELD_CHANGED,
      WILL_SEND_SUBMIT_EVENT,
      SELECT_CHANGED,
    };

    // Invoked when form needs to be saved because of |source|, |element| is
    // valid if the callback caused by source other than
    // WILL_SEND_SUBMIT_EVENT, |form| is valid for the callback caused by
    // WILL_SEND_SUBMIT_EVENT.
    virtual void OnProvisionallySaveForm(
        const blink::WebFormElement& form,
        const blink::WebFormControlElement& element,
        ElementChangeSource source) = 0;

    // Invoked when the form is probably submitted, the submmited form could be
    // the one saved in OnProvisionallySaveForm() or others in the page.
    virtual void OnProbablyFormSubmitted() = 0;

    // Invoked when |form| is submitted. The submission might not be successful,
    // observer needs to check whether the form exists in new page.
    virtual void OnFormSubmitted(const blink::WebFormElement& form) = 0;

    // Invoked when tracker infers the last form or element saved in
    // OnProvisionallySaveForm() is submitted from the |source|, the tracker
    // infers submission from the disappearance of form or element, observer
    // might not need to check it again.
    virtual void OnInferredFormSubmission(mojom::SubmissionSource source) = 0;

   protected:
    virtual ~Observer() {}
  };

  FormTracker(content::RenderFrame* render_frame);
  ~FormTracker() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Same methods as those in blink::WebAutofillClient, but invoked by
  // AutofillAgent.
  void AjaxSucceeded();
  void TextFieldDidChange(const blink::WebFormControlElement& element);
  void SelectControlDidChange(const blink::WebFormControlElement& element);

  void set_ignore_control_changes(bool ignore_control_changes) {
    ignore_control_changes_ = ignore_control_changes;
  }

  void set_user_gesture_required(bool required) {
    user_gesture_required_ = required;
  }

  void FireProbablyFormSubmittedForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(FormAutocompleteTest,
                           FormSubmittedBySameDocumentNavigation);

  // content::RenderFrameObserver:
  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override;
  void DidStartNavigation(
      const GURL& url,
      base::Optional<blink::WebNavigationType> navigation_type) override;
  void FrameDetached() override;
  void WillSendSubmitEvent(const blink::WebFormElement& form) override;
  void WillSubmitForm(const blink::WebFormElement& form) override;
  void OnDestruct() override;

  // Called in a posted task by textFieldDidChange() to work-around a WebKit bug
  // http://bugs.webkit.org/show_bug.cgi?id=16976 , we also don't want to
  // process element while it is changing.
  void FormControlDidChangeImpl(const blink::WebFormControlElement& element,
                                Observer::ElementChangeSource change_source);
  void FireProbablyFormSubmitted();
  void FireFormSubmitted(const blink::WebFormElement& form);
  void FireInferredFormSubmission(mojom::SubmissionSource source);
  void FireSubmissionIfFormDisappear(mojom::SubmissionSource source);
  bool CanInferFormSubmitted();
  void TrackElement();

  void ResetLastInteractedElements();

  // Invoked when the observed element was either removed from the DOM or it's
  // computed style changed to display: none.
  void ElementWasHiddenOrRemoved();

  base::ObserverList<Observer>::Unchecked observers_;
  bool ignore_control_changes_ = false;
  bool user_gesture_required_ = true;
  blink::WebFormElement last_interacted_form_;
  blink::WebFormControlElement last_interacted_formless_element_;
  blink::WebFormElementObserver* form_element_observer_ = nullptr;

  SEQUENCE_CHECKER(form_tracker_sequence_checker_);

  base::WeakPtrFactory<FormTracker> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FormTracker);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_TRACKER_H_
