// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_uninstall_dialog_view.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/url_formatter/elide_url.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
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
    web_app::AppId app_id,
    webapps::WebappUninstallSource uninstall_source,
    std::map<SquareSizePx, SkBitmap> icon_bitmaps,
    UninstallChoiceCallback uninstall_choice_callback)
    : app_id_(std::move(app_id)),
      profile_(profile),
      uninstall_choice_callback_(std::move(uninstall_choice_callback)) {
  provider_ = web_app::WebAppProvider::GetForWebApps(profile_)->AsWeakPtr();
  DCHECK(provider_);

  app_start_url_ = provider_->registrar_unsafe().GetAppStartUrl(app_id_);
  DCHECK(!app_start_url_.is_empty());
  DCHECK(app_start_url_.is_valid());

  gfx::Size image_size{kIconSizeInDip, kIconSizeInDip};

  image_ = gfx::ImageSkia(std::make_unique<WebAppInfoImageSource>(
                              kIconSizeInDip, std::move(icon_bitmaps)),
                          image_size);

  SetModalType(ui::MODAL_TYPE_WINDOW);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  SetShowIcon(true);
  SetTitle(l10n_util::GetStringFUTF16(
      IDS_EXTENSION_PROMPT_UNINSTALL_TITLE,
      base::UTF8ToUTF16(
          provider_->registrar_unsafe().GetAppShortName(app_id_))));

  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
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
            app_start_url_, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));

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
    std::move(uninstall_choice_callback_).Run(/*uninstalled=*/false);
  }
}

void WebAppUninstallDialogDelegateView::OnDialogAccepted() {
  DCHECK(provider_);
  bool is_isolated_web_app = provider_->registrar_unsafe().IsIsolated(app_id_);

  HistogramCloseAction action =
      is_isolated_web_app || (checkbox_ && checkbox_->GetChecked())
          ? HistogramCloseAction::kUninstallAndCheckboxChecked
          : HistogramCloseAction::kUninstall;
  UMA_HISTOGRAM_ENUMERATION("Webapp.UninstallDialogAction", action);

  Uninstall();
  if (checkbox_ && checkbox_->GetChecked()) {
    ClearWebAppSiteData();
  }
}

void WebAppUninstallDialogDelegateView::OnDialogCanceled() {
  UMA_HISTOGRAM_ENUMERATION("Webapp.UninstallDialogAction",
                            HistogramCloseAction::kCancelled);
  // The user_dialog_choice_callback_ is run in the destructor.
}

void WebAppUninstallDialogDelegateView::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
  CancelDialog();
}

ui::ImageModel WebAppUninstallDialogDelegateView::GetWindowIcon() {
  return ui::ImageModel::FromImageSkia(image_);
}

void WebAppUninstallDialogDelegateView::OnWebAppWillBeUninstalled(
    const web_app::AppId& app_id) {
  // Handle the case when web app was uninstalled externally and we have to
  // cancel current dialog.
  if (app_id == app_id_) {
    CancelDialog();
  }
}

void WebAppUninstallDialogDelegateView::Uninstall() {
  DCHECK(provider_);
  if (!provider_->registrar_unsafe().CanUserUninstallWebApp(app_id_)) {
    std::move(uninstall_choice_callback_).Run(/*uninstalled=*/false);
    return;
  }

  std::move(uninstall_choice_callback_).Run(/*uninstalled=*/true);
  install_manager_observation_.Reset();
}

void WebAppUninstallDialogDelegateView::ClearWebAppSiteData() {
  content::ClearSiteData(
      base::BindRepeating(
          [](content::BrowserContext* browser_context) {
            return browser_context;
          },
          base::Unretained(profile_)),
      url::Origin::Create(app_start_url_), content::ClearSiteDataTypeSet::All(),
      /*storage_buckets_to_remove=*/{},
      /*avoid_closing_connections=*/false,
      /*cookie_partition_key=*/absl::nullopt,
      /*storage_key=*/absl::nullopt,
      /*partitioned_state_allowed_only=*/false, base::DoNothing());
}

void WebAppUninstallDialogDelegateView::ProcessAutoConfirmValue() {
  switch (extensions::ScopedTestDialogAutoConfirm::GetAutoConfirmValue()) {
    case extensions::ScopedTestDialogAutoConfirm::NONE:
      break;
    case extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION:
    case extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_REMEMBER_OPTION:
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

BEGIN_METADATA(WebAppUninstallDialogDelegateView, views::DialogDelegateView)
END_METADATA

namespace chrome {

void ShowWebAppUninstallDialog(
    Profile* profile,
    const web_app::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    gfx::NativeWindow parent,
    std::map<SquareSizePx, SkBitmap> icon_bitmaps,
    base::OnceCallback<void(bool)> uninstall_dialog_result_callback) {
  auto* view = new WebAppUninstallDialogDelegateView(
      profile, app_id, uninstall_source, std::move(icon_bitmaps),
      std::move(uninstall_dialog_result_callback));
  constrained_window::CreateBrowserModalDialogViews(view, parent)->Show();
  view->ProcessAutoConfirmValue();
}

}  // namespace chrome
