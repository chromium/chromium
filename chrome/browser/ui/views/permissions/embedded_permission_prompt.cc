// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_ask_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_content_scrim_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_policy_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_previously_denied_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_previously_granted_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_show_system_prompt_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_system_settings_view.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_uma_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/color/color_id.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#endif

namespace {

using content_settings::SettingSource;

// An upper bound on the maximum number of screens that we can record in
// metrics. Practically speaking the actual number should never be more than 3
// but a higher bound allows us to detect via metrics if this happens in the
// wild.
constexpr int SCREEN_COUNTER_MAXIMUM = 10;

bool CanGroupVariants(EmbeddedPermissionPrompt::Variant a,
                      EmbeddedPermissionPrompt::Variant b) {
  // Ask and PreviouslyDenied are a special case and can be grouped together.
  if ((a == EmbeddedPermissionPrompt::Variant::kPreviouslyDenied &&
       b == EmbeddedPermissionPrompt::Variant::kAsk) ||
      (a == EmbeddedPermissionPrompt::Variant::kAsk &&
       b == EmbeddedPermissionPrompt::Variant::kPreviouslyDenied)) {
    return true;
  }

  return (a == b);
}

bool IsPermissionSetByAdministator(ContentSetting setting,
                                   const content_settings::SettingInfo& info) {
  return ((setting == ContentSetting::CONTENT_SETTING_BLOCK ||
           setting == ContentSetting::CONTENT_SETTING_ALLOW) &&
          (info.source == SettingSource::kPolicy ||
           info.source == SettingSource::kSupervised));
}

#if BUILDFLAG(IS_MAC)
void OpenCameraSystemSettingsOnMacOS() {
  if (system_media_permissions::CheckSystemVideoCapturePermission() ==
      system_media_permissions::SystemPermission::kDenied) {
    base::mac::OpenSystemSettingsPane(
        base::mac::SystemSettingsPane::kPrivacySecurity_Camera);
  }
}

void OpenMicSystemSettingsOnMacOS() {
  if (system_media_permissions::CheckSystemAudioCapturePermission() ==
      system_media_permissions::SystemPermission::kDenied) {
    base::mac::OpenSystemSettingsPane(
        base::mac::SystemSettingsPane::kPrivacySecurity_Microphone);
  }
}

bool ShouldShowSystemSettingsViewOnMacOS(ContentSettingsType type) {
  return (type == ContentSettingsType::MEDIASTREAM_MIC &&
          system_media_permissions::CheckSystemAudioCapturePermission() ==
              system_media_permissions::SystemPermission::kDenied) ||
         (type == ContentSettingsType::MEDIASTREAM_CAMERA &&
          system_media_permissions::CheckSystemVideoCapturePermission() ==
              system_media_permissions::SystemPermission::kDenied);
}

bool ShouldShowOSPromptViewOnMacOS(ContentSettingsType type) {
  return (type == ContentSettingsType::MEDIASTREAM_MIC &&
          system_media_permissions::CheckSystemAudioCapturePermission() ==
              system_media_permissions::SystemPermission::kNotDetermined) ||
         (type == ContentSettingsType::MEDIASTREAM_CAMERA &&
          system_media_permissions::CheckSystemVideoCapturePermission() ==
              system_media_permissions::SystemPermission::kNotDetermined);
}
#endif

permissions::ElementAnchoredBubbleVariant GetVariant(
    EmbeddedPermissionPrompt::Variant variant) {
  switch (variant) {
    case EmbeddedPermissionPrompt::Variant::kUninitialized:
      return permissions::ElementAnchoredBubbleVariant::UNINITIALIZED;
    case EmbeddedPermissionPrompt::Variant::kAdministratorGranted:
      return permissions::ElementAnchoredBubbleVariant::ADMINISTRATOR_GRANTED;
    case EmbeddedPermissionPrompt::Variant::kPreviouslyGranted:
      return permissions::ElementAnchoredBubbleVariant::PREVIOUSLY_GRANTED;
    case EmbeddedPermissionPrompt::Variant::kOsSystemSettings:
      return permissions::ElementAnchoredBubbleVariant::OS_SYSTEM_SETTINGS;
    case EmbeddedPermissionPrompt::Variant::kOsPrompt:
      return permissions::ElementAnchoredBubbleVariant::OS_PROMPT;
    case EmbeddedPermissionPrompt::Variant::kAsk:
      return permissions::ElementAnchoredBubbleVariant::ASK;
    case EmbeddedPermissionPrompt::Variant::kPreviouslyDenied:
      return permissions::ElementAnchoredBubbleVariant::PREVIOUSLY_DENIED;
    case EmbeddedPermissionPrompt::Variant::kAdministratorDenied:
      return permissions::ElementAnchoredBubbleVariant::ADMINISTRATOR_DENIED;
  }

  NOTREACHED();
  return permissions::ElementAnchoredBubbleVariant::UNINITIALIZED;
}
}  // namespace

