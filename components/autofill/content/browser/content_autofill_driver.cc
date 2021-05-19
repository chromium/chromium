// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/version_info/channel.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/size_f.h"
#include "url/origin.h"

namespace autofill {

namespace {

bool ShouldEnableHeavyFormDataScraping(const version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
      return true;
    case version_info::Channel::STABLE:
    case version_info::Channel::BETA:
    case version_info::Channel::UNKNOWN:
      return false;
  }
  NOTREACHED();
  return false;
}

GURL StripAuthAndParams(const GURL& gurl) {
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  rep.ClearQuery();
  rep.ClearRef();
  return gurl.ReplaceComponents(rep);
}

}  // namespace

ContentAutofillDriver::ContentAutofillDriver(
    content::RenderFrameHost* render_frame_host,
    AutofillClient* client,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager,
    AutofillManager::AutofillManagerFactoryCallback
        autofill_manager_factory_callback)
    : render_frame_host_(render_frame_host),
      browser_autofill_manager_(nullptr),
      key_press_handler_manager_(this),
      log_manager_(client->GetLogManager()) {
  // AutofillManager isn't used if provider is valid, Autofill provider is
  // currently used by Android WebView only.
  if (autofill_manager_factory_callback) {
    autofill_manager_ = autofill_manager_factory_callback.Run(
        this, client, app_locale, enable_download_manager);
    GetAutofillAgent()->SetUserGestureRequired(false);
    GetAutofillAgent()->SetSecureContextRequired(true);
    GetAutofillAgent()->SetFocusRequiresScroll(false);
    GetAutofillAgent()->SetQueryPasswordSuggestion(true);
  } else {
    SetBrowserAutofillManager(std::make_unique<BrowserAutofillManager>(
        this, client, app_locale, enable_download_manager));
  }
  if (client && ShouldEnableHeavyFormDataScraping(client->GetChannel())) {
    GetAutofillAgent()->EnableHeavyFormDataScraping();
  }
}

ContentAutofillDriver::~ContentAutofillDriver() = default;

// static
ContentAutofillDriver* ContentAutofillDriver::GetForRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  ContentAutofillDriverFactory* factory =
      ContentAutofillDriverFactory::FromWebContents(
          content::WebContents::FromRenderFrameHost(render_frame_host));
  return factory ? factory->DriverForFrame(render_frame_host) : nullptr;
}

void ContentAutofillDriver::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::AutofillDriver> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

bool ContentAutofillDriver::IsIncognito() const {
  // TODO(https://crbug.com/1125474): Enable Autofill for Ephemeral Guest
  // profiles.
  // TODO(https://crbug.com/1125474): Consider renaming this function to
  // |IsOffTheRecord| after deprecation of off-the-record or ephemeral Guest
  // profiles.
  auto* browser_context =
      render_frame_host_->GetSiteInstance()->GetBrowserContext();
  if (profile_metrics::GetBrowserProfileType(browser_context) ==
      profile_metrics::BrowserProfileType::kEphemeralGuest) {
    return true;
  }

  return browser_context->IsOffTheRecord();
}

bool ContentAutofillDriver::IsInMainFrame() const {
  return render_frame_host_->GetParent() == nullptr;
}

bool ContentAutofillDriver::CanShowAutofillUi() const {
  // Don't show AutofillUi for non-current RenderFrameHost. Here it is safe to
  // ignore the calls from inactive RFH as the renderer is not expecting a reply
  // and it doesn't lead to browser-renderer consistency issues.
  return render_frame_host_->IsCurrent();
}

ui::AXTreeID ContentAutofillDriver::GetAxTreeId() const {
  return render_frame_host_->GetAXTreeID();
}

