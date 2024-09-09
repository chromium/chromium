// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_password_manager_driver.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/content/browser/bad_message.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/content/browser/form_meta_data.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/password_manager/core/browser/password_suggestion_generator.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

using autofill::mojom::FocusedFieldType;

namespace password_manager {

namespace {

gfx::RectF TransformToRootCoordinates(
    content::RenderFrameHost* render_frame_host,
    const gfx::RectF& bounds_in_frame_coordinates) {
  content::RenderWidgetHostView* rwhv = render_frame_host->GetView();
  if (!rwhv)
    return bounds_in_frame_coordinates;
  return gfx::RectF(rwhv->TransformPointToRootCoordSpaceF(
                        bounds_in_frame_coordinates.origin()),
                    bounds_in_frame_coordinates.size());
}

void LogSiteIsolationMetricsForSubmittedForm(
    content::RenderFrameHost* render_frame_host) {
  UMA_HISTOGRAM_BOOLEAN(
      "SiteIsolation.IsPasswordFormSubmittedInDedicatedProcess",
      render_frame_host->GetSiteInstance()->RequiresDedicatedProcess());
}

bool HasValidURL(content::RenderFrameHost* render_frame_host) {
  GURL url = GetURLFromRenderFrameHost(render_frame_host);

  // URL might be invalid when GetLastCommittedOrigin is opaque.
  if (!url.is_valid())
    return false;

  return password_manager::bad_message::CheckForIllegalURL(
      render_frame_host, url,
      password_manager::BadMessageReason::CPMD_BAD_ORIGIN_FORM_SUBMITTED);
}

bool IsRenderFrameHostSupported(content::RenderFrameHost* rfh) {
  // Explanation of current PasswordManagerDriver limitations:
  // * Currently, PasswordManagerDriver binding has RenderFrameHost lifetime,
  //   not document lifetime. This can lead to premature binding in rare race
  //   conditions (see https://crbug.com/329989911).
  // * Due to this, we can't reliably determine if the document will be
  //   credentialless at this stage. Returning 'false' speculatively would be
  //   destructive.
  // * Workaround: Temporarily return 'true'; the function will be re-evaluated
  //   on commit via `DidNavigate`.
  //
  // TODO(https://crbug.com/40615943): After RenderDocument is enabled, consider
  // simplifying by binding PasswordManagerDriver via `PopulateFrameBinders`
  // instead of `RegisterAssociatedInterfaceBindersForRenderFrameHost`.
  if (rfh->GetLifecycleState() ==
      content::RenderFrameHost::LifecycleState::kPendingCommit) {
    return true;
  }

  if (rfh->GetLifecycleState() ==
      content::RenderFrameHost::LifecycleState::kPrerendering) {
    return false;
  }

  // [spec] https://wicg.github.io/anonymous-iframe/#spec-autofill
  // > Browsers that implement autofill or password manager functionalities
  //   should make them unavailable in credentialless iframes.
  if (rfh->IsCredentialless()) {
    return false;
  }

  return true;
}

}  // namespace

ContentPasswordManagerDriver::ContentPasswordManagerDriver(
    content::RenderFrameHost* render_frame_host,
    PasswordManagerClient* client)
    : render_frame_host_(render_frame_host),
      client_(client),
      password_generation_helper_(client, this),
      password_autofill_manager_(
          this,
          autofill::ContentAutofillClient::FromWebContents(
              content::WebContents::FromRenderFrameHost(render_frame_host)),
          client) {
  static unsigned next_free_id = 0;
  id_ = next_free_id++;

  render_frame_host_->GetRemoteAssociatedInterfaces()->GetInterface(
      &password_autofill_agent_);

  // For some frames `this` may be instantiated before log manager creation, so
  // here we can not send logging state to renderer process for them. For such
  // cases, after the log manager got ready later,
  // ContentPasswordManagerDriverFactory::RequestSendLoggingAvailability() will
  // call ContentPasswordManagerDriver::SendLoggingAvailability() on `this` to
  // do it actually.
  if (autofill::LogManager* log_manager = client_->GetLogManager()) {
    // RenderFrameHost might be a speculative one: it hasn't committed its
    // document. We don't know if its is safe to use the whole
    // PasswordAutofillAgent interface. For this reason, we use directly
    // `password_autofill_agent_`, as opposed to `GetPasswordAutofillAgent()`.
    // We know the `SetLoggingState()` is safe to be called no matter the state
    // of the RenderFrameHost.
    password_autofill_agent_->SetLoggingState(log_manager->IsLoggingActive());
  }
}

ContentPasswordManagerDriver::~ContentPasswordManagerDriver() = default;

// static
ContentPasswordManagerDriver*
ContentPasswordManagerDriver::GetForRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  ContentPasswordManagerDriverFactory* factory =
      ContentPasswordManagerDriverFactory::FromWebContents(
          content::WebContents::FromRenderFrameHost(render_frame_host));
  return factory ? factory->GetDriverForFrame(
                       render_frame_host,
                       base::PassKey<ContentPasswordManagerDriver>())
                 : nullptr;
}

