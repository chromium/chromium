// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/types/optional_ref.h"
#include "build/build_config.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace autofill {

class ContentAutofillDriverFactory;
class AutofillDriverRouter;

// ContentAutofillDriver drives the Autofill flow in the browser process based
// on communication from the renderer and from the external world.
//
// Each ContentAutofillDriver is associated with exactly one RenderFrameHost
// and communicates with exactly one AutofillAgent throughout its entire
// lifetime.
//
// This RenderFrameHost owns all forms and fields in the renderer-browser
// communication:
// - ContentAutofillDriver may assume that forms and fields received in the
//   mojom::AutofillDriver events are owned by that RenderFrameHost.
// - Conversely, the forms and fields which ContentAutofillDriver passes to
//   mojom::AutofillAgent events must be owned by that RenderFrameHost.
//
// Events in AutofillDriver and mojom::AutofillDriver are passed on to
// AutofillDriverRouter, which has one instance per WebContents. The naming
// pattern is that for all of these events, there are three functions:
//
//   1. ReturnType ContentAutofillDriver::f(Args...)
//   2. ReturnType AutofillDriverRouter::f(AutofillDriver*, Args..., callback)
//   3. ReturnType callback(AutofillDriver*, Args...)
//
// The first function calls the second, and the second calls the third, perhaps
// for a different AutofillDriver and with modified arguments.
//
// Consider the following pseudo-HTML:
//   <!-- frame name "ABC" -->
//   <form>
//     <input> <!-- renderer_id = 12 -->
//     <input> <!-- renderer_id = 34 -->
//     <iframe name="DEF">
//       <input> <!-- renderer_id = 56 -->
//       <input> <!-- renderer_id = 78 -->
//     </iframe>
//   </form>
// In this case, the frame "ABC" holds a form with fields
//   FormFieldData{.host_frame = ABC, .renderer_id = 12, ...},
//   FormFieldData{.host_frame = ABC, .renderer_id = 34, ...},
// and the frame "DEF" holds a form with fields
//   FormFieldData{.host_frame = DEF, .renderer_id = 56, ...},
//   FormFieldData{.host_frame = DEF, .renderer_id = 78, ...}.
// The SendFieldsEligibleForManualFillingToRenderer() event, for example, is
// initiated by ABC's AutofillManager by calling
//   abc_driver->SendFieldsEligibleForManualFillingToRenderer({
//     FieldGlobalId{.host_frame = ABC, .renderer_id = 12},
//     FieldGlobalId{.host_frame = ABC, .renderer_id = 34},
//     FieldGlobalId{.host_frame = DEF, .renderer_id = 56},
//     FieldGlobalId{.host_frame = DEF, .renderer_id = 78}
//   }).
// |abc_driver| forwards the event to the router by calling
//   router->SendFieldsEligibleForManualFillingToRenderer(abc_driver, {
//     FieldGlobalId{.host_frame = ABC, .renderer_id = 12},
//     FieldGlobalId{.host_frame = ABC, .renderer_id = 34},
//     FieldGlobalId{.host_frame = DEF, .renderer_id = 56},
//     FieldGlobalId{.host_frame = DEF, .renderer_id = 78}
//   }, callback).
// The router splits the groups the fields by their host frame token and routes
// the calls to the respective frame's drivers:
//   callback(abc_driver, {
//     FieldRendererId{.renderer_id = 12},
//     FieldRendererId{.renderer_id = 34},
//   });
//   callback(def_driver, {
//     FieldRendererId{.renderer_id = 56},
//     FieldRendererId{.renderer_id = 78}
//   });
// These callbacks call the agents in the renderer processes:
//   abc_agent->SetFieldsEligibleForManualFilling({
//     FieldRendererId{.renderer_id = 12},
//     FieldRendererId{.renderer_id = 34},
//   });
//   def_agent->SetFieldsEligibleForManualFilling({
//     FieldRendererId{.renderer_id = 56},
//     FieldRendererId{.renderer_id = 78}
//   });
//
// See AutofillDriverRouter for further details.
class ContentAutofillDriver : public AutofillDriver,
                              public mojom::AutofillDriver {
 public:
  // Gets the driver for |render_frame_host|.
  // If |render_frame_host| is currently being deleted, this may be nullptr.
  static ContentAutofillDriver* GetForRenderFrameHost(
      content::RenderFrameHost* render_frame_host);

  // Partially constructs the ContentAutofillDriver: afterwards, the caller
  // *must* set a non-null AutofillManager with set_autofill_manager().
  // Outside of unittests, this is done by ContentAutofillDriverFactory.
  ContentAutofillDriver(content::RenderFrameHost* render_frame_host,
                        ContentAutofillDriverFactory* owner);
  ContentAutofillDriver(const ContentAutofillDriver&) = delete;
  ContentAutofillDriver& operator=(const ContentAutofillDriver&) = delete;
  ~ContentAutofillDriver() override;

  void set_autofill_manager(std::unique_ptr<AutofillManager> autofill_manager) {
    autofill_manager_ = std::move(autofill_manager);
  }

  content::RenderFrameHost* render_frame_host() { return &*render_frame_host_; }
  const content::RenderFrameHost* render_frame_host() const {
    return &*render_frame_host_;
  }

  // Expose the events that originate from the browser and renderer processes,
  // respectively.
  //
  // The purpose of not exposing these events directly in ContentAutofillDriver
  // is to make the caller aware of the event's intended source. This is
  // relevant because renderer forms and browser forms have distinct properties:
  // certain fields are not set in renderer form (see SetFrameAndFormMetaData()
  // for details) and, if they are part of a frame-transcending form, they are
  // not flattened yet (see AutofillDriverRouter for details).
  autofill::AutofillDriver& browser_events() { return *this; }
  mojom::AutofillDriver& renderer_events() { return *this; }

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::AutofillDriver> pending_receiver);
  const mojo::AssociatedRemote<mojom::AutofillAgent>& GetAutofillAgent();

  // autofill::AutofillDriver:
  // These are the non-event functions from autofill::AutofillDriver. The events
  // are defined in the private part below.
  LocalFrameToken GetFrameToken() const override;
  absl::optional<LocalFrameToken> Resolve(FrameToken query) override;
  ContentAutofillDriver* GetParent() override;
  AutofillManager& GetAutofillManager() override;
  bool IsInActiveFrame() const override;
  bool IsInAnyMainFrame() const override;
  bool IsPrerendering() const override;
  bool HasSharedAutofillPermission() const override;
  bool CanShowAutofillUi() const override;
  bool RendererIsAvailable() override;
  void HandleParsedForms(const std::vector<FormData>& forms) override {}
  void PopupHidden() override;
  net::IsolationInfo IsolationInfo() override;

  // Indicates that the `potentially_submitted_form_` has probably been
  // submitted if the feature AutofillProbableFormSubmissionInBrowser is
  // enabled.
  void ProbablyFormSubmitted(base::PassKey<ContentAutofillDriverFactory>);

  // Called on certain types of navigations by ContentAutofillDriverFactory.
  void Reset();

  // Called to inform the browser that in the field with `form_global_id` and
  // `field_global_id`, the context menu was triggered. This is different from
  // the usual Autofill flow where the renderer calls the browser or the browser
  // informs the renderer of some event.
  //
  // This is tricky because the context-menu event may refer to a renderer form
  // in a certain frame, but the form is managed by the AutofillManager of
  // another frame.
  //
  // TODO(crbug.com/1490899): Let callers call AutofillManager directly once
  // AutofillManager is per-tab.
  //
  // Virtual for testing.
  virtual void OnContextMenuShownInField(const FormGlobalId& form_global_id,
                                         const FieldGlobalId& field_global_id);

 private:
  friend class ContentAutofillDriverTestApi;

  // Communication falls into two groups:
  //
  // (1) Browser -> renderer (autofill::AutofillDriver):
  //     These events are triggered by an AutofillManager or similar and are
  //     passed to one or multiple AutofillAgents. They fall into three groups:
  //     (1a) Broadcast events are sent to many AutofillAgents.
  //     (1b) Routed events are sent to a single AutofillAgent, which may
  //          be not this driver's AutofillAgent.
  //     (1c) Unrouted events are sent to this driver's AutofillAgent.
  // (2) Renderer -> browser (mojom::AutofillDriver):
  //     These events are triggered by an AutofillAgent and are passed to one or
  //     multiple AutofillManagers. They fall into two groups:
  //     (2a) Broadcast events are sent to many AutofillManagers.
  //     (2b) Routed events are sent to a single AutofillManager, which may
  //          be not this driver's AutofillManager.
  //
  // These events are private to avoid accidental use in the browser process.
  // Groups (1) and (2) can be accessed explicitly through browser_events() and
  // renderer_events(), respectively.
  //
  // Keep the events of each group in alphabetic order.

  // Group (1a): browser -> renderer events, broadcast (see comment above).
  // autofill::AutofillDriver:
  void TriggerFormExtractionInAllFrames(
      base::OnceCallback<void(bool success)> form_extraction_finished_callback)
      override;
  void RendererShouldClearFilledSection() override;
  void RendererShouldClearPreviewedForm() override;

  // Group (1b): browser -> renderer events, routed (see comment above).
  // autofill::AutofillDriver:
  std::vector<FieldGlobalId> ApplyFormAction(
      mojom::ActionType action_type,
      mojom::ActionPersistence action_persistence,
      const FormData& data,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map)
      override;
  void ApplyFieldAction(mojom::ActionPersistence action_persistence,
                        mojom::TextReplacement text_replacement,
                        const FieldGlobalId& field_id,
                        const std::u16string& value) override;
  void ExtractForm(FormGlobalId form,
                   BrowserFormHandler final_handler) override;
  void RendererShouldAcceptDataListSuggestion(
      const FieldGlobalId& field_id,
      const std::u16string& value) override;
  void RendererShouldSetSuggestionAvailability(
      const FieldGlobalId& field_id,
      const mojom::AutofillState state) override;
  void RendererShouldTriggerSuggestions(
      const FieldGlobalId& field_id,
      AutofillSuggestionTriggerSource trigger_source) override;
  void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) override;
  void SendFieldsEligibleForManualFillingToRenderer(
      const std::vector<FieldGlobalId>& fields) override;

  // Group (1c): browser -> renderer events, unrouted (see comment above).
  // autofill::AutofillDriver:
  // TODO(crbug.com/1281695): This event is currently not routed, but it looks
  // like it should be breadcast to all renderers.
  void GetFourDigitCombinationsFromDOM(
      base::OnceCallback<void(const std::vector<std::string>&)>
          potential_matches) override;
  void TriggerFormExtractionInDriverFrame() override;

  // Group (2a): renderer -> browser events, broadcast (see comment above).
  // mojom::AutofillDriver:
  void DidEndTextFieldEditing() override;
  void FocusNoLongerOnForm(bool had_interacted_form) override;
  void HidePopup() override;

  // Group (2b): renderer -> browser events, routed (see comment above).
  // mojom::AutofillDriver:
  void AskForValuesToFill(
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      AutofillSuggestionTriggerSource trigger_source) override;
  void DidFillAutofillFormData(const FormData& form,
                               base::TimeTicks timestamp) override;
  void FocusOnFormField(const FormData& form,
                        const FormFieldData& field,
                        const gfx::RectF& bounding_box) override;
  void FormsSeen(const std::vector<FormData>& updated_forms,
                 const std::vector<FormRendererId>& removed_forms) override;
  void FormSubmitted(const FormData& form,
                     bool known_success,
                     mojom::SubmissionSource submission_source) override;
  void JavaScriptChangedAutofilledValue(
      const FormData& form,
      const FormFieldData& field,
      const std::u16string& old_value) override;
  void SelectControlDidChange(const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box) override;
  void SelectOrSelectListFieldOptionsDidChange(const FormData& form) override;
  void SetFormToBeProbablySubmitted(
      const absl::optional<FormData>& form) override;
  void TextFieldDidChange(const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box,
                          base::TimeTicks timestamp) override;
  void TextFieldDidScroll(const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box) override;

  // Sets parameters of |form| and |optional_field| that can be extracted from
  // |render_frame_host_|. |optional_field| is treated as if it is a field of
  // |form|.
  //
  // These functions must be called for every FormData and FormFieldData
  // received from the renderer.
  void SetFrameAndFormMetaData(FormData& form,
                               FormFieldData* optional_field) const;
  // Returns a copy of `form` after applying `SetFormAndFormMetaData` to it.
  [[nodiscard]] FormData GetFormWithFrameAndFormMetaData(FormData form) const;
  [[nodiscard]] std::optional<FormData> GetFormWithFrameAndFormMetaData(
      base::optional_ref<const FormData> form) const;

  // Transform bounding box coordinates to real viewport coordinates. In the
  // case of a page spanning multiple renderer processes, subframe renderers
  // cannot do this transformation themselves.
  [[nodiscard]] gfx::RectF TransformBoundingBoxToViewportCoordinates(
      const gfx::RectF& bounding_box) const;

  // Returns the AutofillRouter and confirms that it may be accessed (we should
  // not be using the router if we're prerendering).
  //
  // The router must only route among ContentAutofillDrivers because
  // ContentAutofillDriver casts AutofillDrivers to ContentAutofillDrivers.
  AutofillDriverRouter& router();

  // The frame/document to which this driver is associated. Outlives `this`.
  // RFH is corresponds to neither a frame nor a document: it may survive
  // navigations that documents don't, but it may not survive cross-origin
  // navigations.
  const raw_ref<content::RenderFrameHost> render_frame_host_;

  // The factory that created this driver. Outlives `this`.
  const raw_ref<ContentAutofillDriverFactory> owner_;

  // The form pushed from the AutofillAgent to the AutofillDriver. When the
  // ProbablyFormSubmitted() event is fired, this form is considered the
  // submitted one.
  absl::optional<FormData> potentially_submitted_form_;

  // Keeps track of the forms for which FormSubmitted() event has been triggered
  // to avoid duplicates fired by AutofillAgent.
  std::set<FormGlobalId> submitted_forms_;

  std::unique_ptr<AutofillManager> autofill_manager_ = nullptr;

  mojo::AssociatedReceiver<mojom::AutofillDriver> receiver_{this};

  mojo::AssociatedRemote<mojom::AutofillAgent> autofill_agent_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_
