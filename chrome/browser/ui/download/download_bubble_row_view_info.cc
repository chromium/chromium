// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_bubble_row_view_info.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/download_ui_safe_browsing_util.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/download/download_item_mode.h"
#include "chrome/grit/generated_resources.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/vector_icons.h"

using download::DownloadItem;
using offline_items_collection::FailState;
using TailoredVerdict = safe_browsing::ClientDownloadResponse::TailoredVerdict;

DownloadBubbleRowViewInfoObserver::DownloadBubbleRowViewInfoObserver() =
    default;

DownloadBubbleRowViewInfoObserver::~DownloadBubbleRowViewInfoObserver() {
  CHECK(!IsInObserverList());
}

DownloadBubbleRowViewInfo::QuickAction::QuickAction(
    DownloadCommands::Command command,
    const std::u16string& hover_text,
    const gfx::VectorIcon* icon)
    : command(command), hover_text(hover_text), icon(icon) {}

DownloadBubbleRowViewInfo::DownloadBubbleRowViewInfo(
    DownloadUIModel::DownloadUIModelPtr model)
    : model_(std::move(model)), state_(model_->GetState()) {
  // Ignore whether we changed anything because it's the initial setup
  PopulateFromModel();

  model_->SetDelegate(this);
}

DownloadBubbleRowViewInfo::~DownloadBubbleRowViewInfo() {
  model_->SetDelegate(nullptr);
}

void DownloadBubbleRowViewInfo::SetQuickActionsForTesting(
    const std::vector<QuickAction>& actions) {
  quick_actions_ = actions;
}

void DownloadBubbleRowViewInfo::OnDownloadOpened() {
  model_->SetActionedOn(true);
}

void DownloadBubbleRowViewInfo::OnDownloadUpdated() {
  if (state_ != model_->GetState()) {
    NotifyObservers(&DownloadBubbleRowViewInfoObserver::OnDownloadStateChanged,
                    state_, model_->GetState());
  }

  PopulateFromModel();
  NotifyObservers(&DownloadBubbleRowViewInfoObserver::OnInfoChanged);
}

void DownloadBubbleRowViewInfo::OnDownloadDestroyed(
    const offline_items_collection::ContentId& id) {
  NotifyObservers(&DownloadBubbleRowViewInfoObserver::OnDownloadDestroyed, id);
}

void DownloadBubbleRowViewInfo::PopulateFromModel() {
  Reset();
  switch (model_->GetState()) {
    case DownloadItem::IN_PROGRESS:
    case DownloadItem::COMPLETE:
      PopulateForInProgressOrComplete();
      return;
    case DownloadItem::INTERRUPTED: {
      const FailState fail_state = model_->GetLastFailState();
      if (fail_state != FailState::USER_CANCELED) {
        PopulateForInterrupted(fail_state);
        return;
      }
    }
      [[fallthrough]];
    case DownloadItem::CANCELLED:
    case DownloadItem::MAX_DOWNLOAD_STATE:
      PopulateForCancelled();
      return;
  }
}

