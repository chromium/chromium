// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/callback_forward.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/controls/site_icon_text_and_origin_view.h"
#include "chrome/browser/ui/views/shortcuts/create_desktop_shortcut_delegate.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/prefs/pref_service.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "url/gurl.h"

namespace chrome {

void ShowCreateShortcutDialog(
    content::WebContents* web_contents,
    const gfx::ImageSkia& icon,
    std::u16string title,
    CreateShortcutDialogCallback dialog_action_and_text_callback) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    std::move(dialog_action_and_text_callback)
        .Run(/*is_accepted=*/false, title);
    return;
  }

  // Do not show the dialog if it is already being shown.
  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  if (!manager || manager->IsDialogActive()) {
    std::move(dialog_action_and_text_callback)
        .Run(/*is_accepted=*/false, title);
    return;
  }

  // If there is more than one profile on the device, ensure the user knows
  // which profile is triggering the shortcut creation.
  Profile* current_profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (current_profile) {
    title = shortcuts::AppendProfileNameToTitleIfNeeded(current_profile, title);
  }

  auto delegate = std::make_unique<shortcuts::CreateDesktopShortcutDelegate>(
      web_contents, std::move(dialog_action_and_text_callback));
  auto delegate_weak_ptr = delegate->AsWeakPtr();

  auto dialog_model =
      ui::DialogModel::Builder(std::move(delegate))
          .SetInternalName("CreateDesktopShortcutDialog")
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_CREATE_SHORTCUT_NOT_APPS_DIALOG_TITLE))
          .SetSubtitle(l10n_util::GetStringUTF16(
              IDS_CREATE_SHORTCUT_NOT_APPS_DIALOG_SUBTITLE))
          .AddOkButton(base::BindOnce(
                           &shortcuts::CreateDesktopShortcutDelegate::OnAccept,
                           delegate_weak_ptr),
                       ui::DialogModel::Button::Params()
                           .SetLabel(l10n_util::GetStringUTF16(
                               IDS_CREATE_SHORTCUTS_BUTTON_LABEL))
                           .SetId(shortcuts::CreateDesktopShortcutDelegate::
                                      kCreateShortcutDialogOkButtonId))
          // Dialog cancellations and closes are handled properly by the dialog
          // destroying callback.
          .AddCancelButton(base::DoNothing())
          .SetDialogDestroyingCallback(
              base::BindOnce(&shortcuts::CreateDesktopShortcutDelegate::OnClose,
                             delegate_weak_ptr))
          .OverrideDefaultButton(ui::DialogButton::DIALOG_BUTTON_NONE)
          .Build();

  dialog_model->AddCustomField(
      std::make_unique<views::BubbleDialogModelHost::CustomView>(
          std::make_unique<SiteIconTextAndOriginView>(
              icon, title,
              l10n_util::GetStringUTF16(
                  IDS_CREATE_SHORTCUT_NOT_APPS_AX_BUBBLE_LABEL),
              web_contents->GetLastCommittedURL(), web_contents,
              base::BindRepeating(
                  &shortcuts::CreateDesktopShortcutDelegate::OnTitleUpdated,
                  delegate_weak_ptr)),
          views::BubbleDialogModelHost::FieldType::kControl));

  auto dialog = views::BubbleDialogModelHost::CreateModal(
      std::move(dialog_model), ui::MODAL_TYPE_CHILD);

  base::RecordAction(
      base::UserMetricsAction("CreateDesktopShortcutDialogShown"));
  constrained_window::ShowWebModalDialogViews(dialog.release(), web_contents);
}

}  // namespace chrome
