// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver.h"

#include <utility>
#include <vector>

#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_handler_proxy.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/gfx/geometry/size_f.h"

namespace autofill {

ContentAutofillDriver::ContentAutofillDriver(
    content::RenderFrameHost* render_frame_host,
    AutofillClient* client,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager,
    AutofillProvider* provider)
    : render_frame_host_(render_frame_host),
      autofill_manager_(nullptr),
      key_press_handler_manager_(this),
      binding_(this) {
  // AutofillManager isn't used if provider is valid, Autofill provider is
  // currently used by Android WebView only.
  if (provider) {
    SetAutofillProvider(provider);
  } else {
    autofill_handler_ = std::make_unique<AutofillManager>(
        this, client, app_locale, enable_download_manager);
    autofill_manager_ = static_cast<AutofillManager*>(autofill_handler_.get());
    autofill_external_delegate_ =
        std::make_unique<AutofillExternalDelegate>(autofill_manager_, this);
    autofill_manager_->SetExternalDelegate(autofill_external_delegate_.get());
  }
}

ContentAutofillDriver::~ContentAutofillDriver() {}

// static
ContentAutofillDriver* ContentAutofillDriver::GetForRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  ContentAutofillDriverFactory* factory =
      ContentAutofillDriverFactory::FromWebContents(
          content::WebContents::FromRenderFrameHost(render_frame_host));
  return factory ? factory->DriverForFrame(render_frame_host) : nullptr;
}

void ContentAutofillDriver::BindRequest(
    mojom::AutofillDriverAssociatedRequest request) {
  binding_.Bind(std::move(request));
}

bool ContentAutofillDriver::IsIncognito() const {
  return render_frame_host_->GetSiteInstance()
      ->GetBrowserContext()
      ->IsOffTheRecord();
}

bool ContentAutofillDriver::IsInMainFrame() const {
  return render_frame_host_->GetParent() == nullptr;
}

net::URLRequestContextGetter* ContentAutofillDriver::GetURLRequestContext() {
  return content::BrowserContext::GetDefaultStoragePartition(
      render_frame_host_->GetSiteInstance()->GetBrowserContext())->
          GetURLRequestContext();
}

scoped_refptr<network::SharedURLLoaderFactory>
ContentAutofillDriver::GetURLLoaderFactory() {
  return content::BrowserContext::GetDefaultStoragePartition(
             render_frame_host_->GetSiteInstance()->GetBrowserContext())
      ->GetURLLoaderFactoryForBrowserProcess();
}

bool ContentAutofillDriver::RendererIsAvailable() {
  return render_frame_host_->GetRenderViewHost() != nullptr;
}

void ContentAutofillDriver::SendFormDataToRenderer(
    int query_id,
    RendererFormDataAction action,
    const FormData& data) {
  if (!RendererIsAvailable())
    return;

  switch (action) {
    case FORM_DATA_ACTION_FILL:
      GetAutofillAgent()->FillForm(query_id, data);
      break;
    case FORM_DATA_ACTION_PREVIEW:
      GetAutofillAgent()->PreviewForm(query_id, data);
      break;
  }
}

void ContentAutofillDriver::PropagateAutofillPredictions(
    const std::vector<FormStructure*>& forms) {
  autofill_manager_->client()->PropagateAutofillPredictions(render_frame_host_,
                                                            forms);
}

void ContentAutofillDriver::SendAutofillTypePredictionsToRenderer(
    const std::vector<FormStructure*>& forms) {
  if (!RendererIsAvailable())
    return;

  std::vector<FormDataPredictions> type_predictions =
      FormStructure::GetFieldTypePredictions(forms);
  GetAutofillAgent()->FieldTypePredictionsAvailable(type_predictions);
}

void ContentAutofillDriver::RendererShouldAcceptDataListSuggestion(
    const base::string16& value) {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->AcceptDataListSuggestion(value);
}

void ContentAutofillDriver::RendererShouldClearFilledSection() {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->ClearSection();
}

void ContentAutofillDriver::RendererShouldClearPreviewedForm() {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->ClearPreviewedForm();
}

void ContentAutofillDriver::RendererShouldFillFieldWithValue(
    const base::string16& value) {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->FillFieldWithValue(value);
}

void ContentAutofillDriver::RendererShouldPreviewFieldWithValue(
    const base::string16& value) {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->PreviewFieldWithValue(value);
}

void ContentAutofillDriver::PopupHidden() {
  // If the unmask prompt is showing, keep showing the preview. The preview
  // will be cleared when the prompt closes.
  if (autofill_manager_ && !autofill_manager_->IsShowingUnmaskPrompt())
    RendererShouldClearPreviewedForm();
}

gfx::RectF ContentAutofillDriver::TransformBoundingBoxToViewportCoordinates(
    const gfx::RectF& bounding_box) {
  content::RenderWidgetHostView* view = render_frame_host_->GetView();
  if (!view)
    return bounding_box;

  gfx::PointF orig_point(bounding_box.x(), bounding_box.y());
  gfx::PointF transformed_point =
      view->TransformPointToRootCoordSpaceF(orig_point);
  return gfx::RectF(transformed_point.x(), transformed_point.y(),
                    bounding_box.width(), bounding_box.height());
}

