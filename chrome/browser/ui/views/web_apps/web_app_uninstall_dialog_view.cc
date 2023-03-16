// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_uninstall_dialog_view.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
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
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/native_window_tracker.h"
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
    WebAppUninstallDialogViews* dialog_view,
    web_app::AppId app_id,
    webapps::WebappUninstallSource uninstall_source,
    std::map<SquareSizePx, SkBitmap> icon_bitmaps)
    : dialog_(dialog_view), app_id_(std::move(app_id)), profile_(profile) {
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);
  DCHECK(provider);

  app_start_url_ = provider->registrar_unsafe().GetAppStartUrl(app_id_);
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
          provider->registrar_unsafe().GetAppShortName(app_id_))));

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
  if (!provider->registrar_unsafe().IsIsolated(app_id_)) {
    std::u16string checkbox_label = l10n_util::GetStringFUTF16(
        IDS_EXTENSION_UNINSTALL_PROMPT_REMOVE_DATA_CHECKBOX,
        url_formatter::FormatUrlForSecurityDisplay(
            app_start_url_, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));

    auto checkbox = std::make_unique<views::Checkbox>(checkbox_label);
    checkbox->SetMultiLine(true);
    checkbox_ = AddChildView(std::move(checkbox));
  }

  uninstall_source_ = uninstall_source;
}

WebAppUninstallDialogDelegateView::~WebAppUninstallDialogDelegateView() {
  if (dialog_)
    dialog_->UninstallCancelled();
}

void WebAppUninstallDialogDelegateView::OnDialogAccepted() {
  if (!dialog_)
    return;

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);
  DCHECK(provider);
  bool is_isolated_web_app = provider->registrar_unsafe().IsIsolated(app_id_);

  HistogramCloseAction action =
      is_isolated_web_app || (checkbox_ && checkbox_->GetChecked())
          ? HistogramCloseAction::kUninstallAndCheckboxChecked
          : HistogramCloseAction::kUninstall;
  UMA_HISTOGRAM_ENUMERATION("Webapp.UninstallDialogAction", action);

  Uninstall();

  if (is_isolated_web_app || (checkbox_ && checkbox_->GetChecked())) {
    ClearWebAppSiteData();
  }
}

void WebAppUninstallDialogDelegateView::OnDialogCanceled() {
  UMA_HISTOGRAM_ENUMERATION("Webapp.UninstallDialogAction",
                            HistogramCloseAction::kCancelled);
  // `dialog_->UninstallCancelled()` is handled in the destructor.
}

ui::ImageModel WebAppUninstallDialogDelegateView::GetWindowIcon() {
  return ui::ImageModel::FromImageSkia(image_);
}

void WebAppUninstallDialogDelegateView::Uninstall() {
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);
  DCHECK(provider);

  if (!provider->install_finalizer().CanUserUninstallWebApp(app_id_)) {
    std::exchange(dialog_, nullptr)->UninstallCancelled();
    return;
  }

  // Forward callback from the WebAppUninstallDialogViews because
  // WebAppUninstallDialogDelegateView lifetime is controlled by Widget and it
  // is terminiated as soon as dialog is closed regardless of web app
  // uninstallation.
  provider->install_finalizer().UninstallWebApp(app_id_, uninstall_source_,
                                                dialog_->UninstallStarted());
  // We successfully call Web App Uninstall routine, then
  // WebAppUninstallDialogDelegateView can be terminated, but can't call the
  // callback of the dialog caller.
  dialog_ = nullptr;
}