EmbeddedPermissionPrompt::EmbeddedPermissionPrompt(
    Browser* browser,
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate)
    : PermissionPromptDesktop(browser, web_contents, delegate),
      delegate_(delegate) {
  CloseCurrentViewAndMaybeShowNext(/*first_prompt=*/true);
}

EmbeddedPermissionPrompt::~EmbeddedPermissionPrompt() {
  CloseView();
}

// static
EmbeddedPermissionPrompt::Variant
EmbeddedPermissionPrompt::DeterminePromptVariant(
    ContentSetting setting,
    const content_settings::SettingInfo& info,
    ContentSettingsType type) {
  // If the administrator blocked the permission, there is nothing the user can
  // do. Presenting them with a different screen in unproductive.
  if (IsPermissionSetByAdministator(setting, info) &&
      setting == CONTENT_SETTING_BLOCK) {
    return Variant::kAdministratorDenied;
  }

  // Determine if we can directly show one of the OS views. The "System
  // Settings" view is higher priority then all the other remaining options,
  // whereas the "OS Prompt" view is only higher priority then the views that
  // are associated with a site-level allowed state.
  // TODO(crbug.com/40275129): Handle going to Windows settings.
#if BUILDFLAG(IS_MAC)
  if (ShouldShowSystemSettingsViewOnMacOS(type)) {
    return Variant::kOsSystemSettings;
  }

  if (setting == CONTENT_SETTING_ALLOW && ShouldShowOSPromptViewOnMacOS(type)) {
    return Variant::kOsPrompt;
  }
#endif

  if (IsPermissionSetByAdministator(setting, info)) {
    return Variant::kAdministratorGranted;
  }

  switch (setting) {
    case CONTENT_SETTING_ASK:
      return Variant::kAsk;
    case CONTENT_SETTING_ALLOW:
      return Variant::kPreviouslyGranted;
    case CONTENT_SETTING_BLOCK:
      return Variant::kPreviouslyDenied;
    default:
      break;
  }

  return Variant::kUninitialized;
}