scoped_refptr<network::SharedURLLoaderFactory>
ContentAutofillDriver::GetURLLoaderFactory() {
  return render_frame_host_->GetSiteInstance()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

bool ContentAutofillDriver::RendererIsAvailable() {
  return render_frame_host_->GetRenderViewHost() != nullptr;
}

InternalAuthenticator*
ContentAutofillDriver::GetOrCreateCreditCardInternalAuthenticator() {
  if (!authenticator_impl_ && browser_autofill_manager_ &&
      browser_autofill_manager_->client()) {
    authenticator_impl_ =
        browser_autofill_manager_->client()
            ->CreateCreditCardInternalAuthenticator(render_frame_host_);
  }
  return authenticator_impl_.get();
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
  AutofillManager* manager = browser_autofill_manager_
                                 ? browser_autofill_manager_
                                 : autofill_manager_.get();
  DCHECK(manager);
  manager->PropagateAutofillPredictions(render_frame_host_, forms);
}

void ContentAutofillDriver::HandleParsedForms(
    const std::vector<const FormData*>& forms) {
  // No op.
}

void ContentAutofillDriver::SendAutofillTypePredictionsToRenderer(
    const std::vector<FormStructure*>& forms) {
  if (!RendererIsAvailable())
    return;
  // TODO(crbug.com/1185232) Send the FormDataPredictions object only if the
  // debugging flag is enabled.
  std::vector<FormDataPredictions> type_predictions =
      FormStructure::GetFieldTypePredictions(forms);
  GetAutofillAgent()->FieldTypePredictionsAvailable(type_predictions);
}

void ContentAutofillDriver::SendFieldsEligibleForManualFillingToRenderer(
    const std::vector<FieldRendererId>& fields) {
  if (!RendererIsAvailable())
    return;

  GetAutofillAgent()->SetFieldsEligibleForManualFilling(fields);
}

void ContentAutofillDriver::RendererShouldAcceptDataListSuggestion(
    const FieldGlobalId& field,
    const std::u16string& value) {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->AcceptDataListSuggestion(field.renderer_id, value);
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
    const FieldGlobalId& field,
    const std::u16string& value) {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->FillFieldWithValue(field.renderer_id, value);
}

void ContentAutofillDriver::RendererShouldPreviewFieldWithValue(
    const FieldGlobalId& field,
    const std::u16string& value) {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->PreviewFieldWithValue(field.renderer_id, value);
}

void ContentAutofillDriver::RendererShouldSetSuggestionAvailability(
    const FieldGlobalId& field,
    const mojom::AutofillState state) {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->SetSuggestionAvailability(field.renderer_id, state);
}

void ContentAutofillDriver::PopupHidden() {
  // If the unmask prompt is showing, keep showing the preview. The preview
  // will be cleared when the prompt closes.
  if (browser_autofill_manager_ &&
      browser_autofill_manager_->ShouldClearPreviewedForm())
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

net::IsolationInfo ContentAutofillDriver::IsolationInfo() {
  return render_frame_host_->GetPendingIsolationInfoForSubresources();
}

void ContentAutofillDriver::SetFormToBeProbablySubmitted(
    const absl::optional<FormData>& raw_form) {
  potentially_submitted_form_ =
      raw_form ? absl::make_optional<FormData>(
                     GetFormWithFrameAndFormMetaData(*raw_form))
               : absl::nullopt;
}

void ContentAutofillDriver::FormsSeen(const std::vector<FormData>& raw_forms) {
  std::vector<FormData> forms = raw_forms;
  for (auto& form : forms)
    SetFrameAndFormMetaData(form);
  autofill_manager_->OnFormsSeen(forms);
}

void ContentAutofillDriver::ProbablyFormSubmitted() {
  if (potentially_submitted_form_.has_value()) {
    FormSubmitted(potentially_submitted_form_.value(), false,
                  mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED);
  }
}

void ContentAutofillDriver::FormSubmitted(const FormData& raw_form,
                                          bool known_success,
                                          mojom::SubmissionSource source) {
  // Omit duplicate form submissions. It may be reasonable to take |source|
  // into account here as well.
  // TODO(crbug/1117451): Clean up experiment code.
  if (base::FeatureList::IsEnabled(
          features::kAutofillProbableFormSubmissionInBrowser) &&
      !base::FeatureList::IsEnabled(
          features::kAutofillAllowDuplicateFormSubmissions) &&
      !submitted_forms_.insert(raw_form.unique_renderer_id).second) {
    return;
  }

  autofill_manager_->OnFormSubmitted(GetFormWithFrameAndFormMetaData(raw_form),
                                     known_success, source);
}

void ContentAutofillDriver::TextFieldDidChange(const FormData& raw_form,
                                               const FormFieldData& raw_field,
                                               const gfx::RectF& bounding_box,
                                               base::TimeTicks timestamp) {
  autofill_manager_->OnTextFieldDidChange(
      GetFormWithFrameAndFormMetaData(raw_form),
      GetFieldWithFrameAndFormMetaData(raw_field), bounding_box, timestamp);
}

void ContentAutofillDriver::TextFieldDidScroll(const FormData& raw_form,
                                               const FormFieldData& raw_field,
                                               const gfx::RectF& bounding_box) {
  autofill_manager_->OnTextFieldDidScroll(
      GetFormWithFrameAndFormMetaData(raw_form),
      GetFieldWithFrameAndFormMetaData(raw_field), bounding_box);
}

void ContentAutofillDriver::SelectControlDidChange(
    const FormData& raw_form,
    const FormFieldData& raw_field,
    const gfx::RectF& bounding_box) {
  autofill_manager_->OnSelectControlDidChange(
      GetFormWithFrameAndFormMetaData(raw_form),
      GetFieldWithFrameAndFormMetaData(raw_field), bounding_box);
}

void ContentAutofillDriver::QueryFormFieldAutofill(
    int32_t id,
    const FormData& raw_form,
    const FormFieldData& raw_field,
    const gfx::RectF& bounding_box,
    bool autoselect_first_suggestion) {
  autofill_manager_->OnQueryFormFieldAutofill(
      id, GetFormWithFrameAndFormMetaData(raw_form),
      GetFieldWithFrameAndFormMetaData(raw_field), bounding_box,
      autoselect_first_suggestion);
}

void ContentAutofillDriver::HidePopup() {
  autofill_manager_->OnHidePopup();
}

void ContentAutofillDriver::FocusNoLongerOnForm(bool had_interacted_form) {
  autofill_manager_->OnFocusNoLongerOnForm(had_interacted_form);
}

void ContentAutofillDriver::FocusOnFormField(const FormData& raw_form,
                                             const FormFieldData& raw_field,
                                             const gfx::RectF& bounding_box) {
  autofill_manager_->OnFocusOnFormField(
      GetFormWithFrameAndFormMetaData(raw_form),
      GetFieldWithFrameAndFormMetaData(raw_field), bounding_box);
}

void ContentAutofillDriver::DidFillAutofillFormData(const FormData& raw_form,
                                                    base::TimeTicks timestamp) {
  autofill_manager_->OnDidFillAutofillFormData(
      GetFormWithFrameAndFormMetaData(raw_form), timestamp);
}

void ContentAutofillDriver::DidPreviewAutofillFormData() {
  autofill_manager_->OnDidPreviewAutofillFormData();
}

void ContentAutofillDriver::DidEndTextFieldEditing() {
  autofill_manager_->OnDidEndTextFieldEditing();
}

void ContentAutofillDriver::SelectFieldOptionsDidChange(
    const FormData& raw_form) {
  autofill_manager_->SelectFieldOptionsDidChange(
      GetFormWithFrameAndFormMetaData(raw_form));
}

void ContentAutofillDriver::DidNavigateFrame(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument()) {
    // On page refresh, reset the rate limiter for fetching authentication
    // details for credit card unmasking.
    if (browser_autofill_manager_) {
      browser_autofill_manager_->credit_card_access_manager()
          ->SignalCanFetchUnmaskDetails();
    }
    return;
  }

  ShowOfferNotificationIfApplicable(navigation_handle);

  // When IsServedFromBackForwardCache, the form data is not parsed
  // again. So, we should keep and use the autofill manager's
  // form_structures from BFCache for form submit.
  if (navigation_handle->IsServedFromBackForwardCache())
    return;

  submitted_forms_.clear();
  autofill_manager_->Reset();
}

void ContentAutofillDriver::SetBrowserAutofillManager(
    std::unique_ptr<BrowserAutofillManager> manager) {
  autofill_manager_ = std::move(manager);
  browser_autofill_manager_ =
      static_cast<BrowserAutofillManager*>(autofill_manager_.get());
}

ContentAutofillDriver::ContentAutofillDriver()
    : render_frame_host_(nullptr),
      browser_autofill_manager_(nullptr),
      key_press_handler_manager_(this),
      log_manager_(nullptr) {}

const mojo::AssociatedRemote<mojom::AutofillAgent>&
ContentAutofillDriver::GetAutofillAgent() {
  // Here is a lazy binding, and will not reconnect after connection error.
  if (!autofill_agent_) {
    render_frame_host_->GetRemoteAssociatedInterfaces()->GetInterface(
        &autofill_agent_);
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

void ContentAutofillDriver::SetFrameAndFormMetaData(
    FormFieldData& field) const {
  field.host_frame =
      LocalFrameToken(render_frame_host_->GetFrameToken().value());
}

void ContentAutofillDriver::SetFrameAndFormMetaData(FormData& form) const {
  form.host_frame =
      LocalFrameToken(render_frame_host_->GetFrameToken().value());

  form.url = StripAuthAndParams(render_frame_host_->GetLastCommittedURL());
  form.full_url = render_frame_host_->GetLastCommittedURL();

  if (auto* main_rfh = render_frame_host_->GetMainFrame())
    form.main_frame_origin = main_rfh->GetLastCommittedOrigin();
  else
    form.main_frame_origin = url::Origin();

  for (FormFieldData& field : form.fields)
    SetFrameAndFormMetaData(field);
}

FormFieldData ContentAutofillDriver::GetFieldWithFrameAndFormMetaData(
    FormFieldData field) const {
  SetFrameAndFormMetaData(field);
  return field;
}

FormData ContentAutofillDriver::GetFormWithFrameAndFormMetaData(
    FormData form) const {
  SetFrameAndFormMetaData(form);
  return form;
}

bool ContentAutofillDriver::DocumentUsedWebOTP() const {
  return render_frame_host_->DocumentUsedWebOTP();
}

void ContentAutofillDriver::MaybeReportAutofillWebOTPMetrics() {
  // In tests, the browser_autofill_manager_ may be unset or destroyed before
  // |this|.
  if (!browser_autofill_manager_)
    return;
  // It's possible that a frame without any form uses WebOTP. e.g. a server may
  // send the verification code to a phone number that was collected beforehand
  // and uses the WebOTP API for authentication purpose without user manually
  // entering the code.
  if (!browser_autofill_manager_->has_parsed_forms() && !DocumentUsedWebOTP())
    return;

  ReportAutofillWebOTPMetrics(DocumentUsedWebOTP());
}

void ContentAutofillDriver::ReportAutofillWebOTPMetrics(
    bool document_used_webotp) {
  if (browser_autofill_manager_->has_observed_phone_number_field())
    phone_collection_metric_state_ |= phone_collection_metric::kPhoneCollected;
  if (browser_autofill_manager_->has_observed_one_time_code_field())
    phone_collection_metric_state_ |= phone_collection_metric::kOTCUsed;
  if (document_used_webotp)
    phone_collection_metric_state_ |= phone_collection_metric::kWebOTPUsed;

  ukm::UkmRecorder* recorder =
      browser_autofill_manager_->client()->GetUkmRecorder();
  ukm::SourceId source_id =
      browser_autofill_manager_->client()->GetUkmSourceId();
  AutofillMetrics::LogWebOTPPhoneCollectionMetricStateUkm(
      recorder, source_id, phone_collection_metric_state_);

  UMA_HISTOGRAM_ENUMERATION(
      "Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
      static_cast<PhoneCollectionMetricState>(phone_collection_metric_state_));
}

void ContentAutofillDriver::ShowOfferNotificationIfApplicable(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  // TODO(crbug.com/1093057): Android webview does not have
  // |browser_autofill_manager_|, so flow is not enabled in Android Webview.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableOfferNotification) ||
      !browser_autofill_manager_) {
    return;
  }

  AutofillOfferManager* offer_manager =
      browser_autofill_manager_->offer_manager();
  // This happens in the Incognito mode.
  if (!offer_manager)
    return;

  GURL url = browser_autofill_manager_->client()->GetLastCommittedURL();
  if (!offer_manager->IsUrlEligible(url))
    return;

  // Try to show offer notification when the last committed URL has the domain
  // that an offer is applicable for.
  // TODO(crbug.com/1203811): GetOfferForUrl needs to know whether to give
  //   precedence to card-linked offers or promo code offers. Eventually, promo
  //   code offers should take precedence if a bubble is shown. Currently, if a
  //   url has both types of offers and the promo code offer is selected, no
  //   bubble will end up being shown (due to not yet being implemented).
  AutofillOfferData* offer = offer_manager->GetOfferForUrl(url);

  if (!offer || offer->merchant_domain.empty() ||
      (offer->IsCardLinkedOffer() && offer->eligible_instrument_id.empty()) ||
      (offer->IsPromoCodeOffer() && offer->promo_code.empty())) {
    return;
  }

  browser_autofill_manager_->client()->ShowOfferNotificationIfApplicable(offer);
}

}  // namespace autofill
