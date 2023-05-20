// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_password_manager_driver.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/content/browser/bad_message.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/content/browser/form_meta_data.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/cert/cert_status_flags.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/base/page_transition_types.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#endif  // BUILDFLAG(IS_ANDROID)

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

  return password_manager::bad_message::CheckChildProcessSecurityPolicyForURL(
      render_frame_host, url,
      password_manager::BadMessageReason::CPMD_BAD_ORIGIN_FORM_SUBMITTED);
}

}  // namespace

ContentPasswordManagerDriver::ContentPasswordManagerDriver(
    content::RenderFrameHost* render_frame_host,
    PasswordManagerClient* client,
    autofill::AutofillClient* autofill_client)
    : render_frame_host_(render_frame_host),
      client_(client),
      password_generation_helper_(client, this),
      password_autofill_manager_(this, autofill_client, client),
      password_manager_receiver_(this) {
  static unsigned next_free_id = 0;
  id_ = next_free_id++;
  // For some frames |this| may be instantiated before log manager creation, so
  // here we can not send logging state to renderer process for them. For such
  // cases, after the log manager got ready later,
  // ContentPasswordManagerDriverFactory::RequestSendLoggingAvailability() will
  // call ContentPasswordManagerDriver::SendLoggingAvailability() on |this| to
  // do it actually.
  if (client_->GetLogManager()) {
    if (const auto& agent = GetPasswordAutofillAgent()) {
      // Do not call the virtual method SendLoggingAvailability from a
      // constructor here, inline its steps instead.
      agent->SetLoggingState(client_->GetLogManager()->IsLoggingActive());
    }
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
  return factory ? factory->GetDriverForFrame(render_frame_host) : nullptr;
}

void ContentPasswordManagerDriver::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<autofill::mojom::PasswordManagerDriver>
        pending_receiver) {
  if (render_frame_host_->IsCredentialless())
    return;
  password_manager_receiver_.Bind(std::move(pending_receiver));
}

void ContentPasswordManagerDriver::UnbindReceiver() {
  password_manager_receiver_.reset();
}

int ContentPasswordManagerDriver::GetId() const {
  return id_;
}

int ContentPasswordManagerDriver::GetFrameId() const {
  // Use the associated FrameTreeNode ID as the Frame ID.
  return render_frame_host_->GetFrameTreeNodeId();
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
  GetPasswordGenerationAgent()->GeneratedPasswordAccepted(password);
}

void ContentPasswordManagerDriver::GeneratedPasswordAccepted(
    const autofill::FormData& raw_form,
    autofill::FieldRendererId generation_element_id,
    const std::u16string& password) {
  // In case we can't obtain a valid URL or a frame isn't allowed to perform an
  // operation with generated URL, don't forward anything to password manager.
  // TODO(crbug.com/1233990): Test that PasswordManager doesn't receive url
  // and full_url from renderer.
  if (!HasValidURL(render_frame_host_))
    return;

  GetPasswordManager()->OnGeneratedPasswordAccepted(
      this, GetFormWithFrameAndFormMetaData(render_frame_host_, raw_form),
      generation_element_id, password);
}

void ContentPasswordManagerDriver::FillSuggestion(
    const std::u16string& username,
    const std::u16string& password) {
  GetAutofillAgent()->FillPasswordSuggestion(username, password);
}

void ContentPasswordManagerDriver::FillIntoFocusedField(
    bool is_password,
    const std::u16string& credential) {
  if (const auto& agent = GetPasswordAutofillAgent()) {
    agent->FillIntoFocusedField(is_password, credential);
  }
}

#if BUILDFLAG(IS_ANDROID)
void ContentPasswordManagerDriver::KeyboardReplacingSurfaceClosed(
    ShowVirtualKeyboard show_virtual_keyboard) {
  GetPasswordAutofillAgent()->KeyboardReplacingSurfaceClosed(
      show_virtual_keyboard.value());
}

void ContentPasswordManagerDriver::TriggerFormSubmission() {
  GetPasswordAutofillAgent()->TriggerFormSubmission();
}
#endif

void ContentPasswordManagerDriver::PreviewSuggestion(
    const std::u16string& username,
    const std::u16string& password) {
  GetAutofillAgent()->PreviewPasswordSuggestion(username, password);
}

void ContentPasswordManagerDriver::PreviewGenerationSuggestion(
    const std::u16string& password) {
  GetAutofillAgent()->PreviewPasswordGenerationSuggestion(password);
}

void ContentPasswordManagerDriver::ClearPreviewedForm() {
  GetAutofillAgent()->ClearPreviewedForm();
}

void ContentPasswordManagerDriver::SetSuggestionAvailability(
    autofill::FieldRendererId generation_element_id,
    const autofill::mojom::AutofillState state) {
  GetAutofillAgent()->SetSuggestionAvailability(generation_element_id, state);
}