void EmbeddedPermissionPrompt::CloseCurrentViewAndMaybeShowNext(
    bool first_prompt) {
  if (!first_prompt) {
    CloseView();
  }

  auto* map = HostContentSettingsMapFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  content_settings::SettingInfo info;

  for (const auto& request : delegate()->Requests()) {
    ContentSettingsType type = request->GetContentSettingsType();
    ContentSetting setting =
        map->GetContentSetting(delegate()->GetRequestingOrigin(),
                               delegate()->GetEmbeddingOrigin(), type, &info);
    Variant current_request_variant =
        DeterminePromptVariant(setting, info, type);
    PrioritizeAndMergeNewVariant(current_request_variant, type);
  }

  RebuildRequests();

  EmbeddedPermissionPromptBaseView* prompt_view = nullptr;

  switch (embedded_prompt_variant_) {
    case Variant::kAsk:
      prompt_view = new EmbeddedPermissionPromptAskView(
          browser(), weak_factory_.GetWeakPtr());
      permissions::PermissionUmaUtil::RecordElementAnchoredBubbleVariantUMA(
          delegate()->Requests(),
          permissions::ElementAnchoredBubbleVariant::ASK);
      break;
    case Variant::kPreviouslyGranted:
      if (first_prompt) {
        prompt_view = new EmbeddedPermissionPromptPreviouslyGrantedView(
            browser(), weak_factory_.GetWeakPtr());
        permissions::PermissionUmaUtil::RecordElementAnchoredBubbleVariantUMA(
            delegate()->Requests(),
            permissions::ElementAnchoredBubbleVariant::PREVIOUSLY_GRANTED);
      } else {
        delegate()->FinalizeCurrentRequests();
        return;
      }
      break;
    case Variant::kPreviouslyDenied:
      prompt_view = new EmbeddedPermissionPromptPreviouslyDeniedView(
          browser(), weak_factory_.GetWeakPtr());
      permissions::PermissionUmaUtil::RecordElementAnchoredBubbleVariantUMA(
          delegate()->Requests(),
          permissions::ElementAnchoredBubbleVariant::PREVIOUSLY_DENIED);
      break;
    case Variant::kOsPrompt:
      prompt_view = new EmbeddedPermissionPromptShowSystemPromptView(
          browser(), weak_factory_.GetWeakPtr());
      permissions::PermissionUmaUtil::RecordElementAnchoredBubbleVariantUMA(
          delegate()->Requests(),
          permissions::ElementAnchoredBubbleVariant::OS_PROMPT);
      current_variant_first_display_time_ = base::Time::Now();
// This view has no buttons, so the OS level prompt should be triggered at the
// same time as the |EmbeddedPermissionPromptShowSystemPromptView|.
#if BUILDFLAG(IS_MAC)
      PromptForOsPermission();
#endif
      break;
    case Variant::kOsSystemSettings:
      prompt_view = new EmbeddedPermissionPromptSystemSettingsView(
          browser(), weak_factory_.GetWeakPtr());
      permissions::PermissionUmaUtil::RecordElementAnchoredBubbleVariantUMA(
          delegate()->Requests(),
          permissions::ElementAnchoredBubbleVariant::OS_SYSTEM_SETTINGS);
      current_variant_first_display_time_ = base::Time::Now();
      break;
    case Variant::kAdministratorGranted:
      prompt_view = new EmbeddedPermissionPromptPolicyView(
          browser(), weak_factory_.GetWeakPtr(),
          /*is_permission_allowed=*/true);
      permissions::PermissionUmaUtil::RecordElementAnchoredBubbleVariantUMA(
          delegate()->Requests(),
          permissions::ElementAnchoredBubbleVariant::ADMINISTRATOR_GRANTED);
      break;
    case Variant::kAdministratorDenied:
      prompt_view = new EmbeddedPermissionPromptPolicyView(
          browser(), weak_factory_.GetWeakPtr(),
          /*is_permission_allowed=*/false);
      permissions::PermissionUmaUtil::RecordElementAnchoredBubbleVariantUMA(
          delegate()->Requests(),
          permissions::ElementAnchoredBubbleVariant::ADMINISTRATOR_DENIED);
      break;
    case Variant::kUninitialized:
      NOTREACHED();
  }

  if (prompt_view) {
    prompt_view_tracker_.SetView(prompt_view);
    content_scrim_widget_ =
        EmbeddedPermissionPromptContentScrimView::CreateScrimWidget(
            weak_factory_.GetWeakPtr(),
            SkColorSetA(web_contents()->GetColorProvider().GetColor(
                            ui::kColorRefNeutral20),
                        0.8 * SK_AlphaOPAQUE));
    prompt_view->UpdateAnchor(content_scrim_widget_.get());
    prompt_view->Show();
  }
}

EmbeddedPermissionPrompt::TabSwitchingBehavior
EmbeddedPermissionPrompt::GetTabSwitchingBehavior() {
  return TabSwitchingBehavior::kDestroyPromptAndIgnoreRequest;
}

