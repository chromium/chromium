// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/download/download_danger_prompt.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/ui/extensions/extension_dialog_utils.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_item.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace {

// A delegate for the danger prompt dialog shown when an extension tries to
// download a dangerous file. It observes the download item and dismisses the
// dialog if the download is no longer relevant.
class DownloadDangerDialogDelegate : public ui::DialogModelDelegate,
                                     public download::DownloadItem::Observer {
 public:
  DownloadDangerDialogDelegate(
      download::DownloadItem* download,
      base::OnceCallback<void(DownloadDangerPrompt::Action)> callback);
  DownloadDangerDialogDelegate(const DownloadDangerDialogDelegate&) = delete;
  const DownloadDangerDialogDelegate& operator=(
      const DownloadDangerDialogDelegate&) = delete;
  ~DownloadDangerDialogDelegate() override;

  std::u16string GetMessageBody(Profile* profile) const;
  void DismissDialog();

  void OnDialogAccepted();
  void OnDialogCanceled();
  void OnDialogClosed();

  // download::DownloadItem::Observer:
  void OnDownloadUpdated(download::DownloadItem* download) override;
  void OnDownloadRemoved(download::DownloadItem* download) override;
  void OnDownloadDestroyed(download::DownloadItem* download) override;

 private:
  // Stops observing the download item and clears the pointer. This is called
  // when the dialog is being closed or the download is no longer relevant,
  // ensuring no further observer notifications are processed.
  void DetachFromDownload();

  raw_ptr<download::DownloadItem> download_;
  base::OnceCallback<void(DownloadDangerPrompt::Action)> callback_;
};

DownloadDangerDialogDelegate::DownloadDangerDialogDelegate(
    download::DownloadItem* download,
    base::OnceCallback<void(DownloadDangerPrompt::Action)> callback)
    : download_(download), callback_(std::move(callback)) {
  if (download_) {
    download_->AddObserver(this);
  }
}

DownloadDangerDialogDelegate::~DownloadDangerDialogDelegate() {
  DetachFromDownload();
}

std::u16string DownloadDangerDialogDelegate::GetMessageBody(
    Profile* profile) const {
  if (!download_) {
    return std::u16string();
  }

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
              profile)
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

void DownloadDangerDialogDelegate::DismissDialog() {
  DetachFromDownload();
  if (callback_) {
    std::move(callback_).Run(DownloadDangerPrompt::Action::DISMISS);
  }
  dialog_model()->host()->Close();
}

void DownloadDangerDialogDelegate::OnDialogAccepted() {
  DetachFromDownload();
  // Accepting the dialog (the "OK" button) corresponds to the "Cancel" action
  // for the download.
  std::move(callback_).Run(DownloadDangerPrompt::Action::CANCEL);
}

void DownloadDangerDialogDelegate::OnDialogCanceled() {
  DetachFromDownload();
  // Canceling the dialog (the "Cancel" button) corresponds to the "Accept"
  // action for the download.
  std::move(callback_).Run(DownloadDangerPrompt::Action::ACCEPT);
}

void DownloadDangerDialogDelegate::OnDialogClosed() {
  if (callback_) {
    std::move(callback_).Run(DownloadDangerPrompt::Action::DISMISS);
  }
}

void DownloadDangerDialogDelegate::OnDownloadUpdated(
    download::DownloadItem* download) {
  if (!download_ || !download->IsDangerous() || download->IsDone()) {
    DismissDialog();
  }
}

void DownloadDangerDialogDelegate::OnDownloadRemoved(
    download::DownloadItem* download) {
  DismissDialog();
}

void DownloadDangerDialogDelegate::OnDownloadDestroyed(
    download::DownloadItem* download) {
  DismissDialog();
}

void DownloadDangerDialogDelegate::DetachFromDownload() {
  if (download_) {
    download_->RemoveObserver(this);
    download_ = nullptr;
  }
}

}  // namespace

namespace extensions {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kDownloadDangerDialogCancelButtonElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kDownloadDangerDialogKeepButtonElementId);

void ShowDownloadDangerDialog(
    download::DownloadItem* download_item,
    content::WebContents* web_contents,
    base::OnceCallback<void(DownloadDangerPrompt::Action)> done_callback) {
  auto dialog_delegate_unique = std::make_unique<DownloadDangerDialogDelegate>(
      download_item, std::move(done_callback));
  DownloadDangerDialogDelegate* dialog_delegate = dialog_delegate_unique.get();

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  std::unique_ptr<ui::DialogModel> dialog =
      ui::DialogModel::Builder(std::move(dialog_delegate_unique))
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_CONFIRM_KEEP_DANGEROUS_DOWNLOAD_TITLE))
          // The "OK" button is the default action for the dialog. We use it for
          // "Cancel" so that it is the default focused button, encouraging the
          // safer choice.
          .AddOkButton(
              base::BindOnce(&DownloadDangerDialogDelegate::OnDialogAccepted,
                             base::Unretained(dialog_delegate)),
              ui::DialogModel::Button::Params()
                  .SetLabel(l10n_util::GetStringUTF16(IDS_CANCEL))
                  .SetId(kDownloadDangerDialogCancelButtonElementId))
          // The "Cancel" button is used for the "Keep" action so that it is
          // not the default focused choice, reducing the risk of accidental
          // acceptance.
          .AddCancelButton(
              base::BindOnce(&DownloadDangerDialogDelegate::OnDialogCanceled,
                             base::Unretained(dialog_delegate)),
              ui::DialogModel::Button::Params()
                  .SetLabel(l10n_util::GetStringUTF16(IDS_CONFIRM_DOWNLOAD))
                  .SetId(kDownloadDangerDialogKeepButtonElementId))
          .SetCloseActionCallback(
              base::BindOnce(&DownloadDangerDialogDelegate::OnDialogClosed,
                             base::Unretained(dialog_delegate)))
          .SetDialogDestroyingCallback(
              base::BindOnce(&DownloadDangerDialogDelegate::OnDialogClosed,
                             base::Unretained(dialog_delegate)))
          .OverrideShowCloseButton(/*show_close_button=*/false)
          .AddParagraph(
              ui::DialogModelLabel(dialog_delegate->GetMessageBody(profile)))
          .Build();

  ShowWebModalDialog(web_contents, std::move(dialog));
}

}  // namespace extensions
