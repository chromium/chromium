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
#include "components/download/public/common/download_danger_type.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/vector_icons.h"

using download::DownloadItem;
using offline_items_collection::FailState;
using TailoredVerdict = safe_browsing::ClientDownloadResponse::TailoredVerdict;
using TailoredWarningType = DownloadUIModel::TailoredWarningType;

DownloadBubbleRowViewInfoObserver::DownloadBubbleRowViewInfoObserver() =
    default;

DownloadBubbleRowViewInfoObserver::~DownloadBubbleRowViewInfoObserver() {
  CHECK(!IsInObserverList());
}

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
    const std::vector<DownloadBubbleQuickAction>& actions) {
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
  icon_and_color_ = IconAndColorForDownload(*model_);
  // Add primary button/quick actions for in-progress (paused or active), and
  // completed downloads
  quick_actions_ = QuickActionsForDownload(*model_);
  progress_bar_ = ProgressBarForDownload(*model_);

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
          model_->profile(), model_->GetDownloadItem())) {
    switch (model_->GetDangerType()) {
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
        primary_button_command_ = DownloadCommands::Command::REVIEW;
        return;
      case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
        secondary_text_color_ = kColorDownloadItemTextWarning;
        primary_button_command_ = DownloadCommands::Command::REVIEW;
        return;
      case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
        secondary_text_color_ = kColorDownloadItemTextWarning;
        primary_button_command_ = DownloadCommands::Command::REVIEW;
        return;
      default:
        break;
    }
  }

  if (TailoredWarningType type = model_->GetTailoredWarningType();
      type != TailoredWarningType::kNoTailoredWarning) {
    PopulateForTailoredWarning(type);
    return;
  }

  switch (model_->GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      if (model_->IsExtensionDownload()) {
        PopulateSuspiciousUiPattern();
        return;
      } else {
        if (WasSafeBrowsingVerdictObtained(model_->GetDownloadItem())) {
          PopulateSuspiciousUiPattern();
          return;
        }
        if (ShouldShowWarningForNoSafeBrowsing(model_->profile())) {
          PopulateForFileTypeWarningNoSafeBrowsing();
          return;
        }
        PopulateSuspiciousUiPattern();
        return;
      }
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      PopulateDangerousUiPattern();
      return;
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
        secondary_text_color_ = kColorDownloadItemTextWarning;
        return;
      } else {
        PopulateSuspiciousUiPattern();
        return;
      }
    }
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING: {
      has_subpage_ = true;
      secondary_text_color_ = kColorDownloadItemTextWarning;
      primary_button_command_ = DownloadCommands::Command::DISCARD;
      return;
    }
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING: {
      secondary_text_color_ = kColorDownloadItemTextWarning;
      has_subpage_ = true;
      return;
    }
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING: {
      secondary_text_color_ = kColorDownloadItemTextWarning;
      has_subpage_ = true;
      return;
    }
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
      has_subpage_ = true;
      return;
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
      has_subpage_ = true;
      return;
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
      primary_button_command_ = DownloadCommands::Command::OPEN_WHEN_COMPLETE;
      secondary_text_color_ = kColorDownloadItemTextWarning;
      main_button_enabled_ = false;
      return;
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      break;
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
      return;
    }
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE: {
      has_subpage_ = true;
      return;
    }
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK: {
      if (enterprise_connectors::ShouldPromptReviewForDownload(
              model_->profile(), model_->GetDownloadItem())) {
        primary_button_command_ = DownloadCommands::Command::REVIEW;
      } else {
        has_subpage_ = true;
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
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      break;
  }

  switch (fail_state) {
    case FailState::FILE_BLOCKED: {
      has_subpage_ = true;
      return;
    }
    case FailState::FILE_NAME_TOO_LONG:
    case FailState::FILE_NO_SPACE:
    case FailState::SERVER_UNAUTHORIZED: {
      has_subpage_ = true;
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
      NOTREACHED_IN_MIGRATION();
      break;
    case FailState::NO_FAILURE:
      return;
  }

  primary_button_command_ = model_->CanResume()
                                ? DownloadCommands::Command::RESUME
                                : DownloadCommands::Command::RETRY;
}

void DownloadBubbleRowViewInfo::PopulateForTailoredWarning(
    TailoredWarningType tailored_warning_type) {
  CHECK(model_->GetDownloadItem());
  switch (tailored_warning_type) {
    case TailoredWarningType::kSuspiciousArchive:
      return PopulateSuspiciousUiPattern();
    case TailoredWarningType::kCookieTheft:
    case TailoredWarningType::kCookieTheftWithAccountInfo:
      return PopulateDangerousUiPattern();
    case TailoredWarningType::kNoTailoredWarning: {
      NOTREACHED_IN_MIGRATION();
      return;
    }
  }
}

void DownloadBubbleRowViewInfo::PopulateForFileTypeWarningNoSafeBrowsing() {
  PopulateSuspiciousUiPattern();
}

void DownloadBubbleRowViewInfo::PopulateSuspiciousUiPattern() {
  has_subpage_ = true;
  secondary_text_color_ = kColorDownloadItemTextWarning;
}

void DownloadBubbleRowViewInfo::PopulateDangerousUiPattern() {
  has_subpage_ = true;
  secondary_text_color_ = kColorDownloadItemTextDangerous;
}

void DownloadBubbleRowViewInfo::Reset() {
  icon_and_color_ = IconAndColor{};
  secondary_text_color_ = std::nullopt;
  quick_actions_.clear();
  main_button_enabled_ = true;
  has_subpage_ = false;
  primary_button_command_ = std::nullopt;
  progress_bar_ = DownloadBubbleProgressBar::NoProgressBar();
}

bool DownloadBubbleRowViewInfo::ShouldShowDeepScanNotice() const {
  return ShouldShowDeepScanPromptNotice(model_->profile(),
                                        model_->GetDangerType());
}
