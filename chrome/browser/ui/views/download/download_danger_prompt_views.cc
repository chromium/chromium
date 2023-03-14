// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/download/download_danger_prompt.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

using safe_browsing::ClientSafeBrowsingReportRequest;

namespace {

// Views-specific implementation of download danger prompt dialog. We use this
// class rather than a TabModalConfirmDialog so that we can use custom
// formatting on the text in the body of the dialog.
class DownloadDangerPromptViews : public DownloadDangerPrompt,
                                  public download::DownloadItem::Observer,
                                  public views::DialogDelegateView {
 public:
  METADATA_HEADER(DownloadDangerPromptViews);
  DownloadDangerPromptViews(download::DownloadItem* item,
                            Profile* profile,
                            bool show_context,
                            OnDone done);
  ~DownloadDangerPromptViews() override;

  // DownloadDangerPrompt:
  void InvokeActionForTesting(Action action) override;

  // views::DialogDelegateView:
  std::u16string GetWindowTitle() const override;

  // download::DownloadItem::Observer:
  void OnDownloadUpdated(download::DownloadItem* download) override;

 private:
  std::u16string GetMessageBody() const;
  void RunDone(Action action);

  raw_ptr<download::DownloadItem> download_;
  raw_ptr<Profile> profile_;
  // If show_context_ is true, this is a download confirmation dialog by
  // download API, otherwise it is download recovery dialog from a regular
  // download.
  const bool show_context_;
  OnDone done_;
};

DownloadDangerPromptViews::DownloadDangerPromptViews(
    download::DownloadItem* item,
    Profile* profile,
    bool show_context,
    OnDone done)
    : download_(item),
      profile_(profile),
      show_context_(show_context),
      done_(std::move(done)) {
  // Note that this prompt is asking whether to cancel a dangerous download, so
  // the accept path is titled "Cancel".
  SetButtonLabel(ui::DIALOG_BUTTON_OK, l10n_util::GetStringUTF16(IDS_CANCEL));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 show_context_
                     ? l10n_util::GetStringUTF16(IDS_CONFIRM_DOWNLOAD)
                     : l10n_util::GetStringUTF16(IDS_CONFIRM_DOWNLOAD_AGAIN));
  SetModalType(ui::MODAL_TYPE_CHILD);

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  auto make_done_callback = [&](DownloadDangerPrompt::Action action) {
    return base::BindOnce(&DownloadDangerPromptViews::RunDone,
                          base::Unretained(this), action);
  };

  // Note that the presentational concept of "Accept/Cancel" is inverted from
  // the model's concept of ACCEPT/CANCEL. In the UI, the safe path is "Accept"
  // and the dangerous path is "Cancel".
  SetAcceptCallback(make_done_callback(CANCEL));
  SetCancelCallback(make_done_callback(ACCEPT));
  SetCloseCallback(make_done_callback(DISMISS));

  download_->AddObserver(this);

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  SetUseDefaultFillLayout(true);

  auto message_body_label = std::make_unique<views::Label>(GetMessageBody());
  message_body_label->SetMultiLine(true);
  message_body_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_body_label->SetAllowCharacterBreak(true);

  AddChildView(std::move(message_body_label));
}

DownloadDangerPromptViews::~DownloadDangerPromptViews() {
  if (download_)
    download_->RemoveObserver(this);
}

// DownloadDangerPrompt methods:
void DownloadDangerPromptViews::InvokeActionForTesting(Action action) {
  switch (action) {
    case ACCEPT:
      // This inversion is intentional.
      Cancel();
      break;

    case DISMISS:
      Close();
      break;

    case CANCEL:
      Accept();
      break;

    default:
      NOTREACHED_NORETURN();
  }
}

// views::DialogDelegate methods:
std::u16string DownloadDangerPromptViews::GetWindowTitle() const {
  if (show_context_ || !download_)  // |download_| may be null in tests.
    return l10n_util::GetStringUTF16(IDS_CONFIRM_KEEP_DANGEROUS_DOWNLOAD_TITLE);
  switch (download_->GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return l10n_util::GetStringUTF16(IDS_KEEP_DANGEROUS_DOWNLOAD_TITLE);
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return l10n_util::GetStringUTF16(IDS_KEEP_UNCOMMON_DOWNLOAD_TITLE);
    default: {
      return l10n_util::GetStringUTF16(
          IDS_CONFIRM_KEEP_DANGEROUS_DOWNLOAD_TITLE);
    }
  }
}

// download::DownloadItem::Observer:
void DownloadDangerPromptViews::OnDownloadUpdated(
    download::DownloadItem* download) {
  // If the download is nolonger dangerous (accepted externally) or the download
  // is in a terminal state, then the download danger prompt is no longer
  // necessary.
  if (!download_->IsDangerous() || download_->IsDone()) {
    RunDone(DISMISS);
    Cancel();
  }
}

std::u16string DownloadDangerPromptViews::GetMessageBody() const {
  if (show_context_) {
    switch (download_->GetDangerType()) {
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE: {
        return l10n_util::GetStringFUTF16(
            IDS_PROMPT_DANGEROUS_DOWNLOAD,
            download_->GetFileNameToReportUser().LossyDisplayName());
      }
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:  // Fall through
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST: {
        return l10n_util::GetStringFUTF16(
            IDS_PROMPT_MALICIOUS_DOWNLOAD_CONTENT,
            download_->GetFileNameToReportUser().LossyDisplayName());
      }
      case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT: {
        if (safe_browsing::AdvancedProtectionStatusManagerFactory::
                GetForProfile(profile_)
                    ->IsUnderAdvancedProtection()) {
          return l10n_util::GetStringFUTF16(
              IDS_PROMPT_UNCOMMON_DOWNLOAD_CONTENT_IN_ADVANCED_PROTECTION,
              download_->GetFileNameToReportUser().LossyDisplayName());
        } else {
          return l10n_util::GetStringFUTF16(
              IDS_PROMPT_UNCOMMON_DOWNLOAD_CONTENT,
              download_->GetFileNameToReportUser().LossyDisplayName());
        }
      }
      case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED: {
        return l10n_util::GetStringFUTF16(
            IDS_PROMPT_DOWNLOAD_CHANGES_SETTINGS,
            download_->GetFileNameToReportUser().LossyDisplayName());
      }
      case download::DOWNLOAD_DANGER_TYPE_BLOCKED_UNSUPPORTED_FILETYPE:
      case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
      case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
      case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
      case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
      case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
      case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
      case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
      case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
      case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
      case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
      case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
      case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
      case download::DOWNLOAD_DANGER_TYPE_MAX: {
        break;
      }
    }
  } else {
    // If we're insecurely downloading, show a warning first.
    if (download_->IsInsecure()) {
      return l10n_util::GetStringFUTF16(
          IDS_PROMPT_CONFIRM_INSECURE_DOWNLOAD,
          download_->GetFileNameToReportUser().LossyDisplayName());
    }
    switch (download_->GetDangerType()) {
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
      case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT: {
        return l10n_util::GetStringUTF16(
            IDS_PROMPT_CONFIRM_KEEP_MALICIOUS_DOWNLOAD_BODY);
      }
      default: {
        return l10n_util::GetStringUTF16(
            IDS_PROMPT_CONFIRM_KEEP_DANGEROUS_DOWNLOAD);
      }
    }
  }
  NOTREACHED_NORETURN();
}

void DownloadDangerPromptViews::RunDone(Action action) {
  // Invoking the callback can cause the download item state to change or cause
  // the window to close, and |callback| refers to a member variable.
  OnDone done = std::move(done_);
  if (download_) {
    // If this download is no longer dangerous, is already canceled or
    // completed, don't send any report.
    if (download_->IsDangerous() && !download_->IsDone()) {
      const bool accept = action == DownloadDangerPrompt::ACCEPT;
      RecordDownloadDangerPrompt(accept, *download_);
      RecordDownloadWarningEvent(action, download_);
      if (!download_->GetURL().is_empty() &&
          !content::DownloadItemUtils::GetBrowserContext(download_)
               ->IsOffTheRecord()) {
        ClientSafeBrowsingReportRequest::ReportType report_type =
            show_context_
                ? ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_BY_API
                : ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_RECOVERY;
        // Do not send cancel report under the new trigger condition since it's
        // not a terminal action.
        if (!base::FeatureList::IsEnabled(
                safe_browsing::kSafeBrowsingCsbrrNewDownloadTrigger) ||
            accept) {
          SendSafeBrowsingDownloadReport(report_type, accept, download_);
        }
      }
    }
    download_->RemoveObserver(this);
    download_ = nullptr;
  }
  if (done)
    std::move(done).Run(action);
}

BEGIN_METADATA(DownloadDangerPromptViews, views::DialogDelegateView)
ADD_READONLY_PROPERTY_METADATA(std::u16string, MessageBody)
END_METADATA

}  // namespace

// static
DownloadDangerPrompt* DownloadDangerPrompt::Create(
    download::DownloadItem* item,
    content::WebContents* web_contents,
    bool show_context,
    OnDone done) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DownloadDangerPromptViews* download_danger_prompt =
      new DownloadDangerPromptViews(item, profile, show_context,
                                    std::move(done));
  constrained_window::ShowWebModalDialogViews(download_danger_prompt,
                                              web_contents);
  return download_danger_prompt;
}