void ContentPasswordManagerDriver::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<autofill::mojom::PasswordManagerDriver>
        pending_receiver) {
  if (IsRenderFrameHostSupported(render_frame_host_)) {
    password_manager_receiver_.Bind(std::move(pending_receiver));
  }
}

void ContentPasswordManagerDriver::DidNavigate() {
  if (!IsRenderFrameHostSupported(render_frame_host_)) {
    password_manager_receiver_.reset();
  }
}

int ContentPasswordManagerDriver::GetId() const {
  return id_;
}

int ContentPasswordManagerDriver::GetFrameId() const {
  // Use the associated FrameTreeNode ID as the Frame ID.
  return render_frame_host_->GetFrameTreeNodeId().value();
}

void ContentPasswordManagerDriver::SetPasswordFillData(
    const autofill::PasswordFormFillData& form_data) {
  password_autofill_manager_.OnAddPasswordFillData(form_data);
  if (const auto& agent = GetPasswordAutofillAgent()) {
    agent->SetPasswordFillData(autofill::MaybeClearPasswordValues(form_data));
  }
}

void ContentPasswordManagerDriver::InformNoSavedCredentials(
    bool should_show_popup_without_passwords) {
  GetPasswordAutofillManager()->OnNoCredentialsFound();
  if (const auto& agent = GetPasswordAutofillAgent()) {
    agent->InformNoSavedCredentials(should_show_popup_without_passwords);
  }
}

void ContentPasswordManagerDriver::FormEligibleForGenerationFound(
    const autofill::PasswordFormGenerationData& form) {
  if (GetPasswordGenerationHelper()->IsGenerationEnabled(
          /*log_debug_data=*/true)) {
    GetPasswordGenerationAgent()->FoundFormEligibleForGeneration(form);
  }
}

void ContentPasswordManagerDriver::GeneratedPasswordAccepted(
    const std::u16string& password) {
  // Same check as in PasswordGenerationAgent::GeneratedPasswordAccepted. The
  // generated password can't be too short.
  CHECK_LE(4u, password.size());
  GetPasswordGenerationAgent()->GeneratedPasswordAccepted(password);
}

void ContentPasswordManagerDriver::GeneratedPasswordAccepted(
    const autofill::FormData& raw_form,
    autofill::FieldRendererId generation_element_id,
    const std::u16string& password) {
  // In case we can't obtain a valid URL or a frame isn't allowed to perform an
  // operation with generated URL, don't forward anything to password manager.
  // TODO(crbug.com/40191770): Test that PasswordManager doesn't receive url
  // and full_url from renderer.
  if (!HasValidURL(render_frame_host_))
    return;

  GetPasswordManager()->OnGeneratedPasswordAccepted(
      this, GetFormWithFrameAndFormMetaData(render_frame_host_, raw_form),
      generation_element_id, password);
}

void ContentPasswordManagerDriver::FocusNextFieldAfterPasswords() {
  GetPasswordGenerationAgent()->FocusNextFieldAfterPasswords();
}

void ContentPasswordManagerDriver::FillField(const std::u16string& value) {
  if (const auto& agent = GetPasswordAutofillAgent()) {
    LogFilledFieldType();
    agent->FillField(last_triggering_field_id_, value);
  }
}

void ContentPasswordManagerDriver::FillSuggestion(
    const std::u16string& username,
    const std::u16string& password) {
  LogFilledFieldType();
  GetPasswordAutofillAgent()->FillPasswordSuggestion(username, password);
}

