// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/web_install/web_install.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "web_app_modal_dialog_delegate.h"

namespace web_app {

class WebAppInstallNotSupportedDialogDelegate
    : public WebAppModalDialogDelegate {
 public:
  WebAppInstallNotSupportedDialogDelegate(content::WebContents* web_contents,
                                          Profile* profile,
                                          base::OnceClosure callback)
      : WebAppModalDialogDelegate(web_contents),
        profile_(profile),
        callback_(std::move(callback)) {}

  ~WebAppInstallNotSupportedDialogDelegate() override = default;

  void OnAccept() { std::move(callback_).Run(); }

  void OnDestroyed() {
    if (callback_.is_null()) {
      return;
    }
    std::move(callback_).Run();
  }

  void CloseDialogAsIgnored() override {
    if (!dialog_model() || !dialog_model()->host()) {
      return;
    }
    std::move(callback_).Run();
    dialog_model()->host()->Close();
  }

  base::WeakPtr<WebAppInstallNotSupportedDialogDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  raw_ptr<Profile> profile_;
  base::OnceClosure callback_;

  base::WeakPtrFactory<WebAppInstallNotSupportedDialogDelegate>
      weak_ptr_factory_{this};
};

void ShowInstallNotSupportedDialog(content::WebContents* web_contents,
                                   Profile* profile,
                                   base::OnceClosure callback) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    std::move(callback).Run();
    return;
  }

  // Do not show the dialog if it is already being shown.
  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  if (!manager || manager->IsDialogActive()) {
    std::move(callback).Run();
    return;
  }

  auto delegate = std::make_unique<WebAppInstallNotSupportedDialogDelegate>(
      web_contents, profile, std::move(callback));
  auto delegate_weak_ptr = delegate->AsWeakPtr();

  // Show incognito title unless the profile is in guest mode.
  int title_id =
      profile->IsGuestSession()
          ? IDS_WEB_APP_INSTALL_NOT_SUPPORTED_DIALOG_TITLE_GUEST_MODE
          : IDS_WEB_APP_INSTALL_NOT_SUPPORTED_DIALOG_TITLE_INCOGNITO_MODE;

  auto dialog_model =
      ui::DialogModel::Builder(std::move(delegate))
          .SetInternalName("WebAppInstallNotSupportedDialog")
          .SetTitle(l10n_util::GetStringUTF16(title_id))
          .AddOkButton(
              base::BindOnce(&WebAppInstallNotSupportedDialogDelegate::OnAccept,
                             delegate_weak_ptr),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(IDS_EXTENSION_ALERT_ITEM_OK)))
          .SetDialogDestroyingCallback(base::BindOnce(
              &WebAppInstallNotSupportedDialogDelegate::OnDestroyed,
              delegate_weak_ptr))
          .Build();
  auto dialog = views::BubbleDialogModelHost::CreateModal(
      std::move(dialog_model), ui::mojom::ModalType::kChild);

  views::Widget* install_not_supported_dialog_widget =
      constrained_window::ShowWebModalDialogViews(dialog.release(),
                                                  web_contents);
  delegate_weak_ptr->OnWidgetShownStartTracking(
      install_not_supported_dialog_widget);
}

}  // namespace web_app