void DownloadBubbleRowViewInfo::PopulateForInProgressOrComplete() {
  switch (model_->GetInsecureDownloadStatus()) {
    case download::DownloadItem::InsecureDownloadStatus::BLOCK:
    case download::DownloadItem::InsecureDownloadStatus::WARN:
      PopulateSuspiciousUiPattern();
      primary_button_command_ = DownloadCommands::Command::KEEP;
      return;
    case download::DownloadItem::InsecureDownloadStatus::UNKNOWN:
    case download::DownloadItem::InsecureDownloadStatus::SAFE:
    case download::DownloadItem::InsecureDownloadStatus::VALIDATED:
    case download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK:
      break;
  }

  if (enterprise_connectors::ShouldPromptReviewForDownload(
          model_->profile(), model_->GetDangerType())) {
    switch (model_->GetDangerType()) {
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
        icon_override_ = features::IsChromeRefresh2023()
                             ? &vector_icons::kDangerousChromeRefreshIcon
                             : &vector_icons::kDangerousIcon;
        secondary_color_ = kColorDownloadItemIconDangerous;
        primary_button_command_ = DownloadCommands::Command::REVIEW;
        return;
      case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
        icon_override_ = features::IsChromeRefresh2023()
                             ? &kDownloadWarningIcon
                             : &vector_icons::kNotSecureWarningIcon;
        secondary_color_ = kColorDownloadItemIconWarning;
        secondary_text_color_ = kColorDownloadItemTextWarning;
        primary_button_command_ = DownloadCommands::Command::REVIEW;
        return;
      case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
        icon_override_ = features::IsChromeRefresh2023()
                             ? &views::kInfoChromeRefreshIcon
                             : &views::kInfoIcon;
        secondary_color_ = kColorDownloadItemIconWarning;
        secondary_text_color_ = kColorDownloadItemTextWarning;
        primary_button_command_ = DownloadCommands::Command::REVIEW;
        return;
      default:
        break;
    }
  }

  if (model_->ShouldShowTailoredWarning()) {
    PopulateForTailoredWarning();
    return;
  }

  switch (model_->GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      if (model_->IsExtensionDownload()) {
        if (base::FeatureList::IsEnabled(
                safe_browsing::kImprovedDownloadBubbleWarnings)) {
          PopulateSuspiciousUiPattern();
          return;
        }
        has_subpage_ = true;
        icon_override_ = features::IsChromeRefresh2023()
                             ? &kDownloadWarningIcon
                             : &vector_icons::kNotSecureWarningIcon;
        secondary_color_ = kColorDownloadItemIconWarning;
        secondary_text_color_ = kColorDownloadItemTextWarning;
        return;
      } else {
        if (base::FeatureList::IsEnabled(
                safe_browsing::kImprovedDownloadBubbleWarnings)) {
          if (ShouldShowWarningForNoSafeBrowsing(model_->profile())) {
            PopulateForFileTypeWarningNoSafeBrowsing();
            return;
          }
          PopulateSuspiciousUiPattern();
          return;
        }

        has_subpage_ = true;
        icon_override_ = features::IsChromeRefresh2023()
                             ? &kDownloadWarningIcon
                             : &vector_icons::kNotSecureWarningIcon;
        secondary_color_ = ui::kColorSecondaryForeground;
        primary_button_command_ = DownloadCommands::Command::KEEP;
        return;
      }
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
      if (base::FeatureList::IsEnabled(
              safe_browsing::kImprovedDownloadBubbleWarnings)) {
        PopulateDangerousUiPattern();
        return;
      } else {
        has_subpage_ = true;
        icon_override_ = features::IsChromeRefresh2023()
                             ? &vector_icons::kDangerousChromeRefreshIcon
                             : &vector_icons::kDangerousIcon;
        secondary_color_ = kColorDownloadItemIconDangerous;
        primary_button_command_ = DownloadCommands::Command::DISCARD;
        return;
      }
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      if (base::FeatureList::IsEnabled(
              safe_browsing::kImprovedDownloadBubbleWarnings)) {
        PopulateDangerousUiPattern();
        return;
      } else {
        has_subpage_ = true;
        icon_override_ = features::IsChromeRefresh2023()
                             ? &kDownloadWarningIcon
                             : &vector_icons::kNotSecureWarningIcon;
        secondary_color_ = kColorDownloadItemIconWarning;
        secondary_text_color_ = kColorDownloadItemTextWarning;
        primary_button_command_ = DownloadCommands::Command::DISCARD;
        return;
      }
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      if (base::FeatureList::IsEnabled(
              safe_browsing::kImprovedDownloadBubbleWarnings)) {
        return PopulateDangerousUiPattern();
      } else {
        has_subpage_ = true;
        icon_override_ = features::IsChromeRefresh2023()
                             ? &kDownloadWarningIcon
                             : &vector_icons::kNotSecureWarningIcon;
        secondary_color_ = kColorDownloadItemIconDangerous;
        primary_button_command_ = DownloadCommands::Command::DISCARD;
        return;
      }
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT: {
      bool request_ap_verdicts = false;
#if BUILDFLAG(FULL_SAFE_BROWSING)
      request_ap_verdicts =
          safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
              model_->profile())
              ->IsUnderAdvancedProtection();
#endif
      if (request_ap_verdicts) {
        has_subpage_ = true;
        icon_override_ = features::IsChromeRefresh2023()
                             ? &kDownloadWarningIcon
                             : &vector_icons::kNotSecureWarningIcon;
        secondary_color_ = kColorDownloadItemIconWarning;
        secondary_text_color_ = kColorDownloadItemTextWarning;
        return;
      } else {
        if (base::FeatureList::IsEnabled(
                safe_browsing::kImprovedDownloadBubbleWarnings)) {
          PopulateSuspiciousUiPattern();
          return;
        }
        has_subpage_ = true;
        icon_override_ = features::IsChromeRefresh2023()
                             ? &kDownloadWarningIcon
                             : &vector_icons::kNotSecureWarningIcon;
        secondary_color_ = kColorDownloadItemIconWarning;
        secondary_text_color_ = kColorDownloadItemTextWarning;
        primary_button_command_ = DownloadCommands::Command::DISCARD;
        return;
      }
    }
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING: {
      has_subpage_ = true;
      icon_override_ = features::IsChromeRefresh2023()
                           ? &views::kInfoChromeRefreshIcon
                           : &views::kInfoIcon;
      secondary_color_ = kColorDownloadItemIconWarning;
      secondary_text_color_ = kColorDownloadItemTextWarning;
      primary_button_command_ = DownloadCommands::Command::DISCARD;
      return;
    }
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING: {
      icon_override_ = features::IsChromeRefresh2023()
                           ? &kDownloadWarningIcon
                           : &vector_icons::kNotSecureWarningIcon;
      secondary_color_ = kColorDownloadItemIconWarning;
      secondary_text_color_ = kColorDownloadItemTextWarning;
      has_subpage_ = true;
      return;
    }
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING: {
      // TODO(crbug.com/1491184): Implement UX for this danger type.
      NOTREACHED();
      return;
    }
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
      has_progress_bar_ = true;
      is_progress_bar_looping_ = true;
      has_subpage_ = true;
      icon_override_ = features::IsChromeRefresh2023()
                           ? &kDownloadWarningIcon
                           : &vector_icons::kNotSecureWarningIcon;
      secondary_color_ = kColorDownloadItemIconWarning;
      return;
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
      icon_override_ = features::IsChromeRefresh2023()
                           ? &kDownloadWarningIcon
                           : &vector_icons::kNotSecureWarningIcon;
      secondary_color_ = kColorDownloadItemIconWarning;
      primary_button_command_ = DownloadCommands::Command::OPEN_WHEN_COMPLETE;
      secondary_text_color_ = kColorDownloadItemTextWarning;
      main_button_enabled_ = false;
      return;
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_UNSUPPORTED_FILETYPE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      break;
  }

  // Add primary button/quick actions for in-progress (paused or active), and
  // completed downloads
  bool has_progress_bar = model_->GetState() == DownloadItem::IN_PROGRESS;
  if (has_progress_bar) {
    has_progress_bar_ = true;
    if (model_->IsPaused()) {
      if (download::IsDownloadBubbleV2Enabled(model_->profile())) {
        quick_actions_.emplace_back(
            DownloadCommands::Command::RESUME,
            l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_RESUME_QUICK_ACTION),
            features::IsChromeRefresh2023()
                ? &vector_icons::kPlayArrowChromeRefreshIcon
                : &vector_icons::kPlayArrowIcon);
        quick_actions_.emplace_back(
            DownloadCommands::Command::CANCEL,
            l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_CANCEL_QUICK_ACTION),
            features::IsChromeRefresh2023()
                ? &vector_icons::kCancelChromeRefreshIcon
                : &vector_icons::kCancelIcon);
      } else {
        primary_button_command_ = DownloadCommands::Command::RESUME;
      }
    } else {
      if (download::IsDownloadBubbleV2Enabled(model_->profile())) {
        quick_actions_.emplace_back(
            DownloadCommands::Command::PAUSE,
            l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_PAUSE_QUICK_ACTION),
            features::IsChromeRefresh2023()
                ? &vector_icons::kPauseChromeRefreshIcon
                : &vector_icons::kPauseIcon);
        quick_actions_.emplace_back(
            DownloadCommands::Command::CANCEL,
            l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_CANCEL_QUICK_ACTION),
            features::IsChromeRefresh2023()
                ? &vector_icons::kCancelChromeRefreshIcon
                : &vector_icons::kCancelIcon);
      } else {
        primary_button_command_ = DownloadCommands::Command::CANCEL;
      }
    }
  } else {
    if (download::IsDownloadBubbleV2Enabled(model_->profile())) {
      quick_actions_.emplace_back(
          DownloadCommands::Command::SHOW_IN_FOLDER,
          l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_SHOW_IN_FOLDER_QUICK_ACTION),
          features::IsChromeRefresh2023()
              ? &vector_icons::kFolderChromeRefreshIcon
              : &vector_icons::kFolderIcon);
      quick_actions_.emplace_back(
          DownloadCommands::Command::OPEN_WHEN_COMPLETE,
          l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_OPEN_QUICK_ACTION),
          features::IsChromeRefresh2023()
              ? &vector_icons::kLaunchChromeRefreshIcon
              : &kOpenInNewIcon);
    }
  }
}

