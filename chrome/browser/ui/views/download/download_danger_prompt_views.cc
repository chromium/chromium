// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/download/download_danger_prompt.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/download/download_ui_safe_browsing_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

using safe_browsing::ClientSafeBrowsingReportRequest;

namespace {

// Views-specific implementation of download danger prompt dialog, which
// implements danger warning bypass from the downloads extension API. We use
// this class rather than a TabModalConfirmDialog so that we can use custom
// formatting on the text in the body of the dialog.
// TODO(chlily): This probably does not handle (both dangerous and) insecure
// downloads very coherently.
class DownloadDangerPromptViews : public DownloadDangerPrompt,
                                  public download::DownloadItem::Observer,
                                  public views::DialogDelegateView {
  METADATA_HEADER(DownloadDangerPromptViews, views::DialogDelegateView)

 public:
  DownloadDangerPromptViews(download::DownloadItem* item,
                            Profile* profile,
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
  OnDone done_;
};

DownloadDangerPromptViews::DownloadDangerPromptViews(
    download::DownloadItem* item,
    Profile* profile,
    OnDone done)
    : download_(item),
      profile_(profile),
      done_(std::move(done)) {
  // Note that this prompt is asking whether to cancel a dangerous download, so
  // the accept path is titled "Cancel".
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_CANCEL));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(IDS_CONFIRM_DOWNLOAD));
  SetModalType(ui::mojom::ModalType::kChild);

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
      NOTREACHED();
  }
}

// views::DialogDelegate methods:
std::u16string DownloadDangerPromptViews::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_CONFIRM_KEEP_DANGEROUS_DOWNLOAD_TITLE);
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
  std::u16string filename =
      download_->GetFileNameToReportUser().LossyDisplayName();
  switch (download_->GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return l10n_util::GetStringFUTF16(IDS_PROMPT_DANGEROUS_DOWNLOAD,
                                        filename);
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      return l10n_util::GetStringFUTF16(IDS_PROMPT_MALICIOUS_DOWNLOAD_CONTENT,
                                        filename);
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return l10n_util::GetStringFUTF16(
          safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
              profile_)
                  ->IsUnderAdvancedProtection()
              ? IDS_PROMPT_UNCOMMON_DOWNLOAD_CONTENT_IN_ADVANCED_PROTECTION
              : IDS_PROMPT_UNCOMMON_DOWNLOAD_CONTENT,
          filename);
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return l10n_util::GetStringFUTF16(IDS_PROMPT_DOWNLOAD_CHANGES_SETTINGS,
                                        filename);
    default:
      NOTREACHED();
  }
}

void DownloadDangerPromptViews::RunDone(Action action) {
  // Invoking the callback can cause the download item state to change or cause
  // the window to close, and |callback| refers to a member variable.
  OnDone done = std::move(done_);
  if (download_) {
    const bool accept = action == DownloadDangerPrompt::ACCEPT;
    // If this download is no longer dangerous, is already canceled or
    // completed, don't send any report.
    if (download_->IsDangerous() && !download_->IsDone()) {
      // Survey triggered on ACCEPT action, since this is where the user
      // confirms their choice to keep a dangerous download, rather than
      // triggering a survey after selecting to KEEP in the downloads page UI.
      if (safe_browsing::IsSafeBrowsingSurveysEnabled(*profile_->GetPrefs()) &&
          accept) {
        TrustSafetySentimentService* trust_safety_sentiment_service =
            TrustSafetySentimentServiceFactory::GetForProfile(profile_);
        if (trust_safety_sentiment_service) {
          trust_safety_sentiment_service->InteractedWithDownloadWarningUI(
              DownloadItemWarningData::WarningSurface::DOWNLOAD_PROMPT,
              DownloadItemWarningData::WarningAction::PROCEED);
        }
      }
      // Log here for "Shown" unconditionally, and for "Proceed" iff the dialog
      // was accepted. This assumes the dialog cannot be dismissed once it is
      // shown without taking some action on it.
      RecordDownloadDangerPromptHistogram("Shown", *download_);
      if (accept) {
        RecordDownloadDangerPromptHistogram("Proceed", *download_);
      }
      RecordDownloadWarningEvent(action, download_);
      // Do not send cancel report since it's not a terminal action.
      if (accept) {
        SendSafeBrowsingDownloadReport(
            ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_BY_API, accept,
            download_);
      }
    }
    download_->RemoveObserver(this);
    download_ = nullptr;
  }
  if (done)
    std::move(done).Run(action);
}

BEGIN_METADATA(DownloadDangerPromptViews)
ADD_READONLY_PROPERTY_METADATA(std::u16string, MessageBody)
END_METADATA

}  // namespace

// static
DownloadDangerPrompt* DownloadDangerPrompt::Create(
    download::DownloadItem* item,
    content::WebContents* web_contents,
    OnDone done) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DownloadDangerPromptViews* download_danger_prompt =
      new DownloadDangerPromptViews(item, profile, std::move(done));
  constrained_window::ShowWebModalDialogViews(download_danger_prompt,
                                              web_contents);
  return download_danger_prompt;
}
