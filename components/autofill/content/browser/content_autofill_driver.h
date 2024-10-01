// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/types/optional_ref.h"
#include "components/autofill/content/browser/content_autofill_client.h"
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
// pattern is that for all of these events, there are two functions:
//
//   1. ReturnType ContentAutofillDriver::Foo(Args...)
//   2. ReturnType AutofillDriverRouter::Foo(RoutedCallback, Args...)
//
// The first function calls the second, and the second calls the callback.
// That callback takes a target AutofillDriver, which may be different from the
// first function's ContentAutofillDriver.
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
  class ContentAutofillDriverFactoryPassKey {
   private:
    friend class ContentAutofillDriverFactory;
    friend class ContentAutofillDriverTestApi;
    ContentAutofillDriverFactoryPassKey() = default;
  };

  // Gets the driver for |render_frame_host|.
  // If |render_frame_host| is currently being deleted, this may be nullptr.
  static ContentAutofillDriver* GetForRenderFrameHost(
      content::RenderFrameHost* render_frame_host);

  // Part of the initialization may be embedder-specific, implemented in
  // ContentAutofillClient::CreateManager().
  ContentAutofillDriver(content::RenderFrameHost* render_frame_host,
                        ContentAutofillDriverFactory* owner);
  ContentAutofillDriver(const ContentAutofillDriver&) = delete;
  ContentAutofillDriver& operator=(const ContentAutofillDriver&) = delete;
  ~ContentAutofillDriver() override;

  // Clears the driver's and the manager's stored forms and other state,
  // *except* for the LifecycleState, which is controlled by the
  // AutofillDriverFactory. Called on certain types of navigations.
  void Reset(ContentAutofillDriverFactoryPassKey pass_key);

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
  std::optional<LocalFrameToken> Resolve(FrameToken query) override;
  ContentAutofillDriver* GetParent() override;
  ContentAutofillClient& GetAutofillClient() override;
  AutofillManager& GetAutofillManager() override;
  bool IsActive() const override;
  bool IsInAnyMainFrame() const override;
  bool HasSharedAutofillPermission() const override;
  bool CanShowAutofillUi() const override;
  std::optional<net::IsolationInfo> GetIsolationInfo() override;

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
  //     (1c) Main-frame events are sent to the driver's main frame's
  //          AutofillAgent.
  //     (1d) Unrouted events are sent to this driver's AutofillAgent.
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
  void RendererShouldClearPreviewedForm() override;

  // Group (1b): browser -> renderer events, routed (see comment above).
  // autofill::AutofillDriver:
  base::flat_set<FieldGlobalId> ApplyFormAction(
      mojom::FormActionType action_type,
      mojom::ActionPersistence action_persistence,
      base::span<const FormFieldData> data,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, FieldType>& field_type_map) override;
  void ApplyFieldAction(mojom::FieldActionType action_type,
                        mojom::ActionPersistence action_persistence,
                        const FieldGlobalId& field_id,
                        const std::u16string& value) override;
  void ExtractForm(FormGlobalId form,
                   BrowserFormHandler final_handler) override;
  void RendererShouldAcceptDataListSuggestion(
      const FieldGlobalId& field_id,
      const std::u16string& value) override;
  void RendererShouldSetSuggestionAvailability(
      const FieldGlobalId& field_id,
      mojom::AutofillSuggestionAvailability suggestion_availability) override;
  void RendererShouldTriggerSuggestions(
      const FieldGlobalId& field_id,
      AutofillSuggestionTriggerSource trigger_source) override;
  void SendTypePredictionsToRenderer(
      const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms)
      override;

  // Group (1c): browser -> renderer events, directed to to this driver's main
  // driver (see comment above).
  // autofill::AutofillDriver:
  void GetFourDigitCombinationsFromDom(
      base::OnceCallback<void(const std::vector<std::string>&)>
          potential_matches) override;

  // Group (1d): browser -> renderer events, unrouted (see comment above).
  // autofill::AutofillDriver:
  void TriggerFormExtractionInDriverFrame(
      AutofillDriverRouterAndFormForestPassKey pass_key) override;

  // Group (2a): renderer -> browser events, broadcast (see comment above).
  // mojom::AutofillDriver:
  void DidEndTextFieldEditing() override;
  void FocusOnNonFormField() override;
  void HidePopup() override;

  // Group (2b): renderer -> browser events, routed (see comment above).
  // mojom::AutofillDriver:
  void AskForValuesToFill(
      const FormData& form,
      FieldRendererId field_id,
      const gfx::Rect& caret_bounds,
      AutofillSuggestionTriggerSource trigger_source) override;
  void DidFillAutofillFormData(const FormData& form,
                               base::TimeTicks timestamp) override;
  void FocusOnFormField(const FormData& form,
                        FieldRendererId field_id) override;
  void FormsSeen(const std::vector<FormData>& updated_forms,
                 const std::vector<FormRendererId>& removed_forms) override;
  void FormSubmitted(const FormData& form,
                     bool known_success,
                     mojom::SubmissionSource submission_source) override;
  void JavaScriptChangedAutofilledValue(const FormData& form,
                                        FieldRendererId field_id,
                                        const std::u16string& old_value,
                                        bool formatting_only) override;
  void SelectControlDidChange(const FormData& form,
                              FieldRendererId field_id) override;
  void SelectFieldOptionsDidChange(const FormData& form) override;
  void CaretMovedInFormField(const FormData& form,
                             FieldRendererId field_id,
                             const gfx::Rect& caret_bounds) override;
  void TextFieldDidChange(const FormData& form,
                          FieldRendererId field_id,
                          base::TimeTicks timestamp) override;
  void TextFieldDidScroll(const FormData& form,
                          FieldRendererId field_id) override;

  void LiftForTest(FormData& form);

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

  mojo::AssociatedReceiver<mojom::AutofillDriver> receiver_{this};

  mojo::AssociatedRemote<mojom::AutofillAgent> autofill_agent_;

  std::unique_ptr<AutofillManager> autofill_manager_ = nullptr;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_
