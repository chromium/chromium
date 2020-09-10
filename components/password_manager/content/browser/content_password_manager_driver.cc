// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_password_manager_driver.h"

#include <utility>

#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/content/browser/bad_message.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/cert_status_flags.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/base/page_transition_types.h"

using autofill::mojom::FocusedFieldType;

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

}  // namespace

namespace password_manager {

ContentPasswordManagerDriver::ContentPasswordManagerDriver(
    content::RenderFrameHost* render_frame_host,
    PasswordManagerClient* client,
    autofill::AutofillClient* autofill_client)
    : render_frame_host_(render_frame_host),
      client_(client),
      password_generation_helper_(client, this),
      password_autofill_manager_(this, autofill_client, client),
      is_main_frame_(render_frame_host->GetParent() == nullptr),
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
    // Do not call the virtual method SendLoggingAvailability from a constructor
    // here, inline its steps instead.
    GetPasswordAutofillAgent()->SetLoggingState(
        client_->GetLogManager()->IsLoggingActive());
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
  password_manager_receiver_.Bind(std::move(pending_receiver));
}

int ContentPasswordManagerDriver::GetId() const {
  return id_;
}

void ContentPasswordManagerDriver::FillPasswordForm(
    const autofill::PasswordFormFillData& form_data) {
  password_autofill_manager_.OnAddPasswordFillData(form_data);
  GetPasswordAutofillAgent()->FillPasswordForm(
      autofill::MaybeClearPasswordValues(form_data));
}

void ContentPasswordManagerDriver::InformNoSavedCredentials(
    bool should_show_popup_without_passwords) {
  GetPasswordAutofillManager()->OnNoCredentialsFound();
  GetPasswordAutofillAgent()->InformNoSavedCredentials(
      should_show_popup_without_passwords);
}

void ContentPasswordManagerDriver::FormEligibleForGenerationFound(
    const autofill::PasswordFormGenerationData& form) {
  if (GetPasswordGenerationHelper()->IsGenerationEnabled(
          /*log_debug_data=*/true)) {
    GetPasswordGenerationAgent()->FoundFormEligibleForGeneration(form);
  }
}

void ContentPasswordManagerDriver::GeneratedPasswordAccepted(
    const base::string16& password) {
  GetPasswordGenerationAgent()->GeneratedPasswordAccepted(password);
}

void ContentPasswordManagerDriver::GeneratedPasswordAccepted(
    const autofill::FormData& form_data,
    autofill::FieldRendererId generation_element_id,
    const base::string16& password) {
  GetPasswordManager()->OnGeneratedPasswordAccepted(
      this, form_data, generation_element_id, password);
}

void ContentPasswordManagerDriver::TouchToFillClosed(
    ShowVirtualKeyboard show_virtual_keyboard) {
  GetPasswordAutofillAgent()->TouchToFillClosed(show_virtual_keyboard.value());
}

void ContentPasswordManagerDriver::FillSuggestion(
    const base::string16& username,
    const base::string16& password) {
  GetAutofillAgent()->FillPasswordSuggestion(username, password);
}

void ContentPasswordManagerDriver::FillIntoFocusedField(
    bool is_password,
    const base::string16& credential) {
  GetPasswordAutofillAgent()->FillIntoFocusedField(is_password, credential);
}

void ContentPasswordManagerDriver::PreviewSuggestion(
    const base::string16& username,
    const base::string16& password) {
  GetAutofillAgent()->PreviewPasswordSuggestion(username, password);
}

void ContentPasswordManagerDriver::ClearPreviewedForm() {
  GetAutofillAgent()->ClearPreviewedForm();
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
  GetPasswordAutofillAgent()->SetLoggingState(
      client_->GetLogManager()->IsLoggingActive());
}

autofill::AutofillDriver* ContentPasswordManagerDriver::GetAutofillDriver() {
  return autofill::ContentAutofillDriver::GetForRenderFrameHost(
      render_frame_host_);
}

bool ContentPasswordManagerDriver::IsMainFrame() const {
  return is_main_frame_;
}

bool ContentPasswordManagerDriver::CanShowAutofillUi() const {
  // Don't show AutofillUi for non-current RenderFrameHost.
  return render_frame_host_->IsCurrent();
}

const GURL& ContentPasswordManagerDriver::GetLastCommittedURL() const {
  return render_frame_host_->GetLastCommittedURL();
}

void ContentPasswordManagerDriver::AnnotateFieldsWithParsingResult(
    const autofill::ParsingResult& parsing_result) {
  GetPasswordAutofillAgent()->AnnotateFieldsWithParsingResult(parsing_result);
}

