// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_stream_device_permissions.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_ask_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_content_scrim_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_policy_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_previously_denied_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_previously_granted_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_show_system_prompt_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_system_settings_view.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/embedded_permission_prompt_flow_model.h"
#include "components/permissions/permission_uma_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/color/color_id.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "chrome/browser/permissions/system/system_media_capture_permissions_mac.h"
#endif

using Variant = permissions::EmbeddedPermissionPromptFlowModel::Variant;

EmbeddedPermissionPrompt::EmbeddedPermissionPrompt(
    Browser* browser,
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate)
    : PermissionPromptDesktop(browser, web_contents, delegate),
      delegate_(delegate) {
  prompt_model_ =
      std::make_unique<permissions::EmbeddedPermissionPromptFlowModel>(
          web_contents, delegate);
  CloseCurrentViewAndMaybeShowNext(/*first_prompt=*/true);
}

EmbeddedPermissionPrompt::~EmbeddedPermissionPrompt() {
  CloseViewAndScrim();
}

void EmbeddedPermissionPrompt::CloseCurrentViewAndMaybeShowNext(
    bool first_prompt) {
  if (!first_prompt) {
    CloseView();
  }

  prompt_model_->CalculateCurrentVariant();

  EmbeddedPermissionPromptBaseView* prompt_view = nullptr;

  switch (prompt_variant()) {
    case Variant::kAsk:
      prompt_view = new EmbeddedPermissionPromptAskView(
          browser(), weak_factory_.GetWeakPtr());
      break;
    case Variant::kPreviouslyGranted:
      if (first_prompt) {
        prompt_view = new EmbeddedPermissionPromptPreviouslyGrantedView(
            browser(), weak_factory_.GetWeakPtr());
      } else {
        delegate()->FinalizeCurrentRequests();
        return;
      }
      break;
    case Variant::kPreviouslyDenied:
      prompt_view = new EmbeddedPermissionPromptPreviouslyDeniedView(
          browser(), weak_factory_.GetWeakPtr());
      break;
    case Variant::kOsPrompt:
      prompt_view = new EmbeddedPermissionPromptShowSystemPromptView(
          browser(), weak_factory_.GetWeakPtr());
      prompt_model_->StartFirstDisplayTime();
      // This view has no buttons, so the OS level prompt should be triggered at
      // the same time as the |EmbeddedPermissionPromptShowSystemPromptView|.
      PromptForOsPermission();
      break;
    case Variant::kOsSystemSettings:
      prompt_view = new EmbeddedPermissionPromptSystemSettingsView(
          browser(), weak_factory_.GetWeakPtr());
      prompt_model_->StartFirstDisplayTime();
      break;
    case Variant::kAdministratorGranted:
      prompt_view = new EmbeddedPermissionPromptPolicyView(
          browser(), weak_factory_.GetWeakPtr(),
          /*is_permission_allowed=*/true);
      break;
    case Variant::kAdministratorDenied:
      prompt_view = new EmbeddedPermissionPromptPolicyView(
          browser(), weak_factory_.GetWeakPtr(),
          /*is_permission_allowed=*/false);
      break;
    case Variant::kUninitialized:
      NOTREACHED();
  }

  prompt_model_->RecordElementAnchoredBubbleVariantUMA(prompt_variant());

  if (prompt_view) {
    prompt_view_tracker_.SetView(prompt_view);
    if (!content_scrim_widget_) {
      scoped_ignore_input_events_ =
          web_contents()->IgnoreInputEvents(std::nullopt);
      // Creating the widget will display it. That's why we create it only if
      // the tab can show modal UI.
      tabs::TabInterface* tab =
          tabs::TabInterface::GetFromContents(web_contents());
      scoped_tab_modal_ui_ = tab->ShowModalUI();

      content_scrim_widget_ =
          EmbeddedPermissionPromptContentScrimView::CreateScrimWidget(
              weak_factory_.GetWeakPtr(),
              SkColorSetA(web_contents()->GetColorProvider().GetColor(
                              ui::kColorRefNeutral20),
                          0.8 * SK_AlphaOPAQUE),
              /*should_dismiss_on_click=*/true);
    }
    // If the tab/native view is closed, the `content_scrim_widget_` may be
    // nullptr. In this scenario, skip showing the prompt.
    if (!content_scrim_widget_) {
      return;
    }
    prompt_view->UpdateAnchor(content_scrim_widget_.get());
    prompt_view->Show();
  }

  if (prompt_view->GetWidget()) {
    prompt_view->GetWidget()->UpdateAccessibleNameForRootView();
  }
}

EmbeddedPermissionPrompt::TabSwitchingBehavior
EmbeddedPermissionPrompt::GetTabSwitchingBehavior() {
  return TabSwitchingBehavior::kDestroyPromptAndIgnoreRequest;
}

