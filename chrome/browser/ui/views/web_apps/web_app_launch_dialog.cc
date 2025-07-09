// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/commands/launch_web_app_command.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/web_install/web_install.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "web_app_modal_dialog_delegate.h"

namespace web_app {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kWebInstallLaunchDialogAppName);
using WebAppBackgroundAppLaunchAcceptanceCallback =
    base::OnceCallback<void(bool accepted)>;

namespace {
bool g_auto_accept_launch_for_testing = false;
}  // namespace

class WebAppLaunchDialogDelegate : public WebAppModalDialogDelegate {
 public:
  WebAppLaunchDialogDelegate(
      content::WebContents* web_contents,
      WebAppBackgroundAppLaunchAcceptanceCallback callback,
      webapps::AppId app_id,
      Profile* profile)
      : WebAppModalDialogDelegate(web_contents),
        callback_(std::move(callback)),
        app_id_(app_id),
        profile_(profile) {}

  ~WebAppLaunchDialogDelegate() override = default;

  void OnAccept() {
    std::move(callback_).Run(/*accepted=*/true);

    WebAppProvider* provider = WebAppProvider::GetForWebApps(profile_);
    if (provider) {
      apps::AppLaunchParams params(
          app_id_, apps::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW,
          apps::LaunchSource::kFromWebInstallApi);

      provider->command_manager().ScheduleCommand(
          std::make_unique<LaunchWebAppCommand>(
              profile_, provider, std::move(params),
              LaunchWebAppWindowSetting::kUseLaunchParams, base::DoNothing()));
    }
  }

  void OnCancel() { std::move(callback_).Run(/*accepted=*/false); }

  void OnDestroyed() {
    if (!callback_.is_null()) {
      std::move(callback_).Run(/*accepted=*/false);
    }
  }

  void CloseDialogAsIgnored() override {
    if (!dialog_model() || !dialog_model()->host()) {
      return;
    }
    dialog_model()->host()->Close();
  }

  base::WeakPtr<WebAppLaunchDialogDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  WebAppBackgroundAppLaunchAcceptanceCallback callback_;
  webapps::AppId app_id_;
  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<WebAppLaunchDialogDelegate> weak_ptr_factory_{this};
};

void ShowWebInstallAppLaunchDialog(
    content::WebContents* web_contents,
    const webapps::AppId& app_id,
    Profile* profile,
    std::string app_name,
    const SkBitmap& icon,
    WebAppBackgroundAppLaunchAcceptanceCallback callback) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    std::move(callback).Run(/*accepted=*/false);
    return;
  }

  // Do not show the dialog if it is already being shown.
  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  if (!manager || manager->IsDialogActive()) {
    std::move(callback).Run(/*accepted=*/false);
    return;
  }

  views::BubbleDialogDelegate* dialog_delegate = nullptr;

  auto delegate = std::make_unique<WebAppLaunchDialogDelegate>(
      web_contents, std::move(callback), app_id, profile);
  auto delegate_weak_ptr = delegate->AsWeakPtr();

  const gfx::ImageSkia icon_skia = gfx::ImageSkia::CreateFrom1xBitmap(icon);
  const ui::ImageModel icon_model = ui::ImageModel::FromImageSkia(icon_skia);

  auto dialog_model =
      ui::DialogModel::Builder(std::move(delegate))
          .SetInternalName("WebInstallLaunchDialog")
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_INTENT_PICKER_BUBBLE_VIEW_OPEN_WITH))
          .SetIcon(icon_model)
          .AddParagraph(ui::DialogModelLabel(base::UTF8ToUTF16(app_name)))
          .AddOkButton(base::BindOnce(&WebAppLaunchDialogDelegate::OnAccept,
                                      delegate_weak_ptr),
                       ui::DialogModel::Button::Params().SetLabel(
                           l10n_util::GetStringUTF16(
                               IDS_INTENT_PICKER_BUBBLE_VIEW_OPEN)))
          .AddCancelButton(base::BindOnce(&WebAppLaunchDialogDelegate::OnCancel,
                                          delegate_weak_ptr))
          .SetCloseActionCallback(base::BindOnce(
              &WebAppLaunchDialogDelegate::OnCancel, delegate_weak_ptr))
          .SetDialogDestroyingCallback(base::BindOnce(
              &WebAppLaunchDialogDelegate::OnDestroyed, delegate_weak_ptr))
          .OverrideDefaultButton(ui::mojom::DialogButton::kCancel)
          .Build();
  auto dialog = views::BubbleDialogModelHost::CreateModal(
      std::move(dialog_model), ui::mojom::ModalType::kChild);

  dialog_delegate = dialog->AsBubbleDialogDelegate();
  views::Widget* launch_dialog_widget =
      constrained_window::ShowWebModalDialogViews(dialog.release(),
                                                  web_contents);
  delegate_weak_ptr->OnWidgetShownStartTracking(launch_dialog_widget);

  if (g_auto_accept_launch_for_testing) {
    dialog_delegate->AcceptDialog();
  }
}

base::AutoReset<bool> SetAutoAcceptWebInstallLaunchDialogForTesting() {
  return base::AutoReset<bool>(&g_auto_accept_launch_for_testing, true);
}

}  // namespace web_app