void DownloadBubbleRowViewInfo::PopulateForInterrupted(
    offline_items_collection::FailState fail_state) {
  // Only handle danger types that are terminated in the interrupted state in
  // this function. The other danger types are handled in
  // `PopulateForInProgressOrComplete`.
  switch (model_->GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED: {
      has_subpage_ = true;
      icon_override_ = features::IsChromeRefresh2023()
                           ? &views::kInfoChromeRefreshIcon
                           : &views::kInfoIcon;
      secondary_color_ = kColorDownloadItemIconDangerous;
      return;
    }
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE: {
      has_subpage_ = true;
      icon_override_ = features::IsChromeRefresh2023()
                           ? &views::kInfoChromeRefreshIcon
                           : &views::kInfoIcon;
      secondary_color_ = kColorDownloadItemIconDangerous;
      return;
    }
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK: {
      if (enterprise_connectors::ShouldPromptReviewForDownload(
              model_->profile(), model_->GetDangerType())) {
        icon_override_ = features::IsChromeRefresh2023()
                             ? &kDownloadWarningIcon
                             : &vector_icons::kNotSecureWarningIcon;
        secondary_color_ = kColorDownloadItemIconDangerous;
        primary_button_command_ = DownloadCommands::Command::REVIEW;
      } else {
        has_subpage_ = true;
        icon_override_ = features::IsChromeRefresh2023()
                             ? &views::kInfoChromeRefreshIcon
                             : &views::kInfoIcon;
        secondary_color_ = kColorDownloadItemIconDangerous;
      }
      return;
    }
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_UNSUPPORTED_FILETYPE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      break;
  }

  switch (fail_state) {
    case FailState::FILE_BLOCKED: {
      has_subpage_ = true;
      icon_override_ = features::IsChromeRefresh2023()
                           ? &views::kInfoChromeRefreshIcon
                           : &views::kInfoIcon;
      secondary_color_ = kColorDownloadItemIconDangerous;
      return;
    }
    case FailState::FILE_NAME_TOO_LONG:
    case FailState::FILE_NO_SPACE:
    case FailState::SERVER_UNAUTHORIZED: {
      has_subpage_ = true;
      icon_override_ = features::IsChromeRefresh2023()
                           ? &vector_icons::kFileDownloadOffChromeRefreshIcon
                           : &vector_icons::kFileDownloadOffIcon;
      secondary_color_ = kColorDownloadItemIconDangerous;
      return;
    }
    // No Retry in these cases.
    case FailState::FILE_TOO_LARGE:
    case FailState::FILE_VIRUS_INFECTED:
    case FailState::FILE_SECURITY_CHECK_FAILED:
    case FailState::FILE_ACCESS_DENIED:
    case FailState::SERVER_FORBIDDEN:
    case FailState::FILE_SAME_AS_SOURCE:
    case FailState::SERVER_BAD_CONTENT: {
      icon_override_ = features::IsChromeRefresh2023()
                           ? &vector_icons::kFileDownloadOffChromeRefreshIcon
                           : &vector_icons::kFileDownloadOffIcon;
      secondary_color_ = kColorDownloadItemIconDangerous;
      return;
    }
    // Try resume if possible or retry if not in these cases, and in the default
    // case.
    case FailState::NETWORK_INVALID_REQUEST:
    case FailState::NETWORK_FAILED:
    case FailState::NETWORK_TIMEOUT:
    case FailState::NETWORK_DISCONNECTED:
    case FailState::NETWORK_SERVER_DOWN:
    case FailState::FILE_TRANSIENT_ERROR:
    case FailState::USER_SHUTDOWN:
    case FailState::CRASH:
    case FailState::SERVER_CONTENT_LENGTH_MISMATCH:
    case FailState::SERVER_NO_RANGE:
    case FailState::SERVER_CROSS_ORIGIN_REDIRECT:
    case FailState::FILE_FAILED:
    case FailState::FILE_HASH_MISMATCH:
    case FailState::SERVER_FAILED:
    case FailState::SERVER_CERT_PROBLEM:
    case FailState::SERVER_UNREACHABLE:
    case FailState::FILE_TOO_SHORT:
      break;
    // Not possible because the USER_CANCELED fail state does not allow a call
    // into this function
    case FailState::USER_CANCELED:
    // Deprecated
    case FailState::NETWORK_INSTABILITY:
    case FailState::CANNOT_DOWNLOAD:
      NOTREACHED();
      break;
    case FailState::NO_FAILURE:
      return;
  }

  icon_override_ = features::IsChromeRefresh2023()
                       ? &vector_icons::kFileDownloadOffChromeRefreshIcon
                       : &vector_icons::kFileDownloadOffIcon;
  secondary_color_ = kColorDownloadItemIconDangerous;
  if (download::IsDownloadBubbleV2Enabled(model_->profile())) {
    primary_button_command_ = model_->CanResume()
                                  ? DownloadCommands::Command::RESUME
                                  : DownloadCommands::Command::RETRY;
  }
}

