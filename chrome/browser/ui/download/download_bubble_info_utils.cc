// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_bubble_info_utils.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/views/vector_icons.h"

using download::DownloadItem;
using TailoredWarningType = DownloadUIModel::TailoredWarningType;

namespace {

IconAndColor IconAndColorForDangerousUiPattern() {
  return IconAndColor{&vector_icons::kDangerousChromeRefreshIcon,
                      kColorDownloadItemIconDangerous};
}

IconAndColor IconAndColorForSuspiciousUiPattern() {
  return IconAndColor{&kDownloadWarningIcon, kColorDownloadItemIconWarning};
}

IconAndColor IconAndColorForDownloadOff() {
  return IconAndColor{&vector_icons::kFileDownloadOffChromeRefreshIcon,
                      ui::kColorSecondaryForeground};
}

IconAndColor IconAndColorForInterrupted(const DownloadUIModel& model) {
  // Only handle danger types that are terminated in the interrupted state in
  // this function. The other danger types are handled in
  // `IconAndColorForInProgressOrComplete`.
  switch (model.GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED:
      return IconAndColor{&views::kInfoChromeRefreshIcon,
                          kColorDownloadItemIconDangerous};
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK: {
      if (enterprise_connectors::ShouldPromptReviewForDownload(
              model.profile(), model.GetDownloadItem())) {
        return IconAndColor{&kDownloadWarningIcon,
                            kColorDownloadItemIconDangerous};
      } else {
        return IconAndColor{&views::kInfoChromeRefreshIcon,
                            kColorDownloadItemIconDangerous};
      }
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
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      break;
  }

  if (model.GetLastFailState() ==
      offline_items_collection::FailState::FILE_BLOCKED) {
    return IconAndColor{&views::kInfoChromeRefreshIcon,
                        kColorDownloadItemIconDangerous};
  }

  return IconAndColor{&vector_icons::kFileDownloadOffChromeRefreshIcon,
                      kColorDownloadItemIconDangerous};
}

IconAndColor IconAndColorForTailoredWarning(const DownloadUIModel& model) {
  CHECK(model.GetDownloadItem());
  switch (model.GetTailoredWarningType()) {
    case DownloadUIModel::TailoredWarningType::kSuspiciousArchive:
      return IconAndColorForSuspiciousUiPattern();
    case DownloadUIModel::TailoredWarningType::kCookieTheft:
    case DownloadUIModel::TailoredWarningType::kCookieTheftWithAccountInfo:
      return IconAndColorForDangerousUiPattern();
    case DownloadUIModel::TailoredWarningType::kNoTailoredWarning: {
      NOTREACHED_IN_MIGRATION();
      return IconAndColor{};
    }
  }
}

IconAndColor IconAndColorForInProgressOrComplete(const DownloadUIModel& model) {
  switch (model.GetInsecureDownloadStatus()) {
    case download::DownloadItem::InsecureDownloadStatus::BLOCK:
    case download::DownloadItem::InsecureDownloadStatus::WARN:
      // The insecure warning uses the suspicious warning pattern but has a
      // primary button to keep the file.
      return IconAndColorForSuspiciousUiPattern();
    case download::DownloadItem::InsecureDownloadStatus::UNKNOWN:
    case download::DownloadItem::InsecureDownloadStatus::SAFE:
    case download::DownloadItem::InsecureDownloadStatus::VALIDATED:
    case download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK:
      break;
  }

  if (enterprise_connectors::ShouldPromptReviewForDownload(
          model.profile(), model.GetDownloadItem())) {
    switch (model.GetDangerType()) {
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
        return IconAndColor{&vector_icons::kDangerousChromeRefreshIcon,
                            kColorDownloadItemIconDangerous};
      case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
        return IconAndColor{&kDownloadWarningIcon,
                            kColorDownloadItemIconWarning};
      case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
        return IconAndColor{&views::kInfoChromeRefreshIcon,
                            kColorDownloadItemIconWarning};
      default:
        break;
    }
  }

  if (DownloadUIModel::TailoredWarningType type =
          model.GetTailoredWarningType();
      type != DownloadUIModel::TailoredWarningType::kNoTailoredWarning) {
    return IconAndColorForTailoredWarning(model);
  }

  switch (model.GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return IconAndColorForSuspiciousUiPattern();

    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return IconAndColorForDangerousUiPattern();

    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
      return IconAndColor{&views::kInfoChromeRefreshIcon,
                          kColorDownloadItemIconWarning};
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
      return IconAndColor{&kDownloadWarningIcon, kColorDownloadItemIconWarning};
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

  return IconAndColor{};
}

}  // namespace

// static
DownloadBubbleProgressBar DownloadBubbleProgressBar::NoProgressBar() {
  return DownloadBubbleProgressBar{/*is_visible=*/false, /*is_looping=*/false};
}

// static
DownloadBubbleProgressBar DownloadBubbleProgressBar::ProgressBar() {
  return DownloadBubbleProgressBar{/*is_visible=*/true, /*is_looping=*/false};
}

// static
DownloadBubbleProgressBar DownloadBubbleProgressBar::LoopingProgressBar() {
  return DownloadBubbleProgressBar{/*is_visible=*/true, /*is_looping=*/true};
}

IconAndColor IconAndColorForDownload(const DownloadUIModel& model) {
  switch (model.GetState()) {
    case download::DownloadItem::IN_PROGRESS:
    case download::DownloadItem::COMPLETE:
      return IconAndColorForInProgressOrComplete(model);
    case download::DownloadItem::INTERRUPTED: {
      const offline_items_collection::FailState fail_state =
          model.GetLastFailState();
      if (fail_state != offline_items_collection::FailState::USER_CANCELED) {
        return IconAndColorForInterrupted(model);
      }
      break;
    }
    case download::DownloadItem::CANCELLED:
      break;
    case download::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
  }

  return IconAndColorForDownloadOff();
}

DownloadBubbleQuickAction::DownloadBubbleQuickAction(
    DownloadCommands::Command command,
    const std::u16string& hover_text,
    const gfx::VectorIcon* icon)
    : command(command), hover_text(hover_text), icon(icon) {}

std::vector<DownloadBubbleQuickAction> QuickActionsForDownload(
    const DownloadUIModel& model) {
  switch (model.GetState()) {
    case DownloadItem::IN_PROGRESS:
    case DownloadItem::COMPLETE:
      break;
    case DownloadItem::INTERRUPTED:
    case DownloadItem::CANCELLED:
    case DownloadItem::MAX_DOWNLOAD_STATE:
      return {};
  }

  switch (model.GetInsecureDownloadStatus()) {
    case DownloadItem::InsecureDownloadStatus::BLOCK:
    case DownloadItem::InsecureDownloadStatus::WARN:
      return {};
    case DownloadItem::InsecureDownloadStatus::UNKNOWN:
    case DownloadItem::InsecureDownloadStatus::SAFE:
    case DownloadItem::InsecureDownloadStatus::VALIDATED:
    case DownloadItem::InsecureDownloadStatus::SILENT_BLOCK:
      break;
  }

  if (enterprise_connectors::ShouldPromptReviewForDownload(
          model.profile(), model.GetDownloadItem())) {
    switch (model.GetDangerType()) {
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
        return {};
      default:
        break;
    }
  }

  if (model.GetTailoredWarningType() !=
      TailoredWarningType::kNoTailoredWarning) {
    return {};
  }

  switch (model.GetDangerType()) {
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
      return {};
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

  std::vector<DownloadBubbleQuickAction> actions;
  if (model.GetState() == DownloadItem::IN_PROGRESS) {
    if (model.IsPaused()) {
      actions.emplace_back(
          DownloadCommands::Command::RESUME,
          l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_RESUME_QUICK_ACTION),
          &vector_icons::kPlayArrowChromeRefreshIcon);
    } else {
      actions.emplace_back(
          DownloadCommands::Command::PAUSE,
          l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_PAUSE_QUICK_ACTION),
          &vector_icons::kPauseChromeRefreshIcon);
    }

    actions.emplace_back(
        DownloadCommands::Command::CANCEL,
        l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_CANCEL_QUICK_ACTION),
        &vector_icons::kCancelChromeRefreshIcon);

  } else {
    actions.emplace_back(DownloadCommands::Command::SHOW_IN_FOLDER,
                         l10n_util::GetStringUTF16(
                             IDS_DOWNLOAD_BUBBLE_SHOW_IN_FOLDER_QUICK_ACTION),
                         &vector_icons::kFolderChromeRefreshIcon);
    actions.emplace_back(
        DownloadCommands::Command::OPEN_WHEN_COMPLETE,
        l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_OPEN_QUICK_ACTION),
        &vector_icons::kLaunchChromeRefreshIcon);
  }

  return actions;
}