permissions::PermissionPromptDisposition
EmbeddedPermissionPrompt::GetPromptDisposition() const {
  return permissions::PermissionPromptDisposition::ELEMENT_ANCHORED_BUBBLE;
}

bool EmbeddedPermissionPrompt::ShouldFinalizeRequestAfterDecided() const {
  return false;
}

std::vector<permissions::ElementAnchoredBubbleVariant>
EmbeddedPermissionPrompt::GetPromptVariants() const {
  std::vector<permissions::ElementAnchoredBubbleVariant> variants;
  return prompt_model_->GetPromptVariants();
}

bool EmbeddedPermissionPrompt::IsAskPrompt() const {
  return (prompt_variant() == Variant::kAsk);
}

std::optional<permissions::feature_params::PermissionElementPromptPosition>
EmbeddedPermissionPrompt::GetPromptPosition() const {
  if (auto* prompt_view = static_cast<const EmbeddedPermissionPromptBaseView*>(
          prompt_view_tracker_.view())) {
    return prompt_view->GetPromptPosition();
  }
  return std::nullopt;
}

std::optional<gfx::Rect> EmbeddedPermissionPrompt::GetViewBoundsInScreen()
    const {
  if (prompt_view_tracker_.view()) {
    // This is a modal prompt, the view bounds will cover the whole content
    // view.
    return web_contents()->GetContainerBounds();
  }
  return std::nullopt;
}

void EmbeddedPermissionPrompt::Allow() {
  prompt_model_->PrecalculateVariantsForMetrics();
  prompt_model_->RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kGranted);
  prompt_model_->SetDelegateAction(
      permissions::EmbeddedPermissionPromptFlowModel::DelegateAction::kAllow);
  CloseCurrentViewAndMaybeShowNext(/*first_prompt=*/false);
}

void EmbeddedPermissionPrompt::AllowThisTime() {
  prompt_model_->PrecalculateVariantsForMetrics();
  prompt_model_->RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kGrantedOnce);
  prompt_model_->SetDelegateAction(
      permissions::EmbeddedPermissionPromptFlowModel::DelegateAction::
          kAllowThisTime);
  CloseCurrentViewAndMaybeShowNext(/*first_prompt=*/false);
}

void EmbeddedPermissionPrompt::Dismiss() {
  prompt_model_->PrecalculateVariantsForMetrics();
  permissions::PermissionUmaUtil::RecordElementAnchoredBubbleDismiss(
      delegate()->Requests(), permissions::DismissedReason::kDismissedXButton);
  prompt_model_->RecordOsMetrics(
      permissions::OsScreenAction::kDismissedXButton);
  prompt_model_->RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kDismissedXButton);

  prompt_model_->SetDelegateAction(
      permissions::EmbeddedPermissionPromptFlowModel::DelegateAction::kDismiss);
  FinalizePrompt();
}

void EmbeddedPermissionPrompt::Acknowledge() {
  prompt_model_->RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kOk);

  prompt_model_->SetDelegateAction(
      permissions::EmbeddedPermissionPromptFlowModel::DelegateAction::kDismiss);
  FinalizePrompt();
}

void EmbeddedPermissionPrompt::StopAllowing() {
  prompt_model_->PrecalculateVariantsForMetrics();
  prompt_model_->RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kDenied);

  prompt_model_->SetDelegateAction(
      permissions::EmbeddedPermissionPromptFlowModel::DelegateAction::kDeny);
  FinalizePrompt();
}

void EmbeddedPermissionPrompt::ShowSystemSettings() {
  const auto& requests = delegate()->Requests();
  CHECK_GT(requests.size(), 0U);

  prompt_model_->RecordOsMetrics(permissions::OsScreenAction::kSystemSettings);
  prompt_model_->RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kSystemSettings);
  for (const auto& request : requests) {
    if (system_permission_settings::IsDenied(
            request->GetContentSettingsType())) {
      system_permission_settings::OpenSystemSettings(
          delegate()->GetAssociatedWebContents(),
          request->GetContentSettingsType());
      return;
    }
  }

  // Since we don't observe system level permission status changes, there is a
  // possibility that all permission settings have been granted at this point.
  SystemPermissionsNoLongerDenied();
}

void EmbeddedPermissionPrompt::SystemPermissionsNoLongerDenied() {
  CHECK(prompt_model_->prompt_variant() ==
        permissions::EmbeddedPermissionPromptFlowModel::Variant::
            kOsSystemSettings);
  prompt_model_->PrecalculateVariantsForMetrics();
  CloseCurrentViewAndMaybeShowNext(/*first_prompt=*/false);
}

