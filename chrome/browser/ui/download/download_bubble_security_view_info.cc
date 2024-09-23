// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_bubble_security_view_info.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/download/download_ui_safe_browsing_util.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/offline_items_collection/core/fail_state.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

using download::DownloadItem;
using TailoredWarningType = DownloadUIModel::TailoredWarningType;
using offline_items_collection::FailState;

DownloadBubbleSecurityViewInfoObserver::
    DownloadBubbleSecurityViewInfoObserver() = default;
DownloadBubbleSecurityViewInfoObserver::
    ~DownloadBubbleSecurityViewInfoObserver() = default;

DownloadBubbleSecurityViewInfo::SubpageButton::SubpageButton(
    DownloadCommands::Command command,
    std::u16string label,
    bool is_prominent,
    std::optional<ui::ColorId> text_color)
    : command(command),
      label(label),
      is_prominent(is_prominent),
      text_color(text_color) {}

DownloadBubbleSecurityViewInfo::DownloadBubbleSecurityViewInfo() = default;
DownloadBubbleSecurityViewInfo::~DownloadBubbleSecurityViewInfo() = default;

void DownloadBubbleSecurityViewInfo::InitializeForDownload(
    DownloadUIModel& model) {
  if (model.GetContentId() != content_id_) {
    Reset();
    download_item_observation_.Observe(model.GetDownloadItem());
  }

  OnDownloadUpdated(model.GetDownloadItem());
}

void DownloadBubbleSecurityViewInfo::SetSubpageButtonsForTesting(
    std::vector<SubpageButton> buttons) {
  subpage_buttons_ = std::move(buttons);
}

bool DownloadBubbleSecurityViewInfo::HasSubpage() const {
  return !warning_summary_.empty();
}

void DownloadBubbleSecurityViewInfo::OnDownloadUpdated(
    download::DownloadItem* download) {
  ContentId content_id = OfflineItemUtils::GetContentIdForDownload(download);
  bool is_different_download = content_id != content_id_;
  bool danger_type_changed = danger_type_ != download->GetDangerType();
  if (is_different_download) {
    content_id_ = content_id;
    title_text_ = download->GetFileNameToReportUser().LossyDisplayName();
    NotifyObservers(
        &DownloadBubbleSecurityViewInfoObserver::OnContentIdChanged);
  }

  if (is_different_download || danger_type_changed) {
    danger_type_ = download->GetDangerType();
    PopulateForDownload(download);
    NotifyObservers(&DownloadBubbleSecurityViewInfoObserver::OnInfoChanged);
  }
}

void DownloadBubbleSecurityViewInfo::OnDownloadRemoved(
    download::DownloadItem* download) {
  CHECK(content_id_ == OfflineItemUtils::GetContentIdForDownload(download));
  Reset();
}

void DownloadBubbleSecurityViewInfo::Reset() {
  content_id_.reset();
  title_text_ = std::u16string();
  download_item_observation_.Reset();
}

void DownloadBubbleSecurityViewInfo::ClearForUpdate() {
  // `title_text_` cannot change when a download is updated, so we do
  // not need to clear it.
  warning_summary_.clear();
  warning_secondary_text_.clear();
  warning_secondary_icon_ = nullptr;
  learn_more_link_ = std::nullopt;
  subpage_buttons_.clear();
  has_progress_bar_ = false;
  is_progress_bar_looping_ = false;
}

void DownloadBubbleSecurityViewInfo::PopulateForDownload(
    download::DownloadItem* download) {
  ClearForUpdate();

  DownloadItemModel model(
      download, std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>());
  icon_and_color_ = IconAndColorForDownload(model);

  switch (model.GetState()) {
    case download::DownloadItem::IN_PROGRESS:
    case download::DownloadItem::COMPLETE:
      PopulateForInProgressOrComplete(model);
      break;
    case download::DownloadItem::INTERRUPTED:
      if (model.GetLastFailState() !=
          offline_items_collection::FailState::USER_CANCELED) {
        PopulateForInterrupted(model);
      }
      break;
    case download::DownloadItem::CANCELLED:
      break;
    case download::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED_IN_MIGRATION();
  }
}

void DownloadBubbleSecurityViewInfo::PopulateForDangerousUi(
    const std::u16string& subpage_summary) {
  warning_summary_ = subpage_summary;
  PopulateLearnMoreLink(
      l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_BLOCKED_LEARN_MORE_LINK),
      DownloadCommands::Command::LEARN_MORE_DOWNLOAD_BLOCKED);
  PopulatePrimarySubpageButton(
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_DELETE_FROM_HISTORY),
      DownloadCommands::Command::DISCARD);
}

void DownloadBubbleSecurityViewInfo::PopulateForSuspiciousUi(
    const std::u16string& subpage_summary,
    const std::u16string& secondary_subpage_button_label) {
  warning_summary_ = subpage_summary;
  PopulateLearnMoreLink(
      l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_BLOCKED_LEARN_MORE_LINK),
      DownloadCommands::Command::LEARN_MORE_DOWNLOAD_BLOCKED);
  PopulatePrimarySubpageButton(
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_DELETE_FROM_HISTORY),
      DownloadCommands::Command::DISCARD);
  PopulateSecondarySubpageButton(secondary_subpage_button_label,
                                 DownloadCommands::Command::KEEP);
}

void DownloadBubbleSecurityViewInfo::PopulateForInterrupted(
    const DownloadUIModel& model) {
  // Only handle danger types that are terminated in the interrupted state in
  // this function. The other danger types are handled in
  // `PopulateForInProgressOrComplete`.
  switch (model.GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
      warning_summary_ = l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_ENCRYPTED);
      return;
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
      warning_summary_ = l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_TOO_BIG);
      return;
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK: {
      if (!enterprise_connectors::ShouldPromptReviewForDownload(
              model.profile(), model.GetDownloadItem())) {
        warning_summary_ = l10n_util::GetStringUTF16(
            IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_SENSITIVE_CONTENT_BLOCK);
      }
      return;
    }
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED: {
      warning_summary_ = l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_SCAN_FAILED);
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
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      // Fall through to the failed UX
      // TODO(drubery): Not all of these danger types can occur with a
      // fail state. Investigate which can and cannot, and fall
      // through less frequently.
      break;
  }

  switch (model.GetLastFailState()) {
    case FailState::FILE_BLOCKED:
      warning_summary_ = l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_INTERRUPTED_SUBPAGE_SUMMARY_BLOCKED_ORGANIZATION);
      return;
    case FailState::FILE_NAME_TOO_LONG:
      warning_summary_ = l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_INTERRUPTED_SUBPAGE_SUMMARY_PATH_TOO_LONG);
      return;
    case FailState::FILE_NO_SPACE:
      warning_summary_ = l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_INTERRUPTED_SUBPAGE_SUMMARY_DISK_FULL);
      return;
    case FailState::SERVER_UNAUTHORIZED:
      warning_summary_ = l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_INTERRUPTED_SUBPAGE_SUMMARY_FILE_UNAVAILABLE);
      return;
    // No Retry in these cases.
    case FailState::FILE_TOO_LARGE:
    case FailState::FILE_VIRUS_INFECTED:
    case FailState::FILE_SECURITY_CHECK_FAILED:
    case FailState::FILE_ACCESS_DENIED:
    case FailState::SERVER_FORBIDDEN:
    case FailState::FILE_SAME_AS_SOURCE:
    case FailState::SERVER_BAD_CONTENT:
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
      return;
    // Not possible because the USER_CANCELED fail state does not allow a call
    // into this function
    case FailState::USER_CANCELED:
    // Deprecated
    case FailState::NETWORK_INSTABILITY:
    case FailState::CANNOT_DOWNLOAD:
      NOTREACHED_IN_MIGRATION();
      return;
    case FailState::NO_FAILURE:
      return;
  }
}

void DownloadBubbleSecurityViewInfo::PopulateForInProgressOrComplete(
    const DownloadUIModel& model) {
  switch (model.GetInsecureDownloadStatus()) {
    case download::DownloadItem::InsecureDownloadStatus::BLOCK:
    case download::DownloadItem::InsecureDownloadStatus::WARN:
      PopulateForSuspiciousUi(
          l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_INSECURE),
          l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_CONTINUE_INSECURE_FILE));
      return;
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
      case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
        return;
      default:
        break;
    }
  }

  if (TailoredWarningType type = model.GetTailoredWarningType();
      type != TailoredWarningType::kNoTailoredWarning) {
    return PopulateForTailoredWarning(model);
  }

  switch (model.GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      if (model.IsExtensionDownload()) {
        PopulateForSuspiciousUi(
            l10n_util::GetStringFUTF16(
                IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_UNKNOWN_SOURCE,
                l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE)),
            l10n_util::GetStringUTF16(
                IDS_DOWNLOAD_BUBBLE_CONTINUE_SUSPICIOUS_FILE));
        return;
      }
      if (WasSafeBrowsingVerdictObtained(model.GetDownloadItem())) {
        PopulateForSuspiciousUi(
            l10n_util::GetStringUTF16(
                IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_DANGEROUS_FILE_TYPE),
            l10n_util::GetStringUTF16(
                IDS_DOWNLOAD_BUBBLE_CONTINUE_SUSPICIOUS_FILE));
        return;
      }
      if (ShouldShowWarningForNoSafeBrowsing(model.profile())) {
        PopulateForFileTypeWarningNoSafeBrowsing(model);
        return;
      }
      PopulateForSuspiciousUi(
          l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_DANGEROUS_FILE_TYPE),
          l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_CONTINUE_UNVERIFIED_FILE));
      return;

    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      PopulateForDangerousUi(l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_DANGEROUS));
      return;

    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      PopulateForDangerousUi(l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_DECEPTIVE));
      return;

    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT: {
      bool request_ap_verdicts = false;
#if BUILDFLAG(FULL_SAFE_BROWSING)
      request_ap_verdicts =
          safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
              model.profile())
              ->IsUnderAdvancedProtection();
#endif
      if (request_ap_verdicts) {
        warning_summary_ = l10n_util::GetStringUTF16(
            IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_ADVANCED_PROTECTION);
        PopulatePrimarySubpageButton(
            l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_DELETE),
            DownloadCommands::Command::DISCARD);
        PopulateSecondarySubpageButton(
            l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_CONTINUE),
            DownloadCommands::Command::KEEP, kColorDownloadItemTextWarning);
        return;
      }
      PopulateForSuspiciousUi(
          l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_UNCOMMON_FILE),
          l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_CONTINUE_SUSPICIOUS_FILE));
      return;
    }
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
      warning_summary_ = l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_SENSITIVE_CONTENT_WARNING);
      PopulatePrimarySubpageButton(
          l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_DELETE),
          DownloadCommands::Command::DISCARD);
      PopulateSecondarySubpageButton(
          l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_CONTINUE),
          DownloadCommands::Command::KEEP, kColorDownloadItemTextWarning);
      return;
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING: {
      warning_summary_ = l10n_util::GetStringFUTF16(
          model.IsTopLevelEncryptedArchive()
              ? IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_DEEP_SCANNING_PROMPT_ENCRYPTED_ARCHIVE
              : IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_DEEP_SCANNING_PROMPT_UPDATED,
          u"\n\n");
      PopulatePrimarySubpageButton(
          l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_SCAN_UPDATED),
          DownloadCommands::Command::DEEP_SCAN);
      PopulateSecondarySubpageButton(
          l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_OPEN_UPDATED),
          DownloadCommands::Command::BYPASS_DEEP_SCANNING);
      PopulateLearnMoreLink(l10n_util::GetStringUTF16(
                                IDS_DOWNLOAD_BUBBLE_SUBPAGE_DEEP_SCANNING_LINK),
                            DownloadCommands::LEARN_MORE_SCANNING);
      return;
    }
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING: {
      warning_summary_ = l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_DEEP_SCANNING_PROMPT_LOCAL_DECRYPTION,
          u"\n\n");
      PopulatePrimarySubpageButton(
          l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_ACCEPT_LOCAL_DECRYPTION),
          // These download commands are not propagated
          // to the DownloadItem. Instead they are handled specially in
          // DownloadBubbleSecurityView::ProcessButtonClick. That makes it
          // okay that the we aren't really prompting for a deep scan.
          // TODO(crbug.com/40931768): Remove this by creating a dedicated View
          // for the local decryption prompt which directly handles the
          // button presses.
          DownloadCommands::Command::DEEP_SCAN);
      PopulateSecondarySubpageButton(
          l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_BYPASS_LOCAL_DECRYPTION),
          DownloadCommands::Command::KEEP);
      PopulateLearnMoreLink(
          l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_BLOCKED_LEARN_MORE_LINK),
          DownloadCommands::LEARN_MORE_SCANNING);
      return;
    }
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
      has_progress_bar_ = true;
      is_progress_bar_looping_ = true;
      if (DownloadItemWarningData::DownloadDeepScanTrigger(
              model.GetDownloadItem()) ==
          DownloadItemWarningData::DeepScanTrigger::
              TRIGGER_IMMEDIATE_DEEP_SCAN) {
        warning_summary_ = l10n_util::GetStringFUTF16(
            IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_IMMEDIATE_DEEP_SCAN_IN_PROGRESS,
            u"\n\n");
        PopulatePrimarySubpageButton(
            l10n_util::GetStringUTF16(
                IDS_DOWNLOAD_BUBBLE_SUBPAGE_IMMEDIATE_DEEP_SCAN_CANCEL),
            DownloadCommands::Command::DISCARD);
        PopulateSecondarySubpageButton(
            l10n_util::GetStringUTF16(
                IDS_DOWNLOAD_BUBBLE_SUBPAGE_IMMEDIATE_DEEP_SCAN_BYPASS),
            DownloadCommands::Command::BYPASS_DEEP_SCANNING);
        PopulateLearnMoreLink(
            l10n_util::GetStringUTF16(
                IDS_DOWNLOAD_BUBBLE_SUBPAGE_DEEP_SCANNING_LINK),
            DownloadCommands::LEARN_MORE_SCANNING);
      } else {
        warning_summary_ = l10n_util::GetStringUTF16(
            IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_ASYNC_SCANNING);
        warning_secondary_icon_ = &vector_icons::kDocumentScannerIcon;
        warning_secondary_text_ =
            download::DoesDownloadConnectorBlock(model.profile(),
                                                 model.GetURL())
                ? l10n_util::GetStringUTF16(
                      IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_ASYNC_SCANNING_ENTERPRISE_SECONDARY)
                : l10n_util::GetStringUTF16(
                      IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_ASYNC_SCANNING_SECONDARY);
        PopulatePrimarySubpageButton(
            l10n_util::GetStringUTF16(
                IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_ASYNC_SCANNING_DISCARD),
            DownloadCommands::Command::DISCARD,
            /*is_prominent=*/false);
        if (!download::DoesDownloadConnectorBlock(model.profile(),
                                                  model.GetURL())) {
          PopulateSecondarySubpageButton(
              l10n_util::GetStringUTF16(
                  IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_ASYNC_SCANNING_CANCEL),
              DownloadCommands::Command::CANCEL_DEEP_SCAN);
        }
      }
      return;
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING: {
      warning_summary_ = l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_LOCAL_DECRYPTION_IN_PROGRESS,
          u"\n\n");
      has_progress_bar_ = true;
      is_progress_bar_looping_ = true;
      // These download commands are not propagated
      // to the DownloadItem. Instead they are handled specially
      // in DownloadBubbleSecurityView::ProcessButtonClick. That
      // means the semantics don't have to line up with the actual
      // behavior of the download command.
      // TODO(crbug.com/40931768): Remove this by creating a dedicated
      // View for the local decryption prompt which directly
      // handles the button presses.
      PopulatePrimarySubpageButton(
          l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_LOCAL_DECRYPTION_CANCEL),
          DownloadCommands::Command::CANCEL);
      PopulateSecondarySubpageButton(
          l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_BYPASS_LOCAL_DECRYPTION),
          DownloadCommands::Command::BYPASS_DEEP_SCANNING);
      return;
    }
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
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
      return;
  }
}