DownloadBubbleProgressBar ProgressBarForDownload(const DownloadUIModel& model) {
  switch (model.GetState()) {
    case DownloadItem::IN_PROGRESS:
    case DownloadItem::COMPLETE:
      break;
    case DownloadItem::INTERRUPTED:
    case DownloadItem::CANCELLED:
    case DownloadItem::MAX_DOWNLOAD_STATE:
      return DownloadBubbleProgressBar::NoProgressBar();
  }

  switch (model.GetInsecureDownloadStatus()) {
    case DownloadItem::InsecureDownloadStatus::BLOCK:
    case DownloadItem::InsecureDownloadStatus::WARN:
      return DownloadBubbleProgressBar::NoProgressBar();
    case DownloadItem::InsecureDownloadStatus::UNKNOWN:
    case DownloadItem::InsecureDownloadStatus::SAFE:
    case DownloadItem::InsecureDownloadStatus::VALIDATED:
    case DownloadItem::InsecureDownloadStatus::SILENT_BLOCK:
      break;
  }

  if (enterprise_connectors::ShouldPromptReviewForDownload(
          model.profile(), model.GetDownloadItem())) {
    switch (model.GetDangerType()) {
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
        return DownloadBubbleProgressBar::NoProgressBar();
      default:
        break;
    }
  }

  if (model.GetTailoredWarningType() !=
      TailoredWarningType::kNoTailoredWarning) {
    return DownloadBubbleProgressBar::NoProgressBar();
  }

  switch (model.GetDangerType()) {
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
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
      return DownloadBubbleProgressBar::NoProgressBar();
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
      return DownloadBubbleProgressBar::LoopingProgressBar();
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

  return model.GetState() == DownloadItem::IN_PROGRESS
             ? DownloadBubbleProgressBar::ProgressBar()
             : DownloadBubbleProgressBar::NoProgressBar();
}