base::WeakPtr<permissions::PermissionPrompt::Delegate>
EmbeddedPermissionPrompt::GetPermissionPromptDelegate() const {
  return delegate_->GetWeakPtr();
}

const std::vector<base::WeakPtr<permissions::PermissionRequest>>&
EmbeddedPermissionPrompt::Requests() const {
  return prompt_model_->requests();
}

void EmbeddedPermissionPrompt::DismissScrim() {
  permissions::PermissionUmaUtil::RecordElementAnchoredBubbleDismiss(
      delegate()->Requests(), permissions::DismissedReason::kDismissedScrim);
  prompt_model_->RecordOsMetrics(permissions::OsScreenAction::kDismissedScrim);
  prompt_model_->RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kDismissedScrim);

  prompt_model_->PrecalculateVariantsForMetrics();
  prompt_model_->SetDelegateAction(
      permissions::EmbeddedPermissionPromptFlowModel::DelegateAction::kDismiss);
  FinalizePrompt();
}

void EmbeddedPermissionPrompt::PromptForOsPermission() {
  const auto& prompt_types = prompt_model_->prompt_types();
  // We currently support <=2 grouped permissions.
  CHECK_LE(prompt_types.size(), 2U);

  std::vector<ContentSettingsType> types(prompt_types.begin(),
                                         prompt_types.end());

  for (unsigned int idx = 0; idx < types.size(); idx++) {
    system_permission_settings::Request(
        types[idx],
        base::BindOnce(
            &EmbeddedPermissionPrompt::OnRequestSystemPermissionResponse,
            weak_factory_.GetWeakPtr(), types[idx],
            // Pass the other type for grouped permission case.
            (types.size() == 2U ? types[1U - idx]
                                : ContentSettingsType::DEFAULT)));
  }
}

void EmbeddedPermissionPrompt::OnRequestSystemPermissionResponse(
    const ContentSettingsType request_type,
    const ContentSettingsType other_request_type) {
  bool permission_determined =
      !system_permission_settings::CanPrompt(request_type);

  // `other_permission_determined` is left with true in non-grouped scenario,
  // which would make the final logic fully rely on `permission_determined`.
  auto other_permission_determined = true;
  if (other_request_type != ContentSettingsType::DEFAULT) {
    other_permission_determined =
        !system_permission_settings::CanPrompt(other_request_type);
  }

  if (permission_determined) {
#if BUILDFLAG(IS_MAC)
    system_permission_settings::SystemPermission permission;

    if (request_type == ContentSettingsType::MEDIASTREAM_MIC) {
      permission =
          system_permission_settings::CheckSystemAudioCapturePermission();
    }
    if (request_type == ContentSettingsType::MEDIASTREAM_CAMERA) {
      permission =
          system_permission_settings::CheckSystemVideoCapturePermission();
    }

    switch (permission) {
      case system_permission_settings::SystemPermission::kRestricted:
        break;
      case system_permission_settings::SystemPermission::kDenied:
        prompt_model_->RecordOsMetrics(
            permissions::OsScreenAction::kOsPromptDenied);
        break;
      case system_permission_settings::SystemPermission::kAllowed:
        prompt_model_->RecordOsMetrics(
            permissions::OsScreenAction::kOsPromptAllowed);
        break;
      case system_permission_settings::SystemPermission::kNotDetermined:
        NOTREACHED();
    }
#endif  // BUILDFLAG(IS_MAC)

    // Do not finalize request until all the necessary system permissions are
    // granted.
    if (other_permission_determined) {
      FinalizePrompt();
    }
  } else {
    NOTREACHED();
  }
}

void EmbeddedPermissionPrompt::CloseView() {
  if (auto* prompt_view = static_cast<EmbeddedPermissionPromptBaseView*>(
          prompt_view_tracker_.view())) {
    prompt_view->PrepareToClose();
    prompt_view->GetWidget()->Close();
    prompt_view_tracker_.SetView(nullptr);
    prompt_model_->Clear();
  }
}

void EmbeddedPermissionPrompt::CloseViewAndScrim() {
  CloseView();

  if (content_scrim_widget_) {
    content_scrim_widget_->Close();
    content_scrim_widget_ = nullptr;
    scoped_ignore_input_events_.reset();
  }

  scoped_tab_modal_ui_.reset();
}

void EmbeddedPermissionPrompt::FinalizePrompt() {
  CloseViewAndScrim();

  // If by this point we've not sent an action to the delegate, send a dismiss
  // action.
  if (!prompt_model_->HasDelegateActionSet()) {
    prompt_model_->SetDelegateAction(
        permissions::EmbeddedPermissionPromptFlowModel::DelegateAction::
            kDismiss);
  }
  delegate_->FinalizeCurrentRequests();
}
