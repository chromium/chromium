// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_AUTOFILL_DRIVER_H_

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
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/accessibility/ax_tree_id.h"
#include "url/origin.h"

namespace autofill {

class AutofillClient;
class AutofillDriverFactory;
class AutofillManager;
class FormStructure;
class Section;

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
  // LINT.IfChange(LifecycleStateChanges)
  //
  //     ╭───────────────────────────╮
  //     │                           ▼
  //   kInactive ◄──► kActive ──► kPendingDeletion
  //     ▲                ▲
  //     │                ╰─────► kPendingReset
  //     ╰──────────────────────► kPendingReset
  //
  // LINT.ThenChange(autofill_driver.cc:LifecycleStateChanges)
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
                         base::PassKey<AutofillDriverFactory> pass_key);

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
  //
  // Two related properties are IsActive() and IsEmbedded().
  virtual AutofillDriver* GetParent() = 0;

  // Returns true if the AutofillDriver is associated with a frame that is
  // visible to the user, as opposed to being prerendered or bfcache.
  //
  // Two related properties are GetParent() and IsEmbedded().
  //
  // This terminology is borrowed from MPArch.
  virtual bool IsActive() const = 0;

  // Returns true if the AutofillDriver is associated with a frame from a frame
  // tree that is embedded in another frame by a Guest View or <fencedframe>.
  //
  // Two related properties are GetParent() and IsActive().
  //
  // This terminology is borrowed from MPArch. (The MPArch documentation is not
  // entirely consistent in its use of "embedded" and "outermost", so there may
  // be references that are not equivalent to AutofillDriver's use of the term.
  // See crbug.com/459210100.)
  virtual bool IsEmbedded() const = 0;

  // The owning AutofillClient.
  virtual AutofillClient& GetAutofillClient() = 0;

  // Returns the AutofillManager owned by the AutofillDriver.
  virtual AutofillManager& GetAutofillManager() = 0;

  // Gets the UKM source ID associated with this driver's outermost main frame's
  // document.
  //
  // That implies the following properties:
  // - A child frame has the same UKM source ID as its parent frame.
  // - When a cross-document navigation in the outermost main frame leads to ...
  //   - ... a *new* AutofillDriver, the new driver has a new UKM source ID.
  //   - ... the *same* AutofillDriver (i.e., the driver transitions into the
  //     LifecycleState::kPendingReset), the driver gets a new UKM source ID.
  virtual ukm::SourceId GetPageUkmSourceId() const = 0;

  // Returns whether the policy-controlled feature "autofill" is enabled in the
  // document. In the main frame the permission is enabled by default. The main
  // frame may pass it on to its children.
  virtual bool IsPolicyControlledFeatureAutofillEnabled() const = 0;

  // Returns true if the policy-controlled feature "manual-text" is enabled in
  // the document. In the main frame the permission is enabled by default.
  // Parent frames may pass it on to its children.
  virtual bool IsPolicyControlledFeatureManualTextEnabled() const = 0;

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

  // Extracts the form that contains the given field and calls
  // `response_handler` for the browser form that includes that form.
  //
  // Consider the following example:
  //   <form id=f>
  //     <input>
  //     <iframe>
  //       <form id=g>
  //         <input id=i>
  //       </form>
  //     </iframe>
  //   </form>
  // Calling ExtractForm() for "i" re-extracts that form and may then flatten it
  // into "f". So the `response_handler` is called for that browser form that
  // includes "f" and the newly-extracted "g".
  //
  // To re-extract all forms (in all frames), see TriggerFormExtractionIn*().
  //
  // More precisely:
  //
  // If a field with `field_id` is found, `response_handler` is called with the
  // driver that manages the browser form that includes that field and that
  // browser form itself (i.e., their `FormData::host_frame()` and
  // `AutofillDriver::GetFrameToken()` are equal). The driver is distinct from
  // `this` if the form is managed by another frame (e.g., when `this` is a
  // subframe and the form is managed by an ancestor).
  //
  // If the field is not found, the `response_handler` is called with nullptr
  // for the driver and std::nullopt for the form.
  virtual void ExtractFormWithField(FieldGlobalId field_id,
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
  // `section_for_clear_form_on_ios` is a hack for iOS, where "Clear Form"
  // resets the values of fields in a certain section.
  // TODO(crbug.com/338201947): Remove `section_for_clear_form_on_ios` when iOS
  // has "Undo Autofill" instead of "Clear Form".
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
      const base::flat_map<FieldGlobalId, FieldType>& field_type_map,
      const Section& section_for_clear_form_on_ios) = 0;

  // Tells the renderer to perform actions on the node text.
  // If the `action_type` is kSelectAll, then `value` needs to be empty.
  virtual void ApplyFieldAction(mojom::FieldActionType action_type,
                                mojom::ActionPersistence action_persistence,
                                const FieldGlobalId& field_id,
                                const std::u16string& value) = 0;

  // Sends the field type predictions of `form` to the renderer.
  virtual void SendTypePredictionsToRenderer(const FormStructure& forms) = 0;

  // Exposes DOM Node IDs in an attribute "dom-node-id".
  virtual void ExposeDomNodeIdsInAllFrames() = 0;

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

  // Searches for the final checkout amount in the DOM and returns the amount
  // back to the browser process.
  // See `form_util::ExtractFinalCheckoutAmountFromDom()` for details.
  virtual void ExtractLabeledTextNodeValue(
      const std::u16string& value_regex,
      const std::u16string& label_regex,
      uint32_t number_of_ancestor_levels_to_search,
      base::OnceCallback<void(const std::string& amount)>
          response_callback) = 0;

  virtual void DispatchEmailVerifiedEvent(
      FieldGlobalId field_id,
      const std::string& presentation_token) = 0;

 private:
  friend class AutofillDriverTestApi;

  LifecycleState lifecycle_state_ = LifecycleState::kInactive;

#if DCHECK_IS_ON()
  LifecycleState previous_lifecycle_state_ = LifecycleState::kInactive;
#endif
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_AUTOFILL_DRIVER_H_