void WebAppUninstallDialogDelegateView::ClearWebAppSiteData() {
  content::ClearSiteData(base::BindRepeating(
                             [](content::BrowserContext* browser_context) {
                               return browser_context;
                             },
                             base::Unretained(profile_)),
                         url::Origin::Create(app_start_url_),
                         /*clear_cookies=*/true,
                         /*clear_storage=*/true, /*clear_cache=*/true,
                         /*storage_buckets_to_remove=*/{},
                         /*avoid_closing_connections=*/false,
                         /*cookie_partition_key=*/absl::nullopt,
                         /*storage_key=*/absl::nullopt,
                         /*partitioned_state_allowed_only=*/false,
                         base::DoNothing());
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

WebAppUninstallDialogViews::WebAppUninstallDialogViews(Profile* profile,
                                                       gfx::NativeWindow parent)
    : parent_(parent), profile_(profile) {
  if (parent)
    parent_window_tracker_ = views::NativeWindowTracker::Create(parent);
}

WebAppUninstallDialogViews::~WebAppUninstallDialogViews() {
  if (view_)
    view_->CancelDialog();
}

void WebAppUninstallDialogViews::ConfirmUninstall(
    const web_app::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    WebAppUninstallDialogViews::OnWebAppUninstallDialogClosed closed_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  app_id_ = app_id;
  closed_callback_ = std::move(closed_callback);

  if (parent_ && parent_window_tracker_->WasNativeWindowDestroyed()) {
    UninstallCancelled();
    return;
  }

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);
  DCHECK(provider);

  install_manager_observation_.Observe(&provider->install_manager());

  provider->icon_manager().ReadIcons(
      app_id, IconPurpose::ANY,
      provider->registrar_unsafe().GetAppDownloadedIconSizesAny(app_id),
      base::BindOnce(&WebAppUninstallDialogViews::OnIconsRead,
                     weak_ptr_factory_.GetWeakPtr(), uninstall_source));
}

void WebAppUninstallDialogViews::SetDialogShownCallbackForTesting(
    base::OnceClosure callback) {
  dialog_shown_callback_for_testing_ = std::move(callback);
}

void WebAppUninstallDialogViews::OnIconsRead(
    webapps::WebappUninstallSource uninstall_source,
    std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (parent_ && parent_window_tracker_->WasNativeWindowDestroyed()) {
    UninstallCancelled();
    return;
  }

  view_ = new WebAppUninstallDialogDelegateView(
      profile_, this, app_id_, uninstall_source, std::move(icon_bitmaps));

  constrained_window::CreateBrowserModalDialogViews(view_, parent_)->Show();

  if (dialog_shown_callback_for_testing_)
    std::move(dialog_shown_callback_for_testing_).Run();

  // This should be a tail call because it destroys |this|:
  view_->ProcessAutoConfirmValue();
}

void WebAppUninstallDialogViews::OnWebAppWillBeUninstalled(
    const web_app::AppId& app_id) {
  // Handle the case when web app was uninstalled externally and we have to
  // cancel current dialog.
  if (app_id == app_id_ && view_)
    view_->CancelDialog();
}

void WebAppUninstallDialogViews::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
  if (view_)
    view_->CancelDialog();
}

base::OnceCallback<void(webapps::UninstallResultCode code)>
WebAppUninstallDialogViews::UninstallStarted() {
  DCHECK(closed_callback_);
  // Next OnWebAppWillBeUninstalled should be ignored. Unsubscribe:
  install_manager_observation_.Reset();
  // The view can now be destroyed without us knowing, so clear it to prevent
  // UAF in the destructor.
  view_ = nullptr;
  return base::BindOnce(
      [](OnWebAppUninstallDialogClosed callback,
         webapps::UninstallResultCode code) { std::move(callback).Run(code); },
      std::move(closed_callback_));
}

void WebAppUninstallDialogViews::UninstallCancelled() {
  DCHECK(closed_callback_);
  view_ = nullptr;
  std::move(closed_callback_).Run(webapps::UninstallResultCode::kCancelled);
}

// static
std::unique_ptr<web_app::WebAppUninstallDialog>
web_app::WebAppUninstallDialog::Create(Profile* profile,
                                       gfx::NativeWindow parent) {
  return std::make_unique<WebAppUninstallDialogViews>(profile, parent);
}
