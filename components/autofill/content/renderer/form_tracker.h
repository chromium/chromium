// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_TRACKER_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_TRACKER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/types/strong_alias.h"
#include "components/autofill/content/renderer/timing.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame_observer.h"

namespace blink {
class WebFormElementObserver;
}

namespace autofill {

class AutofillAgent;

// Reference to a WebFormElement, represented as such and as a FormRendererId.
// TODO(crbug.com/40056157): Replace with FormRendererId when
// `kAutofillReplaceCachedWebElementsByRendererIds` launches.
class FormRef {
 public:
  FormRef() = default;
  explicit FormRef(blink::WebFormElement form);

  blink::WebFormElement GetForm() const;
  FormRendererId GetId() const;

 private:
  blink::WebFormElement form_;
  FormRendererId form_renderer_id_;
};

// Reference to a WebFormControlElement, represented as such and as a
// FieldRendererId.
// TODO(crbug.com/40056157): Replace with FieldRendererId when
// `kAutofillReplaceCachedWebElementsByRendererIds` launches.
class FieldRef {
 public:
  FieldRef() = default;
  explicit FieldRef(blink::WebFormControlElement form_control);
  explicit FieldRef(blink::WebElement content_editable);

  friend bool operator<(const FieldRef& lhs, const FieldRef& rhs);

  blink::WebFormControlElement GetField() const;
  blink::WebElement GetContentEditable() const;
  FieldRendererId GetId() const;

 private:
  blink::WebElement field_;
  FieldRendererId field_renderer_id_;
};

// TODO(crbug.com/40550175): Track the select and checkbox change.
// This class is used to track user's change of form or WebFormControlElement,
// notifies observers of form's change and submission.
class FormTracker : public content::RenderFrameObserver,
                    public blink::WebLocalFrameObserver {
 public:
  // The interface implemented by observer to get notification of form's change
  // and submission.
  class Observer {
   public:
    enum class SaveFormReason {
      kTextFieldChanged,
      // TODO(crbug.com/40281981): Remove after launching the feature
      // kAutofillUnifyAndFixFormTracking.
      kWillSendSubmitEvent,
      kSelectChanged,
    };

    // TODO(crbug.com/40147954): Find a better name for this method.
    // Invoked when form needs to be saved because of |source|, |element| is
    // valid if the callback caused by source other than
    // WILL_SEND_SUBMIT_EVENT, |form| is valid for the callback caused by
    // WILL_SEND_SUBMIT_EVENT.
    virtual void OnProvisionallySaveForm(
        const blink::WebFormElement& form,
        const blink::WebFormControlElement& element,
        SaveFormReason source) = 0;

    // Invoked when the form is probably submitted, the submitted form could be
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
    virtual ~Observer() = default;
  };

  using UserGestureRequired =
      base::StrongAlias<class UserGestureRequiredTag, bool>;
  explicit FormTracker(content::RenderFrame* render_frame,
                       UserGestureRequired user_gesture_required,
                       AutofillAgent& agent);

  FormTracker(const FormTracker&) = delete;
  FormTracker& operator=(const FormTracker&) = delete;

  ~FormTracker() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Same methods as those in blink::WebAutofillClient, but invoked by
  // AutofillAgent.
  void AjaxSucceeded();
  void TextFieldDidChange(const blink::WebFormControlElement& element);
  void SelectControlDidChange(const blink::WebFormControlElement& element);
  virtual void ElementDisappeared(const blink::WebElement& element);

  // Tells the tracker to track the autofilled `element`. Since autofilling a
  // form or field won't trigger the regular *DidChange events, the tracker
  // won't be notified of this `element` otherwise. This is currently only used
  // by PWM.
  void TrackAutofilledElement(const blink::WebFormControlElement& element);

  void UpdateLastInteractedElement(
      absl::variant<FormRendererId, FieldRendererId> element_id);
  void ResetLastInteractedElements();

  FormRef last_interacted_form() const { return last_interacted_.form; }

  // TODO(crbug.com/40281981): Remove.
  std::optional<FormData>& provisionally_saved_form() {
    return last_interacted_.saved_state;
  }

 private:
  friend class FormTrackerTestApi;

  // content::RenderFrameObserver:
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void DidFinishSameDocumentNavigation() override;
  void DidStartNavigation(
      const GURL& url,
      std::optional<blink::WebNavigationType> navigation_type) override;
  void WillDetach(blink::DetachReason detach_reason) override;
  void WillSubmitForm(const blink::WebFormElement& form) override;
  void OnDestruct() override;

  // The RenderFrame* is nullptr while the AutofillAgent that owns this
  // FormTracker is pending deletion, between OnDestruct() and ~FormTracker().
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

  // content::WebLocalFrameObserver:
  void OnFrameDetached() override;
  void WillSendSubmitEvent(const blink::WebFormElement& form) override;

  // Called in a posted task by textFieldDidChange() to work-around a WebKit bug
  // http://bugs.webkit.org/show_bug.cgi?id=16976 , we also don't want to
  // process element while it is changing.
  void FormControlDidChangeImpl(FieldRendererId element_id,
                                Observer::SaveFormReason change_source);
  void FireProbablyFormSubmitted();
  void FireFormSubmitted(const blink::WebFormElement& form);
  void FireInferredFormSubmission(mojom::SubmissionSource source);
  void FireSubmissionIfFormDisappear(mojom::SubmissionSource source);
  bool CanInferFormSubmitted();

  // Tracks the cached element, as well as its ancestors, until it disappears
  // (removed or hidden), then directly infers submission. `source` is the type
  // of submission to fire when the tracked element disappears.
  // TODO(crbug.com/40281981): Remove.
  void TrackElement(mojom::SubmissionSource source);

  // Invoked when the observed element was either removed from the DOM or it's
  // computed style changed to display: none. `source` is the type of submission
  // to be inferred in case this function is called.
  // TODO(crbug.com/40281981): Remove.
  void ElementWasHiddenOrRemoved(mojom::SubmissionSource source);

  // Whether a user gesture is required to pass on text field change events.
  const UserGestureRequired user_gesture_required_;

  base::ObserverList<Observer>::Unchecked observers_;
  struct {
    FormRef form;
    FieldRef formless_element;
    // Used when a FormData version of the last interacted form is needed if
    // we'd like to avoid extracting using `form`.
    std::optional<FormData> saved_state;
  } last_interacted_;

  // TODO(crbug.com/40281981): Remove.
  raw_ptr<blink::WebFormElementObserver> form_element_observer_ = nullptr;

  struct {
    bool tracked_element_disappeared = false;
    bool tracked_element_autofilled = false;
    bool finished_same_document_navigation = false;
    bool xhr_succeeded = false;
  } submission_triggering_events_;

  // The object owning this `FormTracker`.
  raw_ref<AutofillAgent> agent_;

  SEQUENCE_CHECKER(form_tracker_sequence_checker_);

  base::WeakPtrFactory<FormTracker> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_TRACKER_H_