void ContentPasswordManagerDriver::FillSuggestionById(
    autofill::FieldRendererId username_element_id,
    autofill::FieldRendererId password_element_id,
    const std::u16string& username,
    const std::u16string& password) {
  LogFilledFieldType();
  GetPasswordAutofillAgent()->FillPasswordSuggestionById(
      username_element_id, password_element_id, username, password);
}

void ContentPasswordManagerDriver::FillIntoFocusedField(
    bool is_password,
    const std::u16string& credential) {
  if (const auto& agent = GetPasswordAutofillAgent()) {
    LogFilledFieldType();
    agent->FillIntoFocusedField(is_password, credential);
  }
}

#if BUILDFLAG(IS_ANDROID)
void ContentPasswordManagerDriver::KeyboardReplacingSurfaceClosed(
    ToShowVirtualKeyboard show_virtual_keyboard) {
  GetPasswordAutofillAgent()->KeyboardReplacingSurfaceClosed(
      show_virtual_keyboard.value());
}

void ContentPasswordManagerDriver::TriggerFormSubmission() {
  GetPasswordAutofillAgent()->TriggerFormSubmission();
}
#endif

void ContentPasswordManagerDriver::PreviewField(
    autofill::FieldRendererId field_id,
    const std::u16string& value) {
  if (const auto& agent = GetPasswordAutofillAgent()) {
    agent->PreviewField(field_id, value);
  }
}

void ContentPasswordManagerDriver::PreviewSuggestion(
    const std::u16string& username,
    const std::u16string& password) {
  GetAutofillAgent()->PreviewPasswordSuggestion(username, password);
}

void ContentPasswordManagerDriver::PreviewSuggestionById(
    autofill::FieldRendererId username_element_id,
    autofill::FieldRendererId password_element_id,
    const std::u16string& username,
    const std::u16string& password) {
  GetPasswordAutofillAgent()->PreviewPasswordSuggestionById(
      username_element_id, password_element_id, username, password);
}

void ContentPasswordManagerDriver::PreviewGenerationSuggestion(
    const std::u16string& password) {
  GetAutofillAgent()->PreviewPasswordGenerationSuggestion(password);
}

void ContentPasswordManagerDriver::ClearPreviewedForm() {
  GetAutofillAgent()->ClearPreviewedForm();
}

void ContentPasswordManagerDriver::SetSuggestionAvailability(
    autofill::FieldRendererId element_id,
    autofill::mojom::AutofillSuggestionAvailability suggestion_availability) {
  GetAutofillAgent()->SetSuggestionAvailability(element_id,
                                                suggestion_availability);
}

PasswordGenerationFrameHelper*
ContentPasswordManagerDriver::GetPasswordGenerationHelper() {
  return &password_generation_helper_;
}

PasswordManagerInterface* ContentPasswordManagerDriver::GetPasswordManager() {
  return client_->GetPasswordManager();
}

PasswordAutofillManager*
ContentPasswordManagerDriver::GetPasswordAutofillManager() {
  return &password_autofill_manager_;
}

void ContentPasswordManagerDriver::SendLoggingAvailability() {
  if (const auto& agent = GetPasswordAutofillAgent()) {
    agent->SetLoggingState(client_->GetLogManager()->IsLoggingActive());
  }
}

bool ContentPasswordManagerDriver::IsInPrimaryMainFrame() const {
  return render_frame_host_->IsInPrimaryMainFrame();
}

bool ContentPasswordManagerDriver::CanShowAutofillUi() const {
  // Don't show AutofillUi for inactive RenderFrameHost.
  return render_frame_host_->IsActive();
}

const GURL& ContentPasswordManagerDriver::GetLastCommittedURL() const {
  return render_frame_host_->GetLastCommittedURL();
}

void ContentPasswordManagerDriver::AnnotateFieldsWithParsingResult(
    const autofill::ParsingResult& parsing_result) {
  if (const auto& agent = GetPasswordAutofillAgent()) {
    agent->AnnotateFieldsWithParsingResult(parsing_result);
  }
}

base::WeakPtr<password_manager::PasswordManagerDriver>
ContentPasswordManagerDriver::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ContentPasswordManagerDriver::GeneratePassword(
    autofill::mojom::PasswordGenerationAgent::TriggeredGeneratePasswordCallback
        callback) {
  GetPasswordGenerationAgent()->TriggeredGeneratePassword(std::move(callback));
}

