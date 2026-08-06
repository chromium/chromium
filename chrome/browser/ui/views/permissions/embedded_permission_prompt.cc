// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt.h"

#include <variant>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_ask_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_content_scrim_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_policy_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_previously_denied_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_previously_granted_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_show_system_prompt_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_system_settings_view.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/embedded_permission_prompt_flow_model.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/web_contents.h"
#include "ui/color/color_id.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "chrome/browser/permissions/system/system_media_capture_permissions_mac.h"
#endif

using Variant = permissions::EmbeddedPermissionPromptFlowModel::Variant;

EmbeddedPermissionPrompt::EmbeddedPermissionPrompt(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate)
    : PermissionPromptDesktop(web_contents, delegate), delegate_(delegate) {
  prompt_model_ = delegate_->GetEmbeddedPromptFlowModel();
  CHECK(prompt_model_);

  EmbeddedPermissionPromptBaseView* prompt_view = nullptr;

  switch (prompt_variant()) {
    case Variant::kAsk:
      prompt_view = new EmbeddedPermissionPromptAskView(
          web_contents, weak_factory_.GetWeakPtr());
      break;
    case Variant::kPreviouslyGranted:
      prompt_view = new EmbeddedPermissionPromptPreviouslyGrantedView(
          web_contents, weak_factory_.GetWeakPtr());
      break;
    case Variant::kPreviouslyDenied:
      prompt_view = new EmbeddedPermissionPromptPreviouslyDeniedView(
          web_contents, weak_factory_.GetWeakPtr());
      break;
    case Variant::kOsPrompt:
      prompt_view = new EmbeddedPermissionPromptShowSystemPromptView(
          web_contents, weak_factory_.GetWeakPtr());
      prompt_model_->StartFirstDisplayTime();
      PromptForOsPermission();
      break;
    case Variant::kOsSystemSettings:
      prompt_view = new EmbeddedPermissionPromptSystemSettingsView(
          web_contents, weak_factory_.GetWeakPtr());
      prompt_model_->StartFirstDisplayTime();
      break;
    case Variant::kAdministratorGranted:
      prompt_view = new EmbeddedPermissionPromptPolicyView(
          web_contents, weak_factory_.GetWeakPtr(),
          /*is_permission_allowed=*/true);
      break;
    case Variant::kAdministratorDenied:
      prompt_view = new EmbeddedPermissionPromptPolicyView(
          web_contents, weak_factory_.GetWeakPtr(),
          /*is_permission_allowed=*/false);
      break;
    case Variant::kUninitialized:
      NOTREACHED();
  }

  prompt_model_->RecordElementAnchoredBubbleVariantUMA(prompt_variant());

  if (prompt_view) {
    prompt_view_tracker_.SetView(prompt_view);
    permissions::EmbeddedPermissionPromptFlowModel::PromptContentScrim* scrim =
        prompt_model_->EnsureContentScrim();
    views::Widget* scrim_widget = nullptr;
    if (scrim) {
      scrim_widget = static_cast<EmbeddedPermissionPromptContentScrim*>(scrim)
                         ->GetWidget();
    }
    // If the tab/native view is closed, the `scrim_widget` may be
    // nullptr. In this scenario, skip showing the prompt.
    if (!scrim_widget) {
      return;
    }
    prompt_view->UpdateAnchor(scrim_widget);
    prompt_view->Show();
  }

  if (prompt_view->GetWidget()) {
    prompt_view->GetWidget()->UpdateAccessibleNameForRootView();
  }
}

EmbeddedPermissionPrompt::~EmbeddedPermissionPrompt() {
  CloseView();
}

EmbeddedPermissionPrompt::TabSwitchingBehavior
EmbeddedPermissionPrompt::GetTabSwitchingBehavior() {
  return TabSwitchingBehavior::kDestroyPromptAndIgnoreRequest;
}

permissions::PermissionPromptDisposition
EmbeddedPermissionPrompt::GetPromptDisposition() const {
  return permissions::PermissionPromptDisposition::ELEMENT_ANCHORED_BUBBLE;
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

  // GEOLOCATION_WITH_OPTIONS is currently not supported on desktop.
  //
  // TODO(crbug.com/430494523): Plumb through the selected PromptOptions once it
  // is.
  CHECK_NE(delegate()->Requests()[0]->GetContentSettingsType(),
           ContentSettingsType::GEOLOCATION_WITH_OPTIONS);

  delegate()->Accept(/*prompt_options=*/std::monostate());
}

