// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace autofill {

class ContentAutofillDriverFactory;
class ContentAutofillRouter;

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
// ContentAutofillRouter, which has one instance per WebContents. The naming
// pattern is that for all of these events, there are two functions:
//
//   1. ReturnType ContentAutofillDriver::f(Args...)
//   2. ReturnType ContentAutofillRouter::f(ContentAutofillDriver*, Args...,
//                                          Callback)
//
// The first function calls the second, and the second calls the third, perhaps
// for a different ContentAutofillDriver.
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
// See ContentAutofillRouter for further details.
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
                        ContentAutofillRouter* autofill_router);
  ContentAutofillDriver(const ContentAutofillDriver&) = delete;
  ContentAutofillDriver& operator=(const ContentAutofillDriver&) = delete;
  ~ContentAutofillDriver() override;

  void set_autofill_manager(std::unique_ptr<AutofillManager> autofill_manager) {
    autofill_manager_ = std::move(autofill_manager);
  }
  AutofillManager* autofill_manager() { return autofill_manager_.get(); }

  content::RenderFrameHost* render_frame_host() { return render_frame_host_; }

  // Expose the events that originate from the browser and renderer processes,
  // respectively.
  //
  // The purpose of not exposing these events directly in ContentAutofillDriver
  // is to make the caller aware of the event's intended source. This is
  // relevant because renderer forms and browser forms have distinct properties:
  // certain fields are not set in renderer form (see SetFrameAndFormMetaData()
  // for details) and, if they are part of a frame-transcending form, they are
  // not flattened yet (see ContentAutofillRouter for details).
  autofill::AutofillDriver& browser_events() { return *this; }
  mojom::AutofillDriver& renderer_events() { return *this; }

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::AutofillDriver> pending_receiver);
  const mojo::AssociatedRemote<mojom::AutofillAgent>& GetAutofillAgent();

  // autofill::AutofillDriver:
  // These are the non-event functions from autofill::AutofillDriver. The events
  // are defined in the private part below.
  bool IsInActiveFrame() const override;
  bool IsInAnyMainFrame() const override;
  bool IsPrerendering() const override;
  bool CanShowAutofillUi() const override;
  ui::AXTreeID GetAxTreeId() const override;
  bool RendererIsAvailable() override;
  void HandleParsedForms(const std::vector<FormData>& forms) override {}
  void PopupHidden() override;
  net::IsolationInfo IsolationInfo() override;
  void SetShouldSuppressKeyboard(bool suppress) override;

  // Called to inform the browser that in the field with `form_global_id` and
  // `field_global_id`, the context menu was triggered. This is different from
  // the usual Autofill flow where the renderer calls the browser or the browser
  // informs the renderer of some event.
  virtual void OnContextMenuShownInField(const FormGlobalId& form_global_id,
                                         const FieldGlobalId& field_global_id);

  // Triggers a reparse of the new forms in the AutofillAgent. This is necessary
  // when a form is seen in a child frame and it is not known which form is its
  // parent.
  //
  // Generally, this may happen because AutofillAgent is only notified about
  // newly created form control elements.
  //
  // For example, consider a parent frame with a form that contains an <iframe>.
  // Suppose the parent form is seen (processed by AutofillDriver::FormsSeen())
  // before the iframe is loaded. Loading a cross-origin page into the iframe
  // changes the iframe's frame token. Then, the frame token in the parent
  // form's FormData::child_frames is outdated. When a form is seen in the child
  // frame, it is not known *which* form in the parent frame is its parent
  // form. In this scenario, a reparse is triggered.
  //
  // Virtual for testing.
  virtual void TriggerReparse();

  // Indicates that the `potentially_submitted_form_` has probably been
  // submitted if the feature AutofillProbableFormSubmissionInBrowser is
  // enabled.
  void ProbablyFormSubmitted(base::PassKey<ContentAutofillDriverFactory>);

  // DidNavigateFrame() is called on the frame's driver, respectively, when a
  // navigation occurs in that specific frame.
  void DidNavigateFrame(content::NavigationHandle* navigation_handle);

  // Key-press handlers capture the user input into fields from the renderer.
  // The AutofillPopupControllerImpl listens for input while showing a popup.
  // That way, the user can select suggestions from the popup, for example.
  //
  // In a frame-transcending form, the <input> the user queried Autofill from
  // may be in a different frame than |render_frame_host_|. Therefore,
  // SetKeyPressHandler() and UnsetKeyPressHandler() are forwarded to the
  // last-queried source remembered by ContentAutofillRouter.
  // For non-Autofill forms (i.e., password forms), which are not handled by
  // ContentAutofillDriver and ContentAutofillRouter and hence are not
  // frame-transcending, this routing must be skipped by setting |skip_routing|.
  void SetKeyPressHandler(
      const content::RenderWidgetHost::KeyPressEventCallback& handler);
  void UnsetKeyPressHandler();

  // Callbacks that are called also in other functions by ContentAutofillRouter.
  void FocusNoLongerOnFormCallback(bool had_interacted_form);
  void UnsetKeyPressHandlerCallback();
  void SetShouldSuppressKeyboardCallback(bool suppress);
  void OnContextMenuShownInFieldCallback(const FormGlobalId& form_global_id,
                                         const FieldGlobalId& field_global_id);

 private:
  friend class ContentAutofillDriverTestApi;

  // autofill::AutofillDriver:
  // Events triggered by the browser. These events are routed by
  // ContentAutofillRouter to potentially a different ContentAutofillDriver and
  // then passed to AutofillAgent in the renderer.
  //
  // These events are private to to avoid accidental in the browser.
  // They can be accessed explicitly through browser_events().
  std::vector<FieldGlobalId> FillOrPreviewForm(
      mojom::RendererFormDataAction action,
      const FormData& data,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map)
      override;
  void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) override;
  void RendererShouldAcceptDataListSuggestion(
      const FieldGlobalId& field_id,
      const std::u16string& value) override;
  void RendererShouldClearFilledSection() override;
  void RendererShouldClearPreviewedForm() override;
  void RendererShouldFillFieldWithValue(const FieldGlobalId& field_id,
                                        const std::u16string& value) override;
  void RendererShouldPreviewFieldWithValue(
      const FieldGlobalId& field_id,
      const std::u16string& value) override;
  void RendererShouldSetSuggestionAvailability(
      const FieldGlobalId& field_id,
      const mojom::AutofillState state) override;
  void SendFieldsEligibleForManualFillingToRenderer(
      const std::vector<FieldGlobalId>& fields) override;
  void TriggerReparseInAllFrames(
      base::OnceCallback<void(bool success)> trigger_reparse_finished_callback)
      override;

  // mojom::AutofillDriver:
  // Events triggered by the renderer. These events are routed by
  // ContentAutofillRouter to potentially a different ContentAutofillDriver and
  // then passed to AutofillManager.
  //
  // We do not expect to receive Autofill related messages from a prerendered
  // page, so we validate calls accordingly. If we receive an unexpected call,
  // we shut down the renderer and log the bad message.
  //
  // These events are private to to avoid accidental in the browser.
  // They can be accessed explicitly through renderer_events().
  void SetFormToBeProbablySubmitted(
      const absl::optional<FormData>& form) override;
  void FormsSeen(const std::vector<FormData>& updated_forms,
                 const std::vector<FormRendererId>& removed_forms) override;
  void FormSubmitted(const FormData& form,
                     bool known_success,
                     mojom::SubmissionSource submission_source) override;
  void TextFieldDidChange(const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box,
                          base::TimeTicks timestamp) override;
  void TextFieldDidScroll(const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box) override;
  void SelectControlDidChange(const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box) override;
  void AskForValuesToFill(
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      AutoselectFirstSuggestion autoselect_first_suggestion,
      FormElementWasClicked form_element_was_clicked) override;
  void HidePopup() override;
  void FocusNoLongerOnForm(bool had_interacted_form) override;
  void FocusOnFormField(const FormData& form,
                        const FormFieldData& field,
                        const gfx::RectF& bounding_box) override;
  void DidFillAutofillFormData(const FormData& form,
                               base::TimeTicks timestamp) override;
  void DidPreviewAutofillFormData() override;
  void DidEndTextFieldEditing() override;
  void SelectFieldOptionsDidChange(const FormData& form) override;
  void JavaScriptChangedAutofilledValue(
      const FormData& form,
      const FormFieldData& field,
      const std::u16string& old_value) override;

  // Sets parameters of |form| and |optional_field| that can be extracted from
  // |render_frame_host_|. |optional_field| is treated as if it is a field of
  // |form|.
  //
  // These functions must be called for every FormData and FormFieldData
  // received from the renderer.
  void SetFrameAndFormMetaData(FormData& form,
                               FormFieldData* optional_field) const;
  [[nodiscard]] FormData GetFormWithFrameAndFormMetaData(FormData form) const;

  // Transform bounding box coordinates to real viewport coordinates. In the
  // case of a page spanning multiple renderer processes, subframe renderers
  // cannot do this transformation themselves.
  [[nodiscard]] gfx::RectF TransformBoundingBoxToViewportCoordinates(
      const gfx::RectF& bounding_box) const;

  // Returns the AutofillRouter and confirms that it may be accessed (we should
  // not be using the router if we're prerendering).
  //
  // Also DCHECKs that the driver is in a state where events may be handled.
  ContentAutofillRouter& autofill_router();

  // Weak ref to the RenderFrameHost the driver is associated with. Should
  // always be non-NULL and valid for lifetime of |this|.
  const raw_ptr<content::RenderFrameHost> render_frame_host_ = nullptr;

  // Weak ref to the AutofillRouter associated with the WebContents.
  // Do not access directly, use autofill_router() instead.
  raw_ptr<ContentAutofillRouter> autofill_router_ = nullptr;

  // The form pushed from the AutofillAgent to the AutofillDriver. When the
  // ProbablyFormSubmitted() event is fired, this form is considered the
  // submitted one.
  absl::optional<FormData> potentially_submitted_form_;

  // Keeps track of the forms for which FormSubmitted() event has been triggered
  // to avoid duplicates fired by AutofillAgent.
  std::set<FormGlobalId> submitted_forms_;

  // AutofillManager instance via which this object drives the shared Autofill
  // code.
  std::unique_ptr<AutofillManager> autofill_manager_ = nullptr;

  content::RenderWidgetHost::KeyPressEventCallback key_press_handler_;

  mojo::AssociatedReceiver<mojom::AutofillDriver> receiver_{this};

  mojo::AssociatedRemote<mojom::AutofillAgent> autofill_agent_;

  bool should_suppress_keyboard_ = false;

  content::RenderWidgetHost::SuppressShowingImeCallback
      suppress_showing_ime_callback_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_
