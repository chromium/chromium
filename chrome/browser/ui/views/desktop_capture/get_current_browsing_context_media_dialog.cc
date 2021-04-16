// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/get_current_browsing_context_media_dialog.h"

#include "base/command_line.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"

namespace {
constexpr int kCheckboxId = 1;

class GetCurrentBrowsingContextMediaDialogDelegate
    : public ui::DialogModelDelegate {
 public:
  GetCurrentBrowsingContextMediaDialogDelegate(
      const DesktopMediaPicker::Params& params,
      GetCurrentBrowsingContextMediaDialog* parent)
      : render_process_id_(
            params.web_contents->GetMainFrame()->GetProcess()->GetID()),
        render_frame_id_(params.web_contents->GetMainFrame()->GetRoutingID()),
        parent_(parent) {
    DCHECK(parent_);
  }

  // Callback functions for when the permission is granted.
  void OnAccept() {
    // |id| is non-null if and only if it refers to a native screen/window.
    content::DesktopMediaID source(content::DesktopMediaID::TYPE_WEB_CONTENTS,
                                   /*id=*/content::DesktopMediaID::kNullId,
                                   content::WebContentsMediaCaptureId(
                                       render_process_id_, render_frame_id_));

    source.audio_share =
        dialog_model()->HasField(kCheckboxId) &&
        dialog_model()->GetCheckboxByUniqueId(kCheckboxId)->is_checked();

    // Gets the tab to be shared, which is the current tab in this
    // case.
    content::WebContents* const tab = content::WebContents::FromRenderFrameHost(
        content::RenderFrameHost::FromID(render_process_id_, render_frame_id_));
    // Activates the current tab and browser as confirmation that this is the
    // tab being shared as the user might tab away between the time "share" was
    // pressed and the actual share starts.
    tab->GetDelegate()->ActivateContents(tab);
    Browser* browser = chrome::FindBrowserWithWebContents(tab);
    if (browser && browser->window()) {
      browser->window()->Activate();
    }

    if (parent_) {
      parent_->NotifyDialogResult(source);
      parent_ = nullptr;
    }
  }

  // Callback functions for when the permission is rejected or if the
  // dialog/window is closed by the user.
  void OnClose() {
    if (parent_) {
      parent_->NotifyDialogResult(content::DesktopMediaID());
      parent_ = nullptr;
    }
  }

 private:
  // The pair of values (render_process_id_, render_frame_id_) together define
  // the browsing context which is to be shared.
  const int render_process_id_;
  const int render_frame_id_;

  GetCurrentBrowsingContextMediaDialog* parent_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GetCurrentBrowsingContextMediaDialogDelegate);
};
}  // namespace

GetCurrentBrowsingContextMediaDialog::GetCurrentBrowsingContextMediaDialog()
    : auto_accept_tab_capture_for_testing_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kThisTabCaptureAutoAccept)),
      auto_reject_tab_capture_for_testing_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kThisTabCaptureAutoReject)) {
  DCHECK(!auto_accept_tab_capture_for_testing_ ||
         !auto_reject_tab_capture_for_testing_);
}

GetCurrentBrowsingContextMediaDialog::~GetCurrentBrowsingContextMediaDialog() =
    default;

void GetCurrentBrowsingContextMediaDialog::NotifyDialogResult(
    const content::DesktopMediaID& source) {
  DesktopMediaPickerManager::Get()->OnHideDialog();

  if (callback_) {
    std::move(callback_).Run(source);
  }
}

void GetCurrentBrowsingContextMediaDialog::Show(
    const DesktopMediaPicker::Params& params,
    std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
    DoneCallback done_callback) {
  DCHECK(params.web_contents);

  DesktopMediaPickerManager::Get()->OnShowDialog();
  callback_ = std::move(done_callback);
  std::unique_ptr<views::DialogDelegate> unique_delegate =
      CreateDialogHost(params);

  constrained_window::ShowWebModalDialogViews(unique_delegate.release(),
                                              params.web_contents);

  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::CURRENT_BROWSING_CONTEXT_CONFIRMATION_BOX);

  MaybeAutomateUserInput();
}

std::unique_ptr<views::DialogDelegate>
GetCurrentBrowsingContextMediaDialog::CreateDialogHost(
    const DesktopMediaPicker::Params& params) {
  auto unique_model_delegate =
      std::make_unique<GetCurrentBrowsingContextMediaDialogDelegate>(params,
                                                                     this);
  GetCurrentBrowsingContextMediaDialogDelegate* model_delegate =
      unique_model_delegate.get();

  ui::DialogModel::Builder dialog_builder(std::move(unique_model_delegate));
  // TODO(crbug.com/1136942): Reconcile design-doc and implementation wrt
  // the body text; display frame's URL.
  dialog_builder
      .AddBodyText(ui::DialogModelLabel(l10n_util::GetStringFUTF16(
          IDS_GET_CURRENT_BROWSING_CONTEXT_MEDIA_DIALOG_TEXT_REQUESTED_BY_APP,
          params.target_name)))
      .AddCancelButton(
          base::BindOnce(&GetCurrentBrowsingContextMediaDialogDelegate::OnClose,
                         base::Unretained(model_delegate)))
      .AddOkButton(
          base::BindOnce(
              &GetCurrentBrowsingContextMediaDialogDelegate::OnAccept,
              base::Unretained(model_delegate)),
          l10n_util::GetStringUTF16(
              IDS_GET_CURRENT_BROWSING_CONTEXT_MEDIA_DIALOG_SHARE_BUTTON))
      .SetCloseCallback(
          base::BindOnce(&GetCurrentBrowsingContextMediaDialogDelegate::OnClose,
                         base::Unretained(model_delegate)))
      .SetTitle(l10n_util::GetStringUTF16(
          IDS_GET_CURRENT_BROWSING_CONTEXT_MEDIA_DIALOG_TITLE));

  if (params.request_audio) {
    dialog_builder.AddCheckbox(
        kCheckboxId,
        ui::DialogModelLabel(l10n_util::GetStringUTF16(
            IDS_GET_CURRENT_BROWSING_CONTEXT_MEDIA_DIALOG_AUDIO_SHARE)),
        ui::DialogModelCheckbox::Params().SetIsChecked(
            params.approve_audio_by_default));
  }

  auto dialog_host = views::BubbleDialogModelHost::CreateModal(
      dialog_builder.Build(), params.modality);
  dialog_host->SetDefaultButton(ui::DIALOG_BUTTON_CANCEL);
  dialog_host->SetOwnedByWidget(true);
  dialog_model_host_for_testing_ = dialog_host.get();
  return dialog_host;
}

void GetCurrentBrowsingContextMediaDialog::MaybeAutomateUserInput() {
  if (!auto_accept_tab_capture_for_testing_ &&
      !auto_reject_tab_capture_for_testing_) {
    return;
  }

  // dialog_model_host_for_testing_ outlives the callback here because its
  // ownership is passed to the widget, when the widget is created, and this
  // widget cannot get destroyed in a test without going through this callback.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](views::BubbleDialogModelHost* dialog_host, bool accept) {
            accept ? dialog_host->Accept() : dialog_host->Cancel();
          },
          base::Unretained(dialog_model_host_for_testing_),
          auto_accept_tab_capture_for_testing_));
}