// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_uninstall_dialog_view.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/native_window_tracker.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
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
    WebAppUninstallDialogViews* dialog_view,
    web_app::AppId app_id,
    std::map<SquareSizePx, SkBitmap> icon_bitmaps)
    : dialog_(dialog_view), app_id_(app_id), profile_(profile) {
  auto* provider = web_app::WebAppProvider::Get(profile_);
  DCHECK(provider);

  app_start_url_ = provider->registrar().GetAppStartUrl(app_id_);
  DCHECK(!app_start_url_.is_empty());
  DCHECK(app_start_url_.is_valid());

  gfx::Size image_size{kIconSizeInDip, kIconSizeInDip};

  image_ = gfx::ImageSkia(
      std::make_unique<WebAppInfoImageSource>(kIconSizeInDip, icon_bitmaps),
      image_size);

  SetShowCloseButton(false);
  SetShowIcon(true);
  SetTitle(l10n_util::GetStringFUTF16(
      IDS_EXTENSION_PROMPT_UNINSTALL_TITLE,
      base::UTF8ToUTF16(provider->registrar().GetAppShortName(app_id_))));

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
  gfx::Insets insets =
      layout_provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT);
  set_margins(insets + gfx::Insets(0, insets.left() + kIconSizeInDip, 0, 0));

  base::string16 checkbox_label = l10n_util::GetStringFUTF16(
      IDS_EXTENSION_UNINSTALL_PROMPT_REMOVE_DATA_CHECKBOX,
      url_formatter::FormatUrlForSecurityDisplay(
          app_start_url_, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));

  auto checkbox = std::make_unique<views::Checkbox>(checkbox_label);
  checkbox->SetMultiLine(true);
  checkbox_ = AddChildView(std::move(checkbox));

  chrome::RecordDialogCreation(chrome::DialogIdentifier::EXTENSION_UNINSTALL);
}

WebAppUninstallDialogDelegateView::~WebAppUninstallDialogDelegateView() {
  if (dialog_)
    dialog_->CallCallback(/*uninstalled=*/false);
}

void WebAppUninstallDialogDelegateView::OnDialogAccepted() {
  if (!dialog_)
    return;

  HistogramCloseAction action =
      checkbox_->GetChecked()
          ? HistogramCloseAction::kUninstallAndCheckboxChecked
          : HistogramCloseAction::kUninstall;
  UMA_HISTOGRAM_ENUMERATION("Webapp.UninstallDialogAction", action);

  bool uninstalled = Uninstall();
  if (checkbox_->GetChecked())
    ClearWebAppSiteData();

  std::exchange(dialog_, nullptr)->CallCallback(uninstalled);
}

void WebAppUninstallDialogDelegateView::OnDialogCanceled() {
  UMA_HISTOGRAM_ENUMERATION("Webapp.UninstallDialogAction",
                            HistogramCloseAction::kCancelled);

  if (dialog_)
    std::exchange(dialog_, nullptr)->CallCallback(/*uninstalled=*/false);
}

gfx::Size WebAppUninstallDialogDelegateView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

ui::ModalType WebAppUninstallDialogDelegateView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

gfx::ImageSkia WebAppUninstallDialogDelegateView::GetWindowIcon() {
  return image_;
}

bool WebAppUninstallDialogDelegateView::Uninstall() {
  auto* provider = web_app::WebAppProvider::Get(profile_);
  DCHECK(provider);

  if (!provider->install_finalizer().CanUserUninstallExternalApp(app_id_))
    return false;

  dialog_->UninstallStarted();

  provider->install_finalizer().UninstallExternalAppByUser(app_id_,
                                                           base::DoNothing());
  return true;
}

void WebAppUninstallDialogDelegateView::ClearWebAppSiteData() {
  content::ClearSiteData(
      base::BindRepeating(
          [](content::BrowserContext* browser_context) {
            return browser_context;
          },
          base::Unretained(profile_)),
      url::Origin::Create(app_start_url_), /*clear_cookies=*/true,
      /*clear_storage=*/true, /*clear_cache=*/true,
      /*avoid_closing_connections=*/false, base::DoNothing());
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

WebAppUninstallDialogViews::WebAppUninstallDialogViews(Profile* profile,
                                                       gfx::NativeWindow parent)
    : parent_(parent), profile_(profile) {
  if (parent)
    parent_window_tracker_ = NativeWindowTracker::Create(parent);
}

WebAppUninstallDialogViews::~WebAppUninstallDialogViews() {
  if (view_)
    view_->CancelDialog();
}

void WebAppUninstallDialogViews::ConfirmUninstall(
    const web_app::AppId& app_id,
    WebAppUninstallDialogViews::OnWebAppUninstallDialogClosed closed_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  app_id_ = app_id;
  closed_callback_ = std::move(closed_callback);

  if (parent_ && parent_window_tracker_->WasNativeWindowClosed()) {
    CallCallback(/*uninstalled=*/false);
    return;
  }

  auto* provider = web_app::WebAppProvider::Get(profile_);
  DCHECK(provider);

  registrar_observer_.Add(&provider->registrar());

  provider->icon_manager().ReadIcons(
      app_id, IconPurpose::ANY,
      provider->registrar().GetAppDownloadedIconSizesAny(app_id),
      base::BindOnce(&WebAppUninstallDialogViews::OnIconsRead,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebAppUninstallDialogViews::SetDialogShownCallbackForTesting(
    base::OnceClosure callback) {
  dialog_shown_callback_for_testing_ = std::move(callback);
}

void WebAppUninstallDialogViews::OnIconsRead(
    std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (parent_ && parent_window_tracker_->WasNativeWindowClosed()) {
    CallCallback(/*uninstalled=*/false);
    return;
  }

  view_ = new WebAppUninstallDialogDelegateView(profile_, this, app_id_,
                                                std::move(icon_bitmaps));

  constrained_window::CreateBrowserModalDialogViews(view_, parent_)->Show();

  if (dialog_shown_callback_for_testing_)
    std::move(dialog_shown_callback_for_testing_).Run();

  // This should be a tail call because it destroys |this|:
  view_->ProcessAutoConfirmValue();
}

void WebAppUninstallDialogViews::OnWebAppUninstalled(
    const web_app::AppId& app_id) {
  // Handle the case when web app was uninstalled externally and we have to
  // cancel current dialog.
  if (app_id == app_id_ && view_)
    view_->CancelDialog();
}

void WebAppUninstallDialogViews::OnAppRegistrarDestroyed() {
  registrar_observer_.RemoveAll();
  if (view_)
    view_->CancelDialog();
}

void WebAppUninstallDialogViews::UninstallStarted() {
  // Next OnWebAppUninstalled should be ignored. Unsubscribe:
  registrar_observer_.RemoveAll();
}

void WebAppUninstallDialogViews::CallCallback(bool uninstalled) {
  view_ = nullptr;
  std::move(closed_callback_).Run(uninstalled);
}

// static
std::unique_ptr<web_app::WebAppUninstallDialog>
web_app::WebAppUninstallDialog::Create(Profile* profile,
                                       gfx::NativeWindow parent) {
  return std::make_unique<WebAppUninstallDialogViews>(profile, parent);
}