PasswordGenerationFrameHelper*
ContentPasswordManagerDriver::GetPasswordGenerationHelper() {
  return &password_generation_helper_;
}

PasswordManager* ContentPasswordManagerDriver::GetPasswordManager() {
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

::ui::AXTreeID ContentPasswordManagerDriver::GetAxTreeId() const {
  return render_frame_host_->GetAXTreeID();
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

void ContentPasswordManagerDriver::GeneratePassword(
    autofill::mojom::PasswordGenerationAgent::TriggeredGeneratePasswordCallback
        callback) {
  GetPasswordGenerationAgent()->TriggeredGeneratePassword(std::move(callback));
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

  std::vector<autofill::FormData> forms = raw_forms;
  for (auto& form : forms)
    SetFrameAndFormMetaData(render_frame_host_, form);

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
  // TODO(crbug.com/1233990): Test that PasswordManager doesn't receive url
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
        form_data.url,
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
    const std::u16string& field_name,
    const std::u16string& value,
    bool autocomplete_attribute_has_username) {
  if (!password_manager::bad_message::CheckFrameNotPrerendering(
          render_frame_host_))
    return;
  GetPasswordManager()->OnUserModifiedNonPasswordField(
      this, renderer_id, field_name, value,
      autocomplete_attribute_has_username);
  // A user has modified an input field, it wouldn't be a submission "after
  // Touch To Fill".
  client_->ResetSubmissionTrackingAfterTouchToFill();
}

void ContentPasswordManagerDriver::ShowPasswordSuggestions(
    base::i18n::TextDirection text_direction,
    const std::u16string& typed_username,
    int options,
    const gfx::RectF& bounds) {
  if (!password_manager::bad_message::CheckFrameNotPrerendering(
          render_frame_host_))
    return;
  GetPasswordAutofillManager()->OnShowPasswordSuggestions(
      text_direction, typed_username, options,
      TransformToRootCoordinates(render_frame_host_, bounds));
}

#if BUILDFLAG(IS_ANDROID)
void ContentPasswordManagerDriver::ShowKeyboardReplacingSurface(
    autofill::mojom::SubmissionReadinessState submission_readiness,
    bool is_webauthn_form) {
  if (!password_manager::bad_message::CheckFrameNotPrerendering(
          render_frame_host_)) {
    return;
  }
  if (is_webauthn_form && WebAuthnCredManDelegate::IsCredManEnabled()) {
    WebAuthnCredManDelegate* cred_man_delegate =
        WebAuthnCredManDelegate::GetRequestDelegate(
            content::WebContents::FromRenderFrameHost(render_frame_host_));
    // webauthn forms without passkeys should show TouchToFill bottom sheet.
    if (cred_man_delegate->HasResults()) {
      auto cred_man_request_completion_cb =
          base::BindRepeating(
              [](bool success) { return ShowVirtualKeyboard(!success); })
              .Then(base::BindRepeating(
                  &ContentPasswordManagerDriver::KeyboardReplacingSurfaceClosed,
                  weak_factory_.GetWeakPtr()));

      cred_man_delegate->SetRequestCompletionCallback(
          std::move(cred_man_request_completion_cb));
      cred_man_delegate->TriggerFullRequest();
      return;
    }
  }
  client_->ShowTouchToFill(this, submission_readiness);
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

void ContentPasswordManagerDriver::SetKeyPressHandler(
    const content::RenderWidgetHost::KeyPressEventCallback& handler) {
  UnsetKeyPressHandler();
  content::RenderWidgetHostView* view = render_frame_host_->GetView();
  if (!view)
    return;
  view->GetRenderWidgetHost()->AddKeyPressEventCallback(handler);
  key_press_handler_ = handler;
}

void ContentPasswordManagerDriver::UnsetKeyPressHandler() {
  if (key_press_handler_.is_null())
    return;
  content::RenderWidgetHostView* view = render_frame_host_->GetView();
  if (!view)
    return;
  view->GetRenderWidgetHost()->RemoveKeyPressEventCallback(key_press_handler_);
  key_press_handler_.Reset();
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
  if (render_frame_host_->IsCredentialless() ||
      render_frame_host_->GetLifecycleState() ==
          content::RenderFrameHost::LifecycleState::kPrerendering) {
    password_autofill_agent_.reset();
    return password_autofill_agent_;  // Unbound remote.
  }

  if (!password_autofill_agent_) {
    // Some test environments may have no remote interface support.
    if (render_frame_host_->GetRemoteAssociatedInterfaces()) {
      render_frame_host_->GetRemoteAssociatedInterfaces()->GetInterface(
          &password_autofill_agent_);
    }
  }

  return password_autofill_agent_;
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