void ContentAutofillDriver::DidInteractWithCreditCardForm() {
  // If there is an autofill manager, notify its client about credit card
  // inputs on non-secure pages.
  if (!autofill_manager_)
    return;
  if (content::IsOriginSecure(
          content::WebContents::FromRenderFrameHost(render_frame_host_)
              ->GetVisibleURL())) {
    return;
  }
  autofill_manager_->client()->DidInteractWithNonsecureCreditCardInput();
}

void ContentAutofillDriver::FormsSeen(const std::vector<FormData>& forms,
                                      base::TimeTicks timestamp) {
  autofill_handler_->OnFormsSeen(forms, timestamp);
}

void ContentAutofillDriver::FormSubmitted(const FormData& form,
                                          bool known_success,
                                          SubmissionSource source,
                                          base::TimeTicks timestamp) {
  autofill_handler_->OnFormSubmitted(form, known_success, source, timestamp);
}

void ContentAutofillDriver::TextFieldDidChange(const FormData& form,
                                               const FormFieldData& field,
                                               const gfx::RectF& bounding_box,
                                               base::TimeTicks timestamp) {
  autofill_handler_->OnTextFieldDidChange(form, field, bounding_box, timestamp);
}

void ContentAutofillDriver::TextFieldDidScroll(const FormData& form,
                                               const FormFieldData& field,
                                               const gfx::RectF& bounding_box) {
  autofill_handler_->OnTextFieldDidScroll(form, field, bounding_box);
}

void ContentAutofillDriver::SelectControlDidChange(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  autofill_handler_->OnSelectControlDidChange(form, field, bounding_box);
}

void ContentAutofillDriver::QueryFormFieldAutofill(
    int32_t id,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    bool autoselect_first_suggestion) {
  autofill_handler_->OnQueryFormFieldAutofill(id, form, field, bounding_box,
                                              autoselect_first_suggestion);
}

void ContentAutofillDriver::HidePopup() {
  autofill_handler_->OnHidePopup();
}

void ContentAutofillDriver::FocusNoLongerOnForm() {
  autofill_handler_->OnFocusNoLongerOnForm();
}

void ContentAutofillDriver::FocusOnFormField(const FormData& form,
                                             const FormFieldData& field,
                                             const gfx::RectF& bounding_box) {
  autofill_handler_->OnFocusOnFormField(form, field, bounding_box);
}

void ContentAutofillDriver::DidFillAutofillFormData(const FormData& form,
                                                    base::TimeTicks timestamp) {
  autofill_handler_->OnDidFillAutofillFormData(form, timestamp);
}

void ContentAutofillDriver::DidPreviewAutofillFormData() {
  autofill_handler_->OnDidPreviewAutofillFormData();
}

void ContentAutofillDriver::DidEndTextFieldEditing() {
  autofill_handler_->OnDidEndTextFieldEditing();
}

void ContentAutofillDriver::SetDataList(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {
  autofill_handler_->OnSetDataList(values, labels);
}

void ContentAutofillDriver::SelectFieldOptionsDidChange(const FormData& form) {
  autofill_handler_->SelectFieldOptionsDidChange(form);
}

void ContentAutofillDriver::DidNavigateMainFrame(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument())
    return;

  autofill_handler_->Reset();
}

void ContentAutofillDriver::SetAutofillManager(
    std::unique_ptr<AutofillManager> manager) {
  CHECK(autofill_manager_);
  autofill_handler_ = std::move(manager);
  autofill_manager_ = static_cast<AutofillManager*>(autofill_handler_.get());
  autofill_manager_->SetExternalDelegate(autofill_external_delegate_.get());
}

const mojom::AutofillAgentAssociatedPtr&
ContentAutofillDriver::GetAutofillAgent() {
  // Here is a lazy binding, and will not reconnect after connection error.
  if (!autofill_agent_) {
    render_frame_host_->GetRemoteAssociatedInterfaces()->GetInterface(
        mojo::MakeRequest(&autofill_agent_));
  }

  return autofill_agent_;
}

void ContentAutofillDriver::RegisterKeyPressHandler(
    const content::RenderWidgetHost::KeyPressEventCallback& handler) {
  key_press_handler_manager_.RegisterKeyPressHandler(handler);
}

void ContentAutofillDriver::RemoveKeyPressHandler() {
  key_press_handler_manager_.RemoveKeyPressHandler();
}

void ContentAutofillDriver::AddHandler(
    const content::RenderWidgetHost::KeyPressEventCallback& handler) {
  content::RenderWidgetHostView* view = render_frame_host_->GetView();
  if (!view)
    return;
  view->GetRenderWidgetHost()->AddKeyPressEventCallback(handler);
}

void ContentAutofillDriver::RemoveHandler(
    const content::RenderWidgetHost::KeyPressEventCallback& handler) {
  content::RenderWidgetHostView* view = render_frame_host_->GetView();
  if (!view)
    return;
  view->GetRenderWidgetHost()->RemoveKeyPressEventCallback(handler);
}

void ContentAutofillDriver::SetAutofillProvider(AutofillProvider* provider) {
  autofill_handler_ = std::make_unique<AutofillHandlerProxy>(this, provider);
  GetAutofillAgent()->SetUserGestureRequired(false);
  GetAutofillAgent()->SetSecureContextRequired(true);
  GetAutofillAgent()->SetFocusRequiresScroll(false);
  GetAutofillAgent()->SetQueryPasswordSuggestion(true);
}

void ContentAutofillDriver::SetAutofillProviderForTesting(
    AutofillProvider* provider) {
  SetAutofillProvider(provider);
}

}  // namespace autofill