void DownloadBubbleRowViewInfo::PopulateForCancelled() {
  icon_override_ = features::IsChromeRefresh2023()
                       ? &vector_icons::kFileDownloadOffChromeRefreshIcon
                       : &vector_icons::kFileDownloadOffIcon;
}

void DownloadBubbleRowViewInfo::PopulateForTailoredWarning() {
  CHECK(model_->GetDownloadItem());
  download::DownloadDangerType danger_type = model_->GetDangerType();
  TailoredVerdict tailored_verdict = safe_browsing::DownloadProtectionService::
      GetDownloadProtectionTailoredVerdict(model_->GetDownloadItem());

  // Suspicious archives
  if (danger_type == download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT &&
      tailored_verdict.tailored_verdict_type() ==
          TailoredVerdict::SUSPICIOUS_ARCHIVE) {
    PopulateSuspiciousUiPattern();
    return;
  }

  // Cookie theft
  if (danger_type ==
          download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE &&
      tailored_verdict.tailored_verdict_type() ==
          TailoredVerdict::COOKIE_THEFT) {
    PopulateDangerousUiPattern();
    return;
  }

  NOTREACHED();
}

void DownloadBubbleRowViewInfo::PopulateForFileTypeWarningNoSafeBrowsing() {
  PopulateSuspiciousUiPattern();
}

void DownloadBubbleRowViewInfo::PopulateSuspiciousUiPattern() {
  has_subpage_ = true;
  icon_override_ = features::IsChromeRefresh2023()
                       ? &kDownloadWarningIcon
                       : &vector_icons::kNotSecureWarningIcon,
  secondary_color_ = kColorDownloadItemIconWarning;
  secondary_text_color_ = kColorDownloadItemTextWarning;
}

void DownloadBubbleRowViewInfo::PopulateDangerousUiPattern() {
  has_subpage_ = true;
  icon_override_ = features::IsChromeRefresh2023()
                       ? &vector_icons::kDangerousChromeRefreshIcon
                       : &vector_icons::kDangerousIcon;
  secondary_color_ = kColorDownloadItemIconDangerous;
  secondary_text_color_ = kColorDownloadItemTextDangerous;
}

void DownloadBubbleRowViewInfo::Reset() {
  icon_override_ = nullptr;
  secondary_color_ = ui::kColorSecondaryForeground;
  secondary_text_color_ = absl::nullopt;
  quick_actions_.clear();
  main_button_enabled_ = true;
  has_subpage_ = false;
  primary_button_command_ = absl::nullopt;
  has_progress_bar_ = false;
  is_progress_bar_looping_ = false;
}