void EmbeddedPermissionPrompt::RecordOsMetrics(
    permissions::OsScreenAction action) {
  permissions::OsScreen screen;

  switch (embedded_prompt_variant_) {
    case Variant::kOsPrompt:
      screen = permissions::OsScreen::OS_PROMPT;
      break;
    case Variant::kOsSystemSettings:
      screen = permissions::OsScreen::OS_SYSTEM_SETTINGS;
      break;
    default:
      return;
  }

  base::TimeDelta time_to_decision =
      base::Time::Now() - current_variant_first_display_time_;
  permissions::PermissionUmaUtil::RecordElementAnchoredBubbleOsMetrics(
      delegate()->Requests(), screen, action, time_to_decision);
}

void EmbeddedPermissionPrompt::RecordPermissionActionUKM(
    permissions::ElementAnchoredBubbleAction action) {
  // There should never be more than SCREEN_COUNTER_MAXIMUM screens. If this is
  // hit something has gone wrong and we're probably caught in a loop showing
  // the same screens over and over.
  DCHECK_LE(prompt_screen_counter_for_metrics_, SCREEN_COUNTER_MAXIMUM);

  permissions::PermissionUmaUtil::RecordElementAnchoredPermissionPromptAction(
      // This represents all the requests for the entire prompt.
      delegate_->Requests(),
      // This only contains the requests for the currently active screen, which
      // could sometimes be a subset of all requests for the entire prompt.
      Requests(), action, GetVariant(embedded_prompt_variant_),
      prompt_screen_counter_for_metrics_, delegate_->GetRequestingOrigin(),
      delegate_->GetAssociatedWebContents(),
      delegate_->GetAssociatedWebContents()->GetBrowserContext());

  ++prompt_screen_counter_for_metrics_;
}

permissions::PermissionPromptDisposition
EmbeddedPermissionPrompt::GetPromptDisposition() const {
  return permissions::PermissionPromptDisposition::ELEMENT_ANCHORED_BUBBLE;
}

bool EmbeddedPermissionPrompt::ShouldFinalizeRequestAfterDecided() const {
  return false;
}

void EmbeddedPermissionPrompt::PrecalculateVariantsForMetrics() {
  if (embedded_prompt_variant_ == Variant::kUninitialized) {
    return;
  }

  site_level_prompt_variant_ = embedded_prompt_variant_;

#if BUILDFLAG(IS_MAC)
  if (os_prompt_variant_ == Variant::kUninitialized) {
    for (const auto& request : delegate()->Requests()) {
      if (ShouldShowOSPromptViewOnMacOS(request->GetContentSettingsType())) {
        os_prompt_variant_ = Variant::kOsPrompt;
        break;
      }
    }
  }

  if (os_system_settings_variant_ == Variant::kUninitialized) {
    for (const auto& request : delegate()->Requests()) {
      if (ShouldShowSystemSettingsViewOnMacOS(
              request->GetContentSettingsType())) {
        os_system_settings_variant_ = Variant::kOsSystemSettings;
        break;
      }
    }
  }
#endif  // BUILDFLAG(IS_MAC)
}

std::vector<permissions::ElementAnchoredBubbleVariant>
EmbeddedPermissionPrompt::GetPromptVariants() const {
  std::vector<permissions::ElementAnchoredBubbleVariant> variants;

  // Current prompt variant when the user takes an action on a site level
  // prompt.
  if (embedded_prompt_variant_ != Variant::kUninitialized) {
    variants.push_back(GetVariant(embedded_prompt_variant_));
  }

#if BUILDFLAG(IS_MAC)
  if (os_prompt_variant_ != Variant::kUninitialized) {
    variants.push_back(GetVariant(os_prompt_variant_));
  }
  if (os_system_settings_variant_ != Variant::kUninitialized) {
    variants.push_back(GetVariant(os_system_settings_variant_));
  }
#endif  // BUILDFLAG(IS_MAC)

  return variants;
}

bool EmbeddedPermissionPrompt::IsAskPrompt() const {
  return (embedded_prompt_variant_ == Variant::kAsk);
}

void EmbeddedPermissionPrompt::Allow() {
  PrecalculateVariantsForMetrics();
  RecordPermissionActionUKM(permissions::ElementAnchoredBubbleAction::kGranted);
  delegate_->Accept();
  CloseCurrentViewAndMaybeShowNext(/*first_prompt=*/false);
}

