// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "net/base/isolation_info.h"
#include "ui/accessibility/ax_tree_id.h"
#include "url/origin.h"

namespace autofill {

class FormStructure;
class AutofillClient;
class AutofillDriverFactory;
class AutofillManager;

namespace internal {
class FormForest;
}

// AutofillDriver is Autofill's lowest-level abstraction of a frame that is
// shared among all platforms.
//
// Most notably, it is a gateway for all communication from the browser code to
// the DOM, or more precisely, to the AutofillAgent (which are different classes
// of the same name in non-iOS vs iOS).
//
// The reverse communication, from the AutofillAgent to the browser code, goes
// through mojom::AutofillDriver on non-iOS, and directly to AutofillManager on
// iOS.
//
// Events for browser-internal communication do *NOT* belong here. Use
// AutofillManager::Observer instead.
//
// An AutofillDriver corresponds to a frame, rather than a document, in the
// sense that it may survive navigations.
//
// AutofillDriver has two implementations:
// - AutofillDriverIOS for iOS,
// - ContentAutofillDriver for all other platforms.
//
// An AutofillDriver's lifetime must be contained by the associated frame,
// web::WebFrame on iOS and content::RenderFrameHost on non-iOS. This is ensured
// by the AutofillDriverIOSFactory and ContentAutofillDriverFactory,
// respectively, which own the AutofillDrivers.
class AutofillDriver {
 public:
  // An AutofillDriver's LifecycleState indicates whether its content is
  // currently presented to the user. It closely follows
  // content::RenderFrameHost's LifecycleState but collapses inactive states.
  //
  // State changes must not happen during construction or destruction.
  //
  // State changes fire events in AutofillManager::Observer.
  //
  // The possible transitions are:
  //
  //   ╭───────────────────────────╮
  //   │                           ▼
  // kInactive ◄──► kActive ──► kPendingDeletion
  //   ▲                ▲
  //   │                ╰─────► kPendingReset
  //   ╰──────────────────────► kPendingReset
  //
  // The initial state is kInactive.
  //
  // Transitions from kPendingReset can only return to the previous state.
  // Transitions between kInactive and kPendingReset only happen if the frame is
  // prerendering.
  // TODO: crbug.com/342132628 - Such transitions won't be possible anymore when
  // prerendered CADs are deferred.
  //
  // Common behavior very shortly after the AutofillDriver's creation is the
  // following:
  // 1. It transitions to kActive.
  //    That happens unless the document is prerendering.
  // 2. It transitions to kPendingReset and then back to its previous state,
  //    kActive or kInactive. That happens on non-iOS when the frame does its
  //    first navigation, which is just a special case of a navigation that the
  //    AutofillDriver survives.
  enum class LifecycleState {
    // The AutofillDriver corresponds to a frame that is currently not
    // displayed to the user, either because it is being prerendered or because
    // it is BFCached.
    kInactive,
    // The AutofillDriver corresponds to a frame that is being displayed.
    kActive,
    // The AutofillDriver is about to be reset because the document in its
    // associated driver is about to change.
    kPendingReset,
    // The destructor of AutofillDriver and its associated AutofillDriver are
    // about to begin. The AutofillDriver is still fully intact at this point.
    kPendingDeletion,
  };

  virtual ~AutofillDriver();

  // The current state of the driver. See LifecycleState for details.
  LifecycleState GetLifecycleState() const { return lifecycle_state_; }

  // Sets the new lifecycle state.
  //
  // AutofillDriverFactory (not AutofillDriver) manages the lifecycle because it
  // is easiest for the factory to coordinate the different phases:
  // - construct the driver,
  // - set lifecycle state change,
  // - notify observers,
  // - destruct the driver.
  void SetLifecycleState(LifecycleState new_state,
                         base::PassKey<AutofillDriverFactory> pass_key) {
    DCHECK_NE(lifecycle_state_, new_state);
    lifecycle_state_ = new_state;
  }

  // Returns the uniquely identifying frame token.
  virtual LocalFrameToken GetFrameToken() const = 0;