bool ContentPasswordManagerDriver::IsPasswordFieldForPasswordManager(
    autofill::FieldRendererId field_renderer_id,
    const content::ContextMenuParams& params) {
  if (params.form_control_type ==
          blink::mojom::FormControlType::kInputPassword ||
      params.is_password_type_by_heuristics) {
    return true;
  }

  password_manager::PasswordGenerationFrameHelper*
      password_generation_frame_helper = GetPasswordGenerationHelper();
  if (!password_generation_frame_helper) {
    return false;
  }
  return password_generation_frame_helper->IsManualGenerationEnabledField(
      field_renderer_id);
}

void ContentPasswordManagerDriver::PasswordFormsParsed(
    const std::vector<autofill::FormData>& raw_forms) {
  if (!password_manager::bad_message::CheckFrameNotPrerendering(
          render_frame_host_))
    return;

  // In case we can't obtain a valid URL or a frame isn't allowed to perform an
  // operation with generated URL, don't forward anything to password manager.
  if (!HasValidURL(render_frame_host_))
    return;

  auto logger =
      std::make_unique<password_manager::BrowserSavePasswordProgressLogger>(
          client_->GetLogManager());
  std::vector<autofill::FormData> forms = raw_forms;
  for (auto& form : forms) {
    SetFrameAndFormMetaData(render_frame_host_, form);
    logger->LogFormData(password_manager::BrowserSavePasswordProgressLogger::
                            STRING_FORM_IS_PASSWORD,
                        form);
  }

  GetPasswordManager()->OnPasswordFormsParsed(this, forms);
}

void ContentPasswordManagerDriver::PasswordFormsRendered(
    const std::vector<autofill::FormData>& raw_forms) {
  if (!password_manager::bad_message::CheckFrameNotPrerendering(
          render_frame_host_))
    return;

  // In case we can't obtain a valid URL or a frame isn't allowed to perform an
  // operation with generated URL, don't forward anything to password manager.
  if (!HasValidURL(render_frame_host_))
    return;

  std::vector<autofill::FormData> forms = raw_forms;
  for (auto& form : forms)
    SetFrameAndFormMetaData(render_frame_host_, form);

  GetPasswordManager()->OnPasswordFormsRendered(this, forms);
}

void ContentPasswordManagerDriver::PasswordFormSubmitted(
    const autofill::FormData& raw_form) {
  if (!password_manager::bad_message::CheckFrameNotPrerendering(
          render_frame_host_))
    return;

  // In case we can't obtain a valid URL or a frame isn't allowed to perform an
  // operation with generated URL, don't forward anything to password manager.
  if (!HasValidURL(render_frame_host_))
    return;

  GetPasswordManager()->OnPasswordFormSubmitted(
      this, GetFormWithFrameAndFormMetaData(render_frame_host_, raw_form));

  LogSiteIsolationMetricsForSubmittedForm(render_frame_host_);
}

void ContentPasswordManagerDriver::InformAboutUserInput(
    const autofill::FormData& raw_form) {
  if (!password_manager::bad_message::CheckFrameNotPrerendering(
          render_frame_host_))
    return;

  // In case we can't obtain a valid URL or a frame isn't allowed to perform an
  // operation with generated URL, don't forward anything to password manager.
  // TODO(crbug.com/40191770): Test that PasswordManager doesn't receive url
  // and full_url from renderer.
  if (!HasValidURL(render_frame_host_))
    return;

  autofill::FormData form_data =
      GetFormWithFrameAndFormMetaData(render_frame_host_, raw_form);
  GetPasswordManager()->OnInformAboutUserInput(this, form_data);

  if (FormHasNonEmptyPasswordField(form_data) &&
      client_->IsIsolationForPasswordSitesEnabled()) {
    // This function signals that a password field has been filled (whether by
    // the user, JS, autofill, or some other means) or a password form has been
    // submitted. Use this as a heuristic to start site-isolating the form's
    // site. This is intended to be used primarily when full site isolation is
    // not used, such as on Android.
    content::SiteInstance::StartIsolatingSite(
        render_frame_host_->GetSiteInstance()->GetBrowserContext(),
        form_data.url(),
        content::ChildProcessSecurityPolicy::IsolatedOriginSource::
            USER_TRIGGERED);
  }
}