void ContentPasswordManagerDriver::GeneratePassword(
    autofill::mojom::PasswordGenerationAgent::
        UserTriggeredGeneratePasswordCallback callback) {
  GetPasswordGenerationAgent()->UserTriggeredGeneratePassword(
      std::move(callback));
}

void ContentPasswordManagerDriver::PasswordFormsParsed(
    const std::vector<autofill::FormData>& forms_data) {
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicy(
          render_frame_host_, forms_data,
          BadMessageReason::CPMD_BAD_ORIGIN_FORMS_PARSED))
    return;
  GetPasswordManager()->OnPasswordFormsParsed(this, forms_data);
}

void ContentPasswordManagerDriver::PasswordFormsRendered(
    const std::vector<autofill::FormData>& visible_forms_data,
    bool did_stop_loading) {
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicy(
          render_frame_host_, visible_forms_data,
          BadMessageReason::CPMD_BAD_ORIGIN_FORMS_RENDERED))
    return;
  GetPasswordManager()->OnPasswordFormsRendered(this, visible_forms_data,
                                                did_stop_loading);
}

void ContentPasswordManagerDriver::PasswordFormSubmitted(
    const autofill::FormData& form_data) {
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicyForURL(
          render_frame_host_, form_data.url,
          BadMessageReason::CPMD_BAD_ORIGIN_FORM_SUBMITTED))
    return;
  GetPasswordManager()->OnPasswordFormSubmitted(this, form_data);

  LogSiteIsolationMetricsForSubmittedForm(render_frame_host_);
}

void ContentPasswordManagerDriver::InformAboutUserInput(
    const autofill::FormData& form_data) {
  if (!password_manager::bad_message::CheckChildProcessSecurityPolicyForURL(
          render_frame_host_, form_data.url,
          BadMessageReason::CPMD_BAD_ORIGIN_UPON_USER_INPUT_CHANGE))
    return;
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
        form_data.url);
  }
}

void ContentPasswordManagerDriver::SameDocumentNavigation(
    autofill::mojom::SubmissionIndicatorEvent submission_indication_event) {
  GetPasswordManager()->OnPasswordFormSubmittedNoChecks(
      this, submission_indication_event);
  LogSiteIsolationMetricsForSubmittedForm(render_frame_host_);
}

void ContentPasswordManagerDriver::RecordSavePasswordProgress(
    const std::string& log) {
  client_->GetLogManager()->LogTextMessage(log);
}

void ContentPasswordManagerDriver::UserModifiedPasswordField() {
  if (client_->GetMetricsRecorder())
    client_->GetMetricsRecorder()->RecordUserModifiedPasswordField();
}

void ContentPasswordManagerDriver::UserModifiedNonPasswordField(
    autofill::FieldRendererId renderer_id,
    const base::string16& value) {
  GetPasswordManager()->OnUserModifiedNonPasswordField(this, renderer_id,
                                                       value);
}

void ContentPasswordManagerDriver::ShowPasswordSuggestions(
    base::i18n::TextDirection text_direction,
    const base::string16& typed_username,
    int options,
    const gfx::RectF& bounds) {
  GetPasswordAutofillManager()->OnShowPasswordSuggestions(
      text_direction, typed_username, options,
      TransformToRootCoordinates(render_frame_host_, bounds));
}

void ContentPasswordManagerDriver::ShowTouchToFill() {
  client_->ShowTouchToFill(this);
}

void ContentPasswordManagerDriver::CheckSafeBrowsingReputation(
    const GURL& form_action,
    const GURL& frame_url) {
  // Despite the name, this method is only called on password fields.
  // (See PasswordAutofillAgent::MaybeCheckSafeBrowsingReputation())
  if (client_->GetMetricsRecorder()) {
    client_->GetMetricsRecorder()->RecordUserFocusedPasswordField();
  }
#if defined(ON_FOCUS_PING_ENABLED)
  client_->CheckSafeBrowsingReputation(form_action, frame_url);
#endif
}

void ContentPasswordManagerDriver::FocusedInputChanged(
    FocusedFieldType focused_field_type) {
  client_->FocusedInputChanged(this, focused_field_type);
}

void ContentPasswordManagerDriver::LogFirstFillingResult(
    autofill::FormRendererId form_renderer_id,
    int32_t result) {
  GetPasswordManager()->LogFirstFillingResult(this, form_renderer_id, result);
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
  if (!password_gen_agent_) {
    render_frame_host_->GetRemoteAssociatedInterfaces()->GetInterface(
        &password_gen_agent_);
  }

  return password_gen_agent_;
}

}  // namespace password_manager
