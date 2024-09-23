// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/shortcuts/create_desktop_shortcut.h"

#include <optional>
#include <string>

#include "base/check_is_test.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shortcuts/create_shortcut_for_current_web_contents_task.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/controls/site_icon_text_and_origin_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
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
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace shortcuts {

namespace {

// Shows the `Create Shortcut` dialog to create fire and forget entities on the
// desktop of the OS. This API works only if kShortcutsNotApps is enabled.
// Triggered from the three-dot menu on Chrome, Save & Share > Create Shortcut.
// Callers of the API should pass a |CreateShortcutDialogCallback| so that the
// user action on the dialog or the title in the dialog's text field can be
// obtained.
void ShowCreateDesktopShortcutDialog(
    content::WebContents* web_contents,
    const gfx::ImageSkia& icon,
    std::u16string title,
    CreateShortcutDialogCallback dialog_action_and_text_callback) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    std::move(dialog_action_and_text_callback).Run(std::nullopt);
    return;
  }

  // Do not show the dialog if it is already being shown.
  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  if (!manager || manager->IsDialogActive()) {
    std::move(dialog_action_and_text_callback).Run(std::nullopt);
    return;
  }

  // If there is more than one profile on the device, ensure the user knows
  // which profile is triggering the shortcut creation.
  Profile* current_profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (current_profile) {
    title = AppendProfileNameToTitleIfNeeded(current_profile, title);
  }

  auto delegate = std::make_unique<CreateDesktopShortcutDelegate>(
      web_contents, std::move(dialog_action_and_text_callback));
  auto delegate_weak_ptr = delegate->AsWeakPtr();

  auto dialog_model_builder = ui::DialogModel::Builder(std::move(delegate));
  dialog_model_builder.SetInternalName("CreateDesktopShortcutDialog")
      .SetTitle(
          l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUT_NOT_APPS_DIALOG_TITLE))
      .SetSubtitle(l10n_util::GetStringUTF16(
          IDS_CREATE_SHORTCUT_NOT_APPS_DIALOG_SUBTITLE))
      .AddOkButton(base::BindOnce(&CreateDesktopShortcutDelegate::OnAccept,
                                  delegate_weak_ptr),
                   ui::DialogModel::Button::Params()
                       .SetLabel(l10n_util::GetStringUTF16(
                           IDS_CREATE_SHORTCUTS_BUTTON_LABEL))
                       .SetId(CreateDesktopShortcutDelegate::
                                  kCreateShortcutDialogOkButtonId))
      // Dialog cancellations and closes are handled properly by the dialog
      // destroying callback.
      .AddCancelButton(base::DoNothing())
      .SetDialogDestroyingCallback(base::BindOnce(
          &CreateDesktopShortcutDelegate::OnClose, delegate_weak_ptr))
      .OverrideDefaultButton(ui::mojom::DialogButton::kNone);

  auto site_view = std::make_unique<SiteIconTextAndOriginView>(
      icon, title,
      l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUT_NOT_APPS_AX_BUBBLE_LABEL),
      web_contents->GetLastCommittedURL(), web_contents,
      base::BindRepeating(&CreateDesktopShortcutDelegate::OnTitleUpdated,
                          delegate_weak_ptr));
  views::Textfield* title_field = site_view->title_field();

  auto dialog_model =
      dialog_model_builder
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  std::move(site_view),
                  views::BubbleDialogModelHost::FieldType::kControl,
                  title_field),
              shortcuts::CreateDesktopShortcutDelegate::
                  kCreateShortcutDialogTitleFieldId)
          .SetInitiallyFocusedField(shortcuts::CreateDesktopShortcutDelegate::
                                        kCreateShortcutDialogTitleFieldId)
          .Build();

  auto dialog = views::BubbleDialogModelHost::CreateModal(
      std::move(dialog_model), ui::mojom::ModalType::kChild);

  base::RecordAction(
      base::UserMetricsAction("CreateDesktopShortcutDialogShown"));
  views::Widget* create_shortcuts_dialog_widget =
      constrained_window::ShowWebModalDialogViews(dialog.release(),
                                                  web_contents);
  delegate_weak_ptr->StartObservingForPictureInPictureOcclusion(
      create_shortcuts_dialog_widget);
}

}  // namespace

void ShowCreateDesktopShortcutDialogForTesting(
    content::WebContents* web_contents,
    const gfx::ImageSkia& icon,
    std::u16string title,
    CreateShortcutDialogCallback dialog_action_and_text_callback) {
  CHECK_IS_TEST();
  ShowCreateDesktopShortcutDialog(web_contents, icon, title,
                                  std::move(dialog_action_and_text_callback));
}

void CreateShortcutForWebContents(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool shortcuts_created)>
        shortcut_creation_callback) {
  CreateShortcutForCurrentWebContentsTask::CreateAndStart(
      *web_contents,
      base::BindOnce(&ShowCreateDesktopShortcutDialog, web_contents),
      std::move(shortcut_creation_callback));
}

}  // namespace shortcuts

namespace chrome {

void CreateDesktopShortcutForActiveWebContents(Browser* browser) {
  content::WebContents* const web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  if (!web_contents) {
    return;
  }

  shortcuts::CreateShortcutForWebContents(web_contents, base::DoNothing());
}

}  // namespace chrome