void EmbeddedPermissionPrompt::AllowThisTime() {
  PrecalculateVariantsForMetrics();
  RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kGrantedOnce);
  delegate_->AcceptThisTime();
  CloseCurrentViewAndMaybeShowNext(/*first_prompt=*/false);
}

void EmbeddedPermissionPrompt::Dismiss() {
  PrecalculateVariantsForMetrics();
  permissions::PermissionUmaUtil::RecordElementAnchoredBubbleDismiss(
      delegate()->Requests(), permissions::DismissedReason::DISMISSED_X_BUTTON);
  RecordOsMetrics(permissions::OsScreenAction::DISMISSED_X_BUTTON);
  RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kDismissedXButton);

  delegate_->Dismiss();
  delegate_->FinalizeCurrentRequests();
}

void EmbeddedPermissionPrompt::Acknowledge() {
  // TOOO(crbug.com/1462930): Find how to distinguish between a dismiss and an
  // acknowledge.
  RecordPermissionActionUKM(permissions::ElementAnchoredBubbleAction::kOk);
  CloseView();
  delegate_->FinalizeCurrentRequests();
}

void EmbeddedPermissionPrompt::StopAllowing() {
  PrecalculateVariantsForMetrics();
  RecordPermissionActionUKM(permissions::ElementAnchoredBubbleAction::kDenied);
  delegate_->Deny();
  delegate_->FinalizeCurrentRequests();
}

void EmbeddedPermissionPrompt::ShowSystemSettings() {
  const auto& requests = delegate()->Requests();
  CHECK_GT(requests.size(), 0U);
// TODO(crbug.com/40275129) Chrome always shows the first permission in a group,
// as it is not possible to open multiple System Setting pages. Figure out a
// better way to handle this scenario.
#if BUILDFLAG(IS_MAC)
  RecordOsMetrics(permissions::OsScreenAction::SYSTEM_SETTINGS);
  RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kSystemSettings);
  if (requests_[0]->request_type() == permissions::RequestType::kCameraStream) {
    OpenCameraSystemSettingsOnMacOS();
  } else if (requests_[0]->request_type() ==
             permissions::RequestType::kMicStream) {
    OpenMicSystemSettingsOnMacOS();
  }

#endif
}

base::WeakPtr<permissions::PermissionPrompt::Delegate>
EmbeddedPermissionPrompt::GetPermissionPromptDelegate() const {
  return delegate_->GetWeakPtr();
}

const std::vector<raw_ptr<permissions::PermissionRequest, VectorExperimental>>&
EmbeddedPermissionPrompt::Requests() const {
  return requests_;
}

void EmbeddedPermissionPrompt::DismissScrim() {
  permissions::PermissionUmaUtil::RecordElementAnchoredBubbleDismiss(
      delegate()->Requests(), permissions::DismissedReason::DISMISSED_SCRIM);
  RecordOsMetrics(permissions::OsScreenAction::DISMISSED_SCRIM);
  RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kDismissedScrim);

  CloseView();
  PrecalculateVariantsForMetrics();
  delegate_->Dismiss();
  delegate_->FinalizeCurrentRequests();
}

void EmbeddedPermissionPrompt::PromptForOsPermission() {
#if BUILDFLAG(IS_MAC)
  // We currently support <=2 grouped permissions.
  CHECK_LE(prompt_types_.size(), 2U);

  for (const auto prompt : prompt_types_) {
    RequestMacOSMediaSystemPermission(prompt, prompt_types_.size() == 2U);
  }
#endif
}

