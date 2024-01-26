// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_bubble_info_utils.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/download/download_ui_safe_browsing_util.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/views/vector_icons.h"

namespace {

IconAndColor IconAndColorForDangerousUiPattern() {
  return IconAndColor{features::IsChromeRefresh2023()
                          ? &vector_icons::kDangerousChromeRefreshIcon
                          : &vector_icons::kDangerousIcon,
                      kColorDownloadItemIconDangerous};
}

IconAndColor IconAndColorForSuspiciousUiPattern() {
  return IconAndColor{features::IsChromeRefresh2023()
                          ? &kDownloadWarningIcon
                          : &vector_icons::kNotSecureWarningIcon,
                      kColorDownloadItemIconWarning};
}

IconAndColor IconAndColorForDownloadOff() {
  return IconAndColor{features::IsChromeRefresh2023()
                          ? &vector_icons::kFileDownloadOffChromeRefreshIcon
                          : &vector_icons::kFileDownloadOffIcon,
                      ui::kColorSecondaryForeground};
}

IconAndColor IconAndColorForInterrupted(const DownloadUIModel& model) {
  // Only handle danger types that are terminated in the interrupted state in
  // this function. The other danger types are handled in
  // `IconAndColorForInProgressOrComplete`.
  switch (model.GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
      return IconAndColor{features::IsChromeRefresh2023()
                              ? &views::kInfoChromeRefreshIcon
                              : &views::kInfoIcon,
                          kColorDownloadItemIconDangerous};
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK: {
      if (enterprise_connectors::ShouldPromptReviewForDownload(
              model.profile(), model.GetDangerType())) {
        return IconAndColor{features::IsChromeRefresh2023()
                                ? &kDownloadWarningIcon
                                : &vector_icons::kNotSecureWarningIcon,
                            kColorDownloadItemIconDangerous};
      } else {
        return IconAndColor{features::IsChromeRefresh2023()
                                ? &views::kInfoChromeRefreshIcon
                                : &views::kInfoIcon,
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

  if (model.GetLastFailState() ==
      offline_items_collection::FailState::FILE_BLOCKED) {
    return IconAndColor{features::IsChromeRefresh2023()
                            ? &views::kInfoChromeRefreshIcon
                            : &views::kInfoIcon,
                        kColorDownloadItemIconDangerous};
  }

  return IconAndColor{features::IsChromeRefresh2023()
                          ? &vector_icons::kFileDownloadOffChromeRefreshIcon
                          : &vector_icons::kFileDownloadOffIcon,
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
      NOTREACHED();
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
          model.profile(), model.GetDangerType())) {
    switch (model.GetDangerType()) {
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
        return IconAndColor{features::IsChromeRefresh2023()
                                ? &vector_icons::kDangerousChromeRefreshIcon
                                : &vector_icons::kDangerousIcon,
                            kColorDownloadItemIconDangerous};
      case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
        return IconAndColor{features::IsChromeRefresh2023()
                                ? &kDownloadWarningIcon
                                : &vector_icons::kNotSecureWarningIcon,
                            kColorDownloadItemIconWarning};
      case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
        return IconAndColor{features::IsChromeRefresh2023()
                                ? &views::kInfoChromeRefreshIcon
                                : &views::kInfoIcon,
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
      return IconAndColorForSuspiciousUiPattern();

    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return IconAndColorForDangerousUiPattern();

    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT: {
      bool request_ap_verdicts = false;
#if BUILDFLAG(FULL_SAFE_BROWSING)
      request_ap_verdicts =
          safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
              model.profile())
              ->IsUnderAdvancedProtection();
#endif
      if (request_ap_verdicts) {
        return IconAndColor{features::IsChromeRefresh2023()
                                ? &kDownloadWarningIcon
                                : &vector_icons::kNotSecureWarningIcon,
                            kColorDownloadItemIconWarning};
      }
      return IconAndColorForSuspiciousUiPattern();
    }
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
      return IconAndColor{features::IsChromeRefresh2023()
                              ? &views::kInfoChromeRefreshIcon
                              : &views::kInfoIcon,
                          kColorDownloadItemIconWarning};
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
      return IconAndColor{features::IsChromeRefresh2023()
                              ? &kDownloadWarningIcon
                              : &vector_icons::kNotSecureWarningIcon,
                          kColorDownloadItemIconWarning};
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

  return IconAndColor{};
}

}  // namespace

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
      NOTREACHED_NORETURN();
  }

  return IconAndColorForDownloadOff();
}
