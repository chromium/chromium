// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_uninstall_dialog_view.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_uninstall_dialog_user_options.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/url_formatter/elide_url.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

constexpr int kIconSizeInDip = 32;

// The type of action the dialog took at close. Do not reorder this enum as it
// is used in UMA histograms. Any new entries must be added into
// WebappUninstallDialogAction enum in enums.xml file. Matches
// ExtensionUninstallDialog::CloseAction for historical reasons.
enum HistogramCloseAction {
  kUninstall = 0,
  kUninstallAndCheckboxChecked = 1,
  kCancelled = 2,
  kMaxValue = kCancelled
};

}  // namespace

WebAppUninstallDialogDelegateView::WebAppUninstallDialogDelegateView(
    Profile* profile,
    webapps::AppId app_id,
    webapps::WebappUninstallSource uninstall_source,
    std::map<web_app::SquareSizePx, SkBitmap> icon_bitmaps,
    web_app::UninstallDialogCallback uninstall_choice_callback)
    : app_id_(std::move(app_id)),
      profile_(profile),
      uninstall_choice_callback_(std::move(uninstall_choice_callback)) {
  provider_ = web_app::WebAppProvider::GetForWebApps(profile_)->AsWeakPtr();
  DCHECK(provider_);

  const GURL app_start_url =
      provider_->registrar_unsafe().GetAppStartUrl(app_id_);
  DCHECK(!app_start_url.is_empty());
  DCHECK(app_start_url.is_valid());

  gfx::Size image_size{kIconSizeInDip, kIconSizeInDip};

  image_ = gfx::ImageSkia(std::make_unique<WebAppInfoImageSource>(
                              kIconSizeInDip, std::move(icon_bitmaps)),
                          image_size);

  SetModalType(ui::mojom::ModalType::kWindow);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  SetShowIcon(true);
  SetTitle(l10n_util::GetStringFUTF16(
      IDS_EXTENSION_PROMPT_UNINSTALL_TITLE,
      base::UTF8ToUTF16(
          provider_->registrar_unsafe().GetAppShortName(app_id_))));

  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON));
  SetAcceptCallback(
      base::BindOnce(&WebAppUninstallDialogDelegateView::OnDialogAccepted,
                     base::Unretained(this)));
  SetCancelCallback(
      base::BindOnce(&WebAppUninstallDialogDelegateView::OnDialogCanceled,
                     base::Unretained(this)));

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  // Add margins for the icon plus the icon-title padding so that the dialog
  // contents align with the title text.
  gfx::Insets insets = layout_provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText);
  set_margins(insets +
              gfx::Insets::TLBR(0, insets.left() + kIconSizeInDip, 0, 0));

  // For IWAs checkbox will not be displayed, removal of
  // storage is automatically enforced.
  if (!provider_->registrar_unsafe().IsIsolated(app_id_)) {
    std::u16string checkbox_label = l10n_util::GetStringFUTF16(
        IDS_EXTENSION_UNINSTALL_PROMPT_REMOVE_DATA_CHECKBOX,
        url_formatter::FormatUrlForSecurityDisplay(
            app_start_url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));

    auto checkbox = std::make_unique<views::Checkbox>(checkbox_label);
    checkbox->SetMultiLine(true);
    checkbox_ = AddChildView(std::move(checkbox));
  }

  uninstall_source_ = uninstall_source;
  install_manager_observation_.Observe(&provider_->install_manager());
}

WebAppUninstallDialogDelegateView::~WebAppUninstallDialogDelegateView() {
  install_manager_observation_.Reset();
  if (uninstall_choice_callback_) {
    std::move(uninstall_choice_callback_).Run(web_app::UninstallUserOptions());
  }
}

void WebAppUninstallDialogDelegateView::OnDialogAccepted() {
  DCHECK(provider_);
  bool is_isolated_web_app = provider_->registrar_unsafe().IsIsolated(app_id_);
  bool clear_site_data = checkbox_ && checkbox_->GetChecked();

  HistogramCloseAction action =
      is_isolated_web_app || clear_site_data
          ? HistogramCloseAction::kUninstallAndCheckboxChecked
          : HistogramCloseAction::kUninstall;
  UMA_HISTOGRAM_ENUMERATION("Webapp.UninstallDialogAction", action);

  Uninstall(clear_site_data);
}

void WebAppUninstallDialogDelegateView::OnDialogCanceled() {
  UMA_HISTOGRAM_ENUMERATION("Webapp.UninstallDialogAction",
                            HistogramCloseAction::kCancelled);
  web_app::UninstallUserOptions uninstall_options;
  uninstall_options.user_wants_uninstall = false;
  if (uninstall_choice_callback_) {
    std::move(uninstall_choice_callback_).Run(uninstall_options);
  }
}

void WebAppUninstallDialogDelegateView::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
  CancelDialog();
}

ui::ImageModel WebAppUninstallDialogDelegateView::GetWindowIcon() {
  return ui::ImageModel::FromImageSkia(image_);
}

void WebAppUninstallDialogDelegateView::OnWebAppWillBeUninstalled(
    const webapps::AppId& app_id) {
  // Handle the case when web app was uninstalled externally and we have to
  // cancel current dialog.
  if (app_id == app_id_) {
    CancelDialog();
  }
}

void WebAppUninstallDialogDelegateView::Uninstall(bool clear_site_data) {
  install_manager_observation_.Reset();
  web_app::UninstallUserOptions user_options;
  user_options.clear_site_data = clear_site_data;
  user_options.user_wants_uninstall = true;
  std::move(uninstall_choice_callback_).Run(user_options);
}

void WebAppUninstallDialogDelegateView::ProcessAutoConfirmValue() {
  switch (extensions::ScopedTestDialogAutoConfirm::GetAutoConfirmValue()) {
    case extensions::ScopedTestDialogAutoConfirm::NONE:
      break;
    case extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION:
      checkbox_->SetChecked(/*checked=*/true);
      AcceptDialog();
      break;
    case extensions::ScopedTestDialogAutoConfirm::ACCEPT:
      AcceptDialog();
      break;
    case extensions::ScopedTestDialogAutoConfirm::CANCEL:
      CancelDialog();
      break;
  }
}

BEGIN_METADATA(WebAppUninstallDialogDelegateView)
END_METADATA

namespace web_app {

void ShowWebAppUninstallDialog(
    Profile* profile,
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    gfx::NativeWindow parent,
    std::map<web_app::SquareSizePx, SkBitmap> icon_bitmaps,
    web_app::UninstallDialogCallback uninstall_dialog_result_callback) {
  auto* view = new WebAppUninstallDialogDelegateView(
      profile, app_id, uninstall_source, std::move(icon_bitmaps),
      std::move(uninstall_dialog_result_callback));
  constrained_window::CreateBrowserModalDialogViews(view, parent)->Show();
  view->ProcessAutoConfirmValue();
}

}  // namespace web_app