void ContentPasswordManagerDriver::DynamicFormSubmission(
    autofill::mojom::SubmissionIndicatorEvent submission_indication_event) {
  if (!password_manager::bad_message::CheckFrameNotPrerendering(
          render_frame_host_))
    return;
  GetPasswordManager()->OnDynamicFormSubmission(this,
                                                submission_indication_event);
  LogSiteIsolationMetricsForSubmittedForm(render_frame_host_);
}

void ContentPasswordManagerDriver::PasswordFormCleared(
    const autofill::FormData& raw_form) {
  if (!password_manager::bad_message::CheckFrameNotPrerendering(
          render_frame_host_))
    return;

  // In case we can't obtain a valid URL or a frame isn't allowed to perform an
  // operation with generated URL, don't forward anything to password manager.
  if (!HasValidURL(render_frame_host_))
    return;

  GetPasswordManager()->OnPasswordFormCleared(
      this, GetFormWithFrameAndFormMetaData(render_frame_host_, raw_form));
}

void ContentPasswordManagerDriver::RecordSavePasswordProgress(
    const std::string& log) {
  if (!password_manager::bad_message::CheckFrameNotPrerendering(
          render_frame_host_))
    return;
  // Skip messages from chrome:// URLs as they are just noise for
  // chrome://password-manager-internals based debugging.
  if (GetLastCommittedURL().SchemeIs(content::kChromeUIScheme))
    return;
  LOG_AF(client_->GetLogManager())
      << autofill::Tag{"div"}
      << autofill::Attrib{"class", "preserve-white-space"} << log
      << autofill::CTag{"div"};
}

void ContentPasswordManagerDriver::UserModifiedPasswordField() {
  if (!password_manager::bad_message::CheckFrameNotPrerendering(
          render_frame_host_))
    return;
  if (client_->GetMetricsRecorder())
    client_->GetMetricsRecorder()->RecordUserModifiedPasswordField();
  // A user has modified an input field, it wouldn't be a submission "after
  // Touch To Fill".
  client_->ResetSubmissionTrackingAfterTouchToFill();
}

void ContentPasswordManagerDriver::UserModifiedNonPasswordField(
    autofill::FieldRendererId renderer_id,
    const std::u16string& value,
    bool autocomplete_attribute_has_username,
    bool is_likely_otp) {
  if (!password_manager::bad_message::CheckFrameNotPrerendering(
          render_frame_host_))
    return;
  GetPasswordManager()->OnUserModifiedNonPasswordField(
      this, renderer_id, value, autocomplete_attribute_has_username,
      is_likely_otp);
  // A user has modified an input field, it wouldn't be a submission "after
  // Touch To Fill".
  client_->ResetSubmissionTrackingAfterTouchToFill();
}

void ContentPasswordManagerDriver::ShowPasswordSuggestions(
    const autofill::PasswordSuggestionRequest& request) {
  if (!password_manager::bad_message::CheckFrameNotPrerendering(
          render_frame_host_))
    return;

  if ((request.username_field_index > request.form_data.fields().size()) ||
      (request.password_field_index > request.form_data.fields().size())) {
    mojo::ReportBadMessage(
        "username_field_index or password_field_index cannot be greater than "
        "form.fields.size()!");
  }

  last_triggering_field_id_ = request.element_id;

  base::OnceClosure show_with_autofill_manager_cb = base::BindOnce(
      &PasswordAutofillManager::OnShowPasswordSuggestions,
      GetPasswordAutofillManager()->GetWeakPtr(), request.element_id,
      request.trigger_source, request.text_direction, request.typed_username,
      ShowWebAuthnCredentials(request.show_webauthn_credentials),
      TransformToRootCoordinates(render_frame_host_, request.bounds));
#if !BUILDFLAG(IS_ANDROID)
  std::move(show_with_autofill_manager_cb).Run();
#else
  if (!base::FeatureList::IsEnabled(
          features::kPasswordSuggestionBottomSheetV2)) {
    std::move(show_with_autofill_manager_cb).Run();
    return;
  }
  // TODO(crbug.com/40269373): Remove the parameter
  // autofill::mojom::SubmissionReadinessState::kNoInformation when the
  // feature is launched.
  client_->ShowKeyboardReplacingSurface(
      this,
      PasswordFillingParams(
          request.form_data, request.username_field_index,
          request.password_field_index, request.element_id,
          autofill::mojom::SubmissionReadinessState::kNoInformation),
      request.show_webauthn_credentials,
      base::BindOnce(
          [](base::OnceClosure cb, bool shown) {
            if (shown) {
              // UI shown by `client_`, all done.
              return;
            }
            // Otherwise, show with PasswordAutofillManager.
            std::move(cb).Run();
          },
          std::move(show_with_autofill_manager_cb)));
#endif  // !BUILDFLAG(IS_ANDROID)
}