  // Resolves a FrameToken `query` from the perspective of `this` to the
  // globally unique LocalFrameToken. Returns `std::nullopt` if `query` is a
  // RemoteFrameToken that cannot be resolved from the perspective of `this`.
  //
  // This function should not be cached: a later Resolve() call may map the same
  // RemoteFrameToken to another LocalFrameToken.
  //
  // See the documentation of LocalFrameToken and RemoteFrameToken for details.
  virtual std::optional<LocalFrameToken> Resolve(FrameToken query) = 0;

  // Returns the AutofillDriver of the parent frame, if such a frame and driver
  // exist, and nullptr otherwise.
  virtual AutofillDriver* GetParent() = 0;

  // The owning AutofillClient.
  virtual AutofillClient& GetAutofillClient() = 0;

  // Returns the AutofillManager owned by the AutofillDriver.
  virtual AutofillManager& GetAutofillManager() = 0;

  // Returns whether the AutofillDriver instance is associated with an active
  // frame in the MPArch sense.
  virtual bool IsActive() const = 0;

  // Returns whether the AutofillDriver instance is associated with a main
  // frame, in the MPArch sense. This can be a primary or non-primary main
  // frame.
  virtual bool IsInAnyMainFrame() const = 0;

  // Returns whether the policy-controlled feature "shared-autofill" is enabled
  // in the document. In the main frame the permission is enabled by default.
  // The main frame may pass it on to its children.
  virtual bool HasSharedAutofillPermission() const = 0;

  // Returns the IsolationInfo of the associated frame. May be nullopt if the
  // IsolationInfo is not used (for example, on iOS).
  virtual std::optional<net::IsolationInfo> GetIsolationInfo() = 0;

  // Returns true iff a popup can be shown on the behalf of the associated
  // frame.
  virtual bool CanShowAutofillUi() const = 0;

  class AutofillDriverRouterAndFormForestPassKey {
    friend class AutofillDriverRouter;
    friend class internal::FormForest;
    friend class AutofillDriverTestApi;
    AutofillDriverRouterAndFormForestPassKey() = default;
  };

  // Triggers a form extraction of the new forms in the AutofillAgent. This is
  // necessary when a form is seen in a child frame and it is not known which
  // form is its parent.
  //
  // Unlike other events, this is *not* be routed or broadcast to other frames;
  // it refers to the frame associated with the driver.
  //
  // Generally, this may happen because AutofillAgent is only notified about
  // newly created form control elements, but not about newly created or loaded
  // child frames.
  //
  // For example, consider a parent frame with a form that contains an <iframe>.
  // Suppose the parent form is seen (processed by AutofillDriver::FormsSeen())
  // before the iframe is loaded. Loading a cross-origin page into the iframe
  // may change the iframe's frame token. Then, the frame token in the parent
  // form's FormData::child_frames may be outdated. When a form is now seen in
  // the child frame, it is not known *which form* in the parent frame is its
  // parent form. In this scenario, a form extraction should be triggered.
  virtual void TriggerFormExtractionInDriverFrame(
      AutofillDriverRouterAndFormForestPassKey pass_key) = 0;

  // Triggers a form_extraction on all frames of the same frame tree. Calls
  // `form_extraction_finished_callback` when all frames reported back
  // being done. `success == false` indicates that in some frame, a
  // form_extraction was triggered while another form_extraction was ongoing.
  virtual void TriggerFormExtractionInAllFrames(
      base::OnceCallback<void(bool success)>
          form_extraction_finished_callback) = 0;

  // Response handler for ExtractForm(). The `host_frame_driver` manages `form`,
  // i.e., `form.host_frame == host_frame_driver->GetFrameToken()`. The form is
  // the flattened representation of the form (see autofill_driver_router.h or
  // form_forest.h for the definition of a browser form).
  using BrowserFormHandler =
      base::OnceCallback<void(AutofillDriver* host_frame_driver,
                              const std::optional<FormData>& form)>;

