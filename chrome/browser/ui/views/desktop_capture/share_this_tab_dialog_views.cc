// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/share_this_tab_dialog_views.h"

#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window_tree_host.h"
#endif

using content::BrowserThread;
using content::DesktopMediaID;

ShareThisTabDialogView::ShareThisTabDialogView(
    const DesktopMediaPicker::Params& params,
    ShareThisTabDialogViews* parent)
    : web_contents_(params.web_contents->GetWeakPtr()),
      app_name_(params.app_name),
      parent_(parent) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SetModalType(params.modality);
  RegisterDeleteDelegateCallback(base::BindOnce(
      [](ShareThisTabDialogView* dialog) {
        // If the dialog is being closed then notify the parent about it.
        if (dialog->parent_) {
          dialog->parent_->NotifyDialogResult(DesktopMediaID());
        }
      },
      this));

  const ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kControl),
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_VERTICAL_SMALL)));

  auto description_label = std::make_unique<views::Label>();
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description_label_ = AddChildView(std::move(description_label));
  description_label_->SetText(
      l10n_util::GetStringUTF16(IDS_SHARE_THIS_TAB_DIALOG_TEXT));

  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_SHARE_THIS_TAB_DIALOG_ALLOW));

  // If |params.web_contents| is set and it's not a background page then the
  // picker will be shown modal to the web contents. Otherwise the picker is
  // shown in a separate window.
  if (params.web_contents &&
      !params.web_contents->GetDelegate()->IsNeverComposited(
          params.web_contents)) {
    const Browser* browser =
        chrome::FindBrowserWithWebContents(params.web_contents);
    // Close the extension popup to prevent spoofing.
    if (browser && browser->window() &&
        browser->window()->GetExtensionsContainer()) {
      browser->window()->GetExtensionsContainer()->HideActivePopup();
    }
    constrained_window::ShowWebModalDialogViews(this, params.web_contents);
  } else {
#if BUILDFLAG(IS_MAC)
    // On Mac, MODAL_TYPE_CHILD with a null parent isn't allowed - fall back to
    // MODAL_TYPE_WINDOW.
    SetModalType(ui::MODAL_TYPE_WINDOW);
#endif
    CreateDialogWidget(this, params.context, nullptr)->Show();
  }
}

ShareThisTabDialogView::~ShareThisTabDialogView() = default;

void ShareThisTabDialogView::DetachParent() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  parent_ = nullptr;
}

gfx::Size ShareThisTabDialogView::CalculatePreferredSize() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static constexpr size_t kDialogViewWidth = 600;
  return gfx::Size(kDialogViewWidth, GetHeightForWidth(kDialogViewWidth));
}

std::u16string ShareThisTabDialogView::GetWindowTitle() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return l10n_util::GetStringFUTF16(IDS_SHARE_THIS_TAB_DIALOG_TITLE, app_name_);
}

bool ShareThisTabDialogView::Accept() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  if (parent_ && web_contents_) {
    parent_->NotifyDialogResult(DesktopMediaID(
        content::DesktopMediaID::TYPE_WEB_CONTENTS,
        content::DesktopMediaID::kNullId,
        content::WebContentsMediaCaptureId(
            web_contents_->GetPrimaryMainFrame()->GetProcess()->GetID(),
            web_contents_->GetPrimaryMainFrame()->GetRoutingID())));
  }

  // Return true to close the window.
  return true;
}

bool ShareThisTabDialogView::Cancel() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return views::DialogDelegateView::Cancel();
}

bool ShareThisTabDialogView::ShouldShowCloseButton() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return false;
}

BEGIN_METADATA(ShareThisTabDialogView, views::DialogDelegateView)
END_METADATA

ShareThisTabDialogViews::ShareThisTabDialogViews() : dialog_(nullptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ShareThisTabDialogViews::~ShareThisTabDialogViews() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (dialog_) {
    dialog_->DetachParent();
    dialog_->GetWidget()->Close();
  }
}

void ShareThisTabDialogViews::Show(
    const DesktopMediaPicker::Params& params,
    std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
    DoneCallback done_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(!callback_);
  CHECK(!dialog_);

  DesktopMediaPickerManager::Get()->OnShowDialog();
  callback_ = std::move(done_callback);
  dialog_ = new ShareThisTabDialogView(params, this);
}

void ShareThisTabDialogViews::NotifyDialogResult(const DesktopMediaID& source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Once this method is called the |dialog_| will close and destroy itself.
  dialog_->DetachParent();
  dialog_ = nullptr;

  DesktopMediaPickerManager::Get()->OnHideDialog();

  if (callback_.is_null()) {
    return;
  }

  // Notify the |callback_| asynchronously because it may need to destroy
  // DesktopMediaPicker.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), source));
}