#if BUILDFLAG(IS_MAC)
void EmbeddedPermissionPrompt::OnRequestSystemMediaPermissionResponse(
    const ContentSettingsType request_type,
    bool grouped_permissions) {
  system_media_permissions::SystemPermission permission,
      other_permission =
          system_media_permissions::SystemPermission::kNotDetermined;

  if (request_type == ContentSettingsType::MEDIASTREAM_MIC) {
    permission = system_media_permissions::CheckSystemAudioCapturePermission();
    other_permission =
        grouped_permissions
            ? system_media_permissions::CheckSystemVideoCapturePermission()
            : system_media_permissions::SystemPermission::kNotDetermined;
  }

  if (request_type == ContentSettingsType::MEDIASTREAM_CAMERA) {
    permission = system_media_permissions::CheckSystemVideoCapturePermission();
    other_permission =
        grouped_permissions
            ? system_media_permissions::CheckSystemAudioCapturePermission()
            : system_media_permissions::SystemPermission::kNotDetermined;
  }

  switch (permission) {
    case system_media_permissions::SystemPermission::kRestricted:
      break;
    case system_media_permissions::SystemPermission::kDenied:
      RecordOsMetrics(permissions::OsScreenAction::OS_PROMPT_DENIED);
      break;
    case system_media_permissions::SystemPermission::kAllowed:
      RecordOsMetrics(permissions::OsScreenAction::OS_PROMPT_ALLOWED);
      break;
    case system_media_permissions::SystemPermission::kNotDetermined:
      NOTREACHED();
  }
  // Do not finalize request until all the necessary system permissions are
  // granted.
  if (!grouped_permissions ||
      other_permission !=
          system_media_permissions::SystemPermission::kNotDetermined) {
    CloseView();
    delegate_->FinalizeCurrentRequests();
  }
}

// TODO: Refactor this logic for PEPC and other permission prompts, to avoid
// code duplication.
void EmbeddedPermissionPrompt::RequestMacOSMediaSystemPermission(
    const ContentSettingsType request_type,
    bool grouped_permissions) {
  if (request_type == ContentSettingsType::MEDIASTREAM_MIC) {
    system_media_permissions::RequestSystemAudioCapturePermission(
        base::BindOnce(
            &EmbeddedPermissionPrompt::OnRequestSystemMediaPermissionResponse,
            weak_factory_.GetWeakPtr(), request_type, grouped_permissions));
    return;
  }

  if (request_type == ContentSettingsType::MEDIASTREAM_CAMERA) {
    system_media_permissions::RequestSystemVideoCapturePermission(
        base::BindOnce(
            &EmbeddedPermissionPrompt::OnRequestSystemMediaPermissionResponse,
            weak_factory_.GetWeakPtr(), request_type, grouped_permissions));
    return;
  }
}
#endif

void EmbeddedPermissionPrompt::PrioritizeAndMergeNewVariant(
    EmbeddedPermissionPrompt::Variant new_variant,
    ContentSettingsType new_type) {
  // The new variant can be grouped with the already existing one.
  if (CanGroupVariants(embedded_prompt_variant_, new_variant)) {
    prompt_types_.insert(new_type);
    embedded_prompt_variant_ = std::max(embedded_prompt_variant_, new_variant);
    return;
  }

  // The existing variant is higher priority than the new one.
  if (embedded_prompt_variant_ > new_variant) {
    return;
  }

  // The new variant has higher priority than the existing one.
  prompt_types_.clear();
  prompt_types_.insert(new_type);
  embedded_prompt_variant_ = new_variant;
}

void EmbeddedPermissionPrompt::RebuildRequests() {
  if (requests_.size() != prompt_types_.size()) {
    const auto& requests = delegate()->Requests();
    for (permissions::PermissionRequest* request : requests) {
      if (prompt_types_.contains(request->GetContentSettingsType())) {
        requests_.push_back(request);
      }
    }
  }
}

void EmbeddedPermissionPrompt::CloseView() {
  if (auto* prompt_view = static_cast<EmbeddedPermissionPromptBaseView*>(
          prompt_view_tracker_.view())) {
    prompt_view->PrepareToClose();
    prompt_view->GetWidget()->Close();
    prompt_view_tracker_.SetView(nullptr);

    requests_.clear();
    prompt_types_.clear();
    embedded_prompt_variant_ = Variant::kUninitialized;
  }

  if (content_scrim_widget_) {
    content_scrim_widget_->Close();
    content_scrim_widget_.reset();
  }
}