  // Extracts the given form and calls `response_handler` for the browser form
  // that includes `form`.
  //
  // The semantics may be a little surprising. Consider the following example:
  //   <form id=f>
  //     <input>
  //     <iframe>
  //       <form id=g>
  //         <input id=i>
  //       </form>
  //     </iframe>
  //   </form>
  // Calling ExtractForm() for "g" re-extracts that form and may then flatten it
  // into "f". So the `response_handler` is called for that browser form that
  // includes "f" and the newly-extracted "g".
  //
  // To re-extract all forms (in all frames), see TriggerFormExtractionIn*().
  //
  // More precisely:
  //
  // If the `form` is found, `response_handler` is called with the driver that
  // manages the browser form that includes `form` and that browser form itself
  // (i.e., their `FormData.host_frame` and `AutofillDriver::GetFrameToken()`
  // are equal). The driver is distinct from `this` if the form is managed by
  // another frame (e.g., when `this` is a subframe and the form is managed by
  // an ancestor).
  //
  // If the form is not found, the `response_handler` is called with nullptr for
  // the driver and std::nullopt for the form.
  virtual void ExtractForm(FormGlobalId form,
                           BrowserFormHandler response_handler) = 0;

  // Forwards `form` to the renderer.
  //
  // `field_type_map` contains the type predictions of the fields that may be
  // modified; this parameter can be taken into account to decide which fields
  // to modify across frames. See FormForest::GetRendererFormsOfBrowserForm()
  // for the details on Autofill's security policy. Note that this map contains
  // the types of the fields at filling time and not at undo time, to ensure
  // consistency.
  //
  // `triggered_origin` is the origin of the field that triggered the filling
  // operation currently being filled or undone.
  //
  // Returns the FieldGlobalIds that were safe to modify according to Autofill's
  // security policy. This is a subset of the FieldGlobalIds of `form.fields`.
  //
  // This method is a no-op if the renderer is not currently available.
  virtual base::flat_set<FieldGlobalId> ApplyFormAction(
      mojom::FormActionType action_type,
      mojom::ActionPersistence action_persistence,
      base::span<const FormFieldData> data,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, FieldType>& field_type_map) = 0;

  // Tells the renderer to perform actions on the node text.
  // If the `action_type` is kSelectAll, then `value` needs to be empty.
  virtual void ApplyFieldAction(mojom::FieldActionType action_type,
                                mojom::ActionPersistence action_persistence,
                                const FieldGlobalId& field_id,
                                const std::u16string& value) = 0;

  // Sends the field type predictions specified in |forms| to the renderer. This
  // method is a no-op if the renderer is not available or the appropriate
  // command-line flag is not set.
  virtual void SendTypePredictionsToRenderer(
      const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms) = 0;

  // Tells the renderer to accept data list suggestions for |value|.
  virtual void RendererShouldAcceptDataListSuggestion(
      const FieldGlobalId& field_id,
      const std::u16string& value) = 0;

  // Tells the renderer to clear the currently previewed Autofill results.
  virtual void RendererShouldClearPreviewedForm() = 0;

  // Tells the renderer to trigger a AskForValuesToFill() event.
  virtual void RendererShouldTriggerSuggestions(
      const FieldGlobalId& field_id,
      AutofillSuggestionTriggerSource trigger_source) = 0;

  // Tells the renderer to set the currently focused node's corresponding
  // accessibility node's autofill suggestion_availability to
  // |suggestion_availability|.
  virtual void RendererShouldSetSuggestionAvailability(
      const FieldGlobalId& field_id,
      mojom::AutofillSuggestionAvailability suggestion_availability) = 0;

  // Query's the DOM for four digit combinations that could potentially be of a
  // card number.
  virtual void GetFourDigitCombinationsFromDom(
      base::OnceCallback<void(const std::vector<std::string>&)>
          potential_matches) = 0;

 private:
  friend class AutofillDriverTestApi;

  LifecycleState lifecycle_state_ = LifecycleState::kInactive;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_