void EmbeddedPermissionPrompt::AllowThisTime() {
  prompt_model_->PrecalculateVariantsForMetrics();
  prompt_model_->RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kGrantedOnce);

  // GEOLOCATION_WITH_OPTIONS is currently not supported on desktop.
  //
  // TODO(crbug.com/430494523): Plumb through the selected PromptOptions once it
  // is.
  CHECK_NE(delegate()->Requests()[0]->GetContentSettingsType(),
           ContentSettingsType::GEOLOCATION_WITH_OPTIONS);

  delegate()->AcceptThisTime(/*prompt_options=*/std::monostate());
}

void EmbeddedPermissionPrompt::Dismiss() {
  prompt_model_->PrecalculateVariantsForMetrics();
  permissions::PermissionUmaUtil::RecordElementAnchoredBubbleDismiss(
      delegate()->Requests(), permissions::DismissedReason::kDismissedXButton);
  prompt_model_->RecordOsMetrics(
      permissions::OsScreenAction::kDismissedXButton);
  prompt_model_->RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kDismissedXButton);

  // GEOLOCATION_WITH_OPTIONS is currently not supported on desktop.
  //
  // TODO(crbug.com/430494523): Plumb through the selected PromptOptions once it
  // is.
  CHECK_NE(delegate()->Requests()[0]->GetContentSettingsType(),
           ContentSettingsType::GEOLOCATION_WITH_OPTIONS);

  delegate()->Dismiss(/*prompt_options=*/std::monostate());
}

void EmbeddedPermissionPrompt::Acknowledge() {
  prompt_model_->RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kOk);

  // GEOLOCATION_WITH_OPTIONS is currently not supported on desktop.
  //
  // TODO(crbug.com/430494523): Plumb through the selected PromptOptions once it
  // is.
  CHECK_NE(delegate()->Requests()[0]->GetContentSettingsType(),
           ContentSettingsType::GEOLOCATION_WITH_OPTIONS);

  delegate()->Dismiss(/*prompt_options=*/std::monostate());
}

void EmbeddedPermissionPrompt::StopAllowing() {
  prompt_model_->PrecalculateVariantsForMetrics();
  prompt_model_->RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kDenied);

  // GEOLOCATION_WITH_OPTIONS is currently not supported on desktop.
  //
  // TODO(crbug.com/430494523): Plumb through the selected PromptOptions once it
  // is.
  CHECK_NE(delegate()->Requests()[0]->GetContentSettingsType(),
           ContentSettingsType::GEOLOCATION_WITH_OPTIONS);

  delegate()->Deny(/*prompt_options=*/std::monostate());
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
  delegate_->AdvanceOrFinalizeEmbeddedPromptFlow();
}

base::WeakPtr<permissions::PermissionPrompt::Delegate>
EmbeddedPermissionPrompt::GetPermissionPromptDelegate() const {
  return delegate_->GetWeakPtr();
}

void EmbeddedPermissionPrompt::PromptForOsPermission() {
  const auto& requests = delegate()->Requests();
  // We currently support <=2 grouped permissions.
  CHECK_LE(requests.size(), 2U);

  std::vector<ContentSettingsType> types;
  types.reserve(requests.size());
  for (const auto& request : requests) {
    types.push_back(request->GetContentSettingsType());
  }

  for (size_t idx = 0; idx < types.size(); idx++) {
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

  // Note, system permission determination is not guaranteed. We just exit and
  // take no action
  if (!permission_determined) {
    return;
  }

  // `other_permission_determined` is left with true in non-grouped scenario,
  // which would make the final logic fully rely on `permission_determined`.
  auto other_permission_determined = true;
  if (other_request_type != ContentSettingsType::DEFAULT) {
    other_permission_determined =
        !system_permission_settings::CanPrompt(other_request_type);
  }

#if BUILDFLAG(IS_MAC)
  if (request_type == ContentSettingsType::MEDIASTREAM_MIC ||
      request_type == ContentSettingsType::MEDIASTREAM_CAMERA) {
    system_permission_settings::SystemPermission permission =
        request_type == ContentSettingsType::MEDIASTREAM_MIC
            ? system_permission_settings::CheckSystemAudioCapturePermission()
            : system_permission_settings::CheckSystemVideoCapturePermission();

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
  }
#endif  // BUILDFLAG(IS_MAC)

    // Do not finalize request until all the necessary system permissions are
    // granted.
    if (other_permission_determined) {
      delegate_->AdvanceOrFinalizeEmbeddedPromptFlow();
    }
}

void EmbeddedPermissionPrompt::CloseView() {
  if (auto* prompt_view = static_cast<EmbeddedPermissionPromptBaseView*>(
          prompt_view_tracker_.view())) {
    prompt_view->PrepareToClose();
    prompt_view->GetWidget()->Close();
    prompt_view_tracker_.SetView(nullptr);
  }
}
