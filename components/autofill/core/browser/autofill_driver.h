// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
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
  virtual ~AutofillDriver() = default;

  // Returns the uniquely identifying frame token.
  virtual LocalFrameToken GetFrameToken() const = 0;

  // Returns the AutofillDriver of the parent frame, if such a frame and driver
  // exist, and nullptr otherwise.
  virtual AutofillDriver* GetParent() = 0;

  // Resolves a FrameToken `query` from the perspective of `this` to the
  // globally unique LocalFrameToken. Returns `absl::nullopt` if `query` is a
  // RemoteFrameToken that cannot be resolved from the perspective of `this`.
  //
  // This function should not be cached: a later Resolve() call may map the same
  // RemoteFrameToken to another LocalFrameToken.
  //
  // See the documentation of LocalFrameToken and RemoteFrameToken for details.
  virtual absl::optional<LocalFrameToken> Resolve(FrameToken query) = 0;

  // Returns whether the AutofillDriver instance is associated with an active
  // frame in the MPArch sense.
  virtual bool IsInActiveFrame() const = 0;

  // Returns whether the AutofillDriver instance is associated with a main
  // frame, in the MPArch sense. This can be a primary or non-primary main
  // frame.
  virtual bool IsInAnyMainFrame() const = 0;

  // Returns whether the AutofillDriver instance is associated with a
  // prerendered frame.
  virtual bool IsPrerendering() const = 0;

  // Returns whether the policy-controlled feature "shared-autofill" is enabled
  // in the document. In the main frame the permission is enabled by default.
  // The main frame may pass it on to its children.
  virtual bool HasSharedAutofillPermission() const = 0;

  // Returns true iff a popup can be shown on the behalf of the associated
  // frame.
  virtual bool CanShowAutofillUi() const = 0;

  // Triggers a form extraction of the new forms in the AutofillAgent. This is
  // necessary when a form is seen in a child frame and it is not known which
  // form is its parent.
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
  virtual void TriggerFormExtraction() = 0;

  // Triggers a form_extraction on all frames of the same frame tree. Calls
  // `form_extraction_finished_callback` when all frames reported back
  // being done. `success == false` indicates that in some frame, a
  // form_extraction was triggered while another form_extraction was ongoing.
  virtual void TriggerFormExtractionInAllFrames(
      base::OnceCallback<void(bool success)>
          form_extraction_finished_callback) = 0;

  // Returns the ax tree id associated with this driver.
  virtual ui::AXTreeID GetAxTreeId() const = 0;

  // Returns true iff the renderer is available for communication.
  virtual bool RendererIsAvailable() = 0;

  // Forwards |data| to the renderer which shall preview or fill the values of
  // |data|'s fields into the relevant DOM elements.
  //
  // |action| is the action the renderer should perform with the |data|.
  //
  // |triggered_origin| is the origin of the field on which Autofill was
  // triggered, and |field_type_map| contains the type predictions of the fields
  // that may be previewed or filled; these two parameters can be taken into
  // account to decide which fields to preview or fill across frames. See
  // FormForest::GetRendererFormsOfBrowserForm() for the details on Autofill's
  // security policy.
  //
  // Returns the ids of those fields that are safe to fill according to the
  // security policy for cross-frame previewing and filling. This is a subset of
  // the ids of the fields in |data|. It is not necessarily a subset of the
  // fields in |field_type_map|.
  //
  // This method is a no-op if the renderer is not currently available.
  virtual std::vector<FieldGlobalId> FillOrPreviewForm(
      mojom::AutofillActionPersistence action_persistence,
      const FormData& data,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map) = 0;

  // Forwards `data` to the renderer which shall fill the values of `data`'s
  // fields, whose last filling operation was undone, into the relevant DOM
  // elements.
  //
  // `field_type_map` contains the type predictions of the fields that may be
  // modified; this parameter can be taken into account to decide which fields
  // to modify across frames. See FormForest::GetRendererFormsOfBrowserForm()
  // for the details on Autofill's security policy. Note that this map contains
  // the types of the fields at filling time and not at undo time, to ensure
  // consistency.
  //
  // `triggered_origin` is the origin of the field that triggered the filling
  // operation currently being undone.
  //
  // This method is a no-op if the renderer is not currently available.
  virtual void UndoAutofill(
      mojom::AutofillActionPersistence action_persistence,
      const FormData& data,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map) = 0;

  // Forwards parsed |forms| to the embedder.
  virtual void HandleParsedForms(const std::vector<FormData>& forms) = 0;

  // Sends the field type predictions specified in |forms| to the renderer. This
  // method is a no-op if the renderer is not available or the appropriate
  // command-line flag is not set.
  virtual void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) = 0;

  // Tells the renderer to accept data list suggestions for |value|.
  virtual void RendererShouldAcceptDataListSuggestion(
      const FieldGlobalId& field_id,
      const std::u16string& value) = 0;

  // Tells the renderer to clear the current section of the autofilled values.
  virtual void RendererShouldClearFilledSection() = 0;

  // Tells the renderer to clear the currently previewed Autofill results.
  virtual void RendererShouldClearPreviewedForm() = 0;

  virtual void RendererShouldTriggerSuggestions(
      const FieldGlobalId& field_id,
      AutofillSuggestionTriggerSource trigger_source) = 0;

  // Tells the renderer to set the node text.
  virtual void RendererShouldFillFieldWithValue(
      const FieldGlobalId& field_id,
      const std::u16string& value) = 0;

  // Tells the renderer to preview the node with suggested text.
  virtual void RendererShouldPreviewFieldWithValue(
      const FieldGlobalId& field_id,
      const std::u16string& value) = 0;

  // Tells the renderer to set the currently focused node's corresponding
  // accessibility node's autofill state to |state|.
  virtual void RendererShouldSetSuggestionAvailability(
      const FieldGlobalId& field_id,
      const mojom::AutofillState state) = 0;

  // Informs the renderer that the popup has been hidden.
  virtual void PopupHidden() = 0;

  virtual net::IsolationInfo IsolationInfo() = 0;

  // Tells the renderer about the form fields that are eligible for triggering
  // manual filling on form interaction.
  virtual void SendFieldsEligibleForManualFillingToRenderer(
      const std::vector<FieldGlobalId>& fields) = 0;

  // Query's the DOM for four digit combinations that could potentially be of a
  // card number.
  virtual void GetFourDigitCombinationsFromDOM(
      base::OnceCallback<void(const std::vector<std::string>&)>
          potential_matches) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_