#if BUILDFLAG(IS_ANDROID)
void ContentPasswordManagerDriver::ShowKeyboardReplacingSurface(
    autofill::mojom::SubmissionReadinessState submission_readiness,
    bool is_webauthn_form) {
  if (!password_manager::bad_message::CheckFrameNotPrerendering(
          render_frame_host_)) {
    return;
  }
  autofill::FormData form;
  // This is only called when `kPasswordSuggestionBottomSheetV2` feature flag is
  // disabled. In this scenario only `submission_readiness` field of the
  // `PasswordFillingParams` is used later, other fields are not needed. This
  // call will be removed after launching the `kPasswordSuggestionBottomSheetV2`
  // feature.
  client_->ShowKeyboardReplacingSurface(
      this,
      PasswordFillingParams(form, 0, 0, autofill::FieldRendererId(),
                            submission_readiness),
      is_webauthn_form, base::DoNothing());
}
#endif

void ContentPasswordManagerDriver::CheckSafeBrowsingReputation(
    const GURL& form_action,
    const GURL& frame_url) {
  if (!password_manager::bad_message::CheckFrameNotPrerendering(
          render_frame_host_))
    return;
#if defined(ON_FOCUS_PING_ENABLED)
  client_->CheckSafeBrowsingReputation(form_action, frame_url);
#endif
}

void ContentPasswordManagerDriver::FocusedInputChanged(
    autofill::FieldRendererId focused_field_id,
    FocusedFieldType focused_field_type) {
  if (!password_manager::bad_message::CheckFrameNotPrerendering(
          render_frame_host_))
    return;
  client_->FocusedInputChanged(this, focused_field_id, focused_field_type);
}

void ContentPasswordManagerDriver::LogFirstFillingResult(
    autofill::FormRendererId form_renderer_id,
    int32_t result) {
  if (!password_manager::bad_message::CheckFrameNotPrerendering(
          render_frame_host_))
    return;
  GetPasswordManager()->LogFirstFillingResult(this, form_renderer_id, result);
}

void ContentPasswordManagerDriver::LogFilledFieldType() {
  bool field_classified_as_target_filling_password =
      GetPasswordManager()->GetPasswordFormCache()->GetPasswordForm(
          this, last_triggering_field_id_);
  base::UmaHistogramBoolean("Autofill.FilledFieldType.Password",
                            field_classified_as_target_filling_password);
}

const mojo::AssociatedRemote<autofill::mojom::AutofillAgent>&
ContentPasswordManagerDriver::GetAutofillAgent() {
  autofill::ContentAutofillDriver* autofill_driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(
          render_frame_host_);
  DCHECK(autofill_driver);
  return autofill_driver->GetAutofillAgent();
}

const mojo::AssociatedRemote<autofill::mojom::PasswordAutofillAgent>&
ContentPasswordManagerDriver::GetPasswordAutofillAgent() {
  // Autofill is not expected to interact with a RenderFrameHost that hasn't yet
  // committed its document.
  CHECK_NE(render_frame_host_->GetLifecycleState(),
           content::RenderFrameHost::LifecycleState::kPendingCommit);

  return IsRenderFrameHostSupported(render_frame_host_)
             ? password_autofill_agent_
             : password_autofill_agent_unbound_;
}

const mojo::AssociatedRemote<autofill::mojom::PasswordGenerationAgent>&
ContentPasswordManagerDriver::GetPasswordGenerationAgent() {
  DCHECK(!password_gen_agent_ ||
         (content::RenderFrameHost::LifecycleState::kPrerendering !=
          render_frame_host_->GetLifecycleState()));
  if (!password_gen_agent_) {
    render_frame_host_->GetRemoteAssociatedInterfaces()->GetInterface(
        &password_gen_agent_);
  }

  return password_gen_agent_;
}

}  // namespace password_manager