void DownloadBubbleSecurityViewInfo::PopulateForTailoredWarning(
    const DownloadUIModel& model) {
  switch (model.GetTailoredWarningType()) {
    case TailoredWarningType::kSuspiciousArchive:
      PopulateForSuspiciousUi(
          l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_ARCHIVE_MALWARE),
          l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_CONTINUE_SUSPICIOUS_FILE));
      return;
    case TailoredWarningType::kCookieTheft:
      PopulateForDangerousUi(l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_COOKIE_THEFT));
      return;
    case TailoredWarningType::kCookieTheftWithAccountInfo: {
      std::string email;
      if (auto* identity_manager =
              IdentityManagerFactory::GetForProfile(model.profile());
          identity_manager) {
        email = identity_manager
                    ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                    .email;
      }
      base::UmaHistogramBoolean(
          "SBClientDownload.TailoredWarning.HasVaidEmailForAccountInfo",
          !email.empty());
      if (!email.empty()) {
        PopulateForDangerousUi(l10n_util::GetStringFUTF16(
            IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_COOKIE_THEFT_AND_ACCOUNT,
            base::ASCIIToUTF16(email)));
        return;
      }
      PopulateForDangerousUi(l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_COOKIE_THEFT));
      return;
    }
    case TailoredWarningType::kNoTailoredWarning: {
      NOTREACHED_IN_MIGRATION();
    }
  }
}

void DownloadBubbleSecurityViewInfo::PopulateForFileTypeWarningNoSafeBrowsing(
    const DownloadUIModel& model) {
  PopulateForSuspiciousUi(
      l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_NO_SAFE_BROWSING),
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_CONTINUE_UNVERIFIED_FILE));
  // Clear the "Learn why Chrome..." link. If the user is not capable of turning
  // on SB, do not show the default link and label.
  learn_more_link_ = std::nullopt;
  if (CanUserTurnOnSafeBrowsing(model.profile())) {
    PopulateLearnMoreLink(
        IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_SAFE_BROWSING_SETTING_LABEL,
        IDS_DOWNLOAD_BUBBLE_SUBPAGE_SUMMARY_WARNING_SAFE_BROWSING_SETTING_LINK,
        DownloadCommands::Command::OPEN_SAFE_BROWSING_SETTING);
  }
}

void DownloadBubbleSecurityViewInfo::PopulateLearnMoreLink(
    const std::u16string& link_text,
    DownloadCommands::Command command) {
  learn_more_link_ = LabelWithLink{
      link_text, LabelWithLink::LinkedRange{0u, link_text.length(), command}};
}

void DownloadBubbleSecurityViewInfo::PopulateLearnMoreLink(
    int label_text_id,
    int link_text_id,
    DownloadCommands::Command command) {
  size_t link_start_offset = 0;
  std::u16string link_text = l10n_util::GetStringUTF16(link_text_id);
  std::u16string label_and_link_text =
      l10n_util::GetStringFUTF16(label_text_id, link_text, &link_start_offset);
  learn_more_link_ = LabelWithLink{
      label_and_link_text, LabelWithLink::LinkedRange{
                               link_start_offset, link_text.length(), command}};
}

void DownloadBubbleSecurityViewInfo::PopulatePrimarySubpageButton(
    const std::u16string& label,
    DownloadCommands::Command command,
    bool is_prominent) {
  CHECK(subpage_buttons_.empty());
  subpage_buttons_.emplace_back(command, label, is_prominent);
}

void DownloadBubbleSecurityViewInfo::PopulateSecondarySubpageButton(
    const std::u16string& label,
    DownloadCommands::Command command,
    std::optional<ui::ColorId> color) {
  CHECK(subpage_buttons_.size() == 1);
  subpage_buttons_.emplace_back(command, label, /*is_prominent=*/false, color);
}
