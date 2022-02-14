// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_identity_update_confirmation_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/web_apps/web_app_uninstall_dialog_view.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/vector_icons/vector_icons.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/style/typography.h"

namespace {
const int kArrowIconSizeDp = 32;

// The width of the columns left and right of the arrow (containing the name
// of the app (before and after).
const int kNameColumnWidth = 170;

// Keeps track of whether the testing code has set an action to be performed
// when the dialog is set to show (and what that action should be: true = accept
// the dialog, false = do not accept).
absl::optional<bool> g_auto_resolve_app_identity_update_dialog_for_testing;

}  // namespace

WebAppIdentityUpdateConfirmationView::~WebAppIdentityUpdateConfirmationView() =
    default;

WebAppIdentityUpdateConfirmationView::WebAppIdentityUpdateConfirmationView(
    Profile* profile,
    const std::string& app_id,
    bool title_change,
    bool icon_change,
    const std::u16string& old_title,
    const std::u16string& new_title,
    const SkBitmap& old_icon,
    const SkBitmap& new_icon,
    web_app::AppIdentityDialogCallback callback)
    : profile_(profile), app_id_(app_id), callback_(std::move(callback)) {
  DCHECK(!callback_.is_null());
  DCHECK(title_change || icon_change);

  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int distance_related_horizontal = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  const gfx::Size image_size(web_app::kWebAppIconSmall,
                             web_app::kWebAppIconSmall);

  views::Builder<WebAppIdentityUpdateConfirmationView>(this)
      .set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH))
      .SetButtonLabel(
          ui::DIALOG_BUTTON_CANCEL,
          l10n_util::GetStringUTF16(IDS_WEBAPP_UPDATE_NEGATIVE_BUTTON))
      .SetModalType(ui::MODAL_TYPE_WINDOW)
      .SetTitle(title_change
                    ? (icon_change
                           ? IDS_WEBAPP_UPDATE_DIALOG_TITLE_NAME_AND_ICON
                           : IDS_WEBAPP_UPDATE_DIALOG_TITLE_NAME)
                    : IDS_WEBAPP_UPDATE_DIALOG_TITLE_ICON)
      .SetAcceptCallback(base::BindOnce(
          &WebAppIdentityUpdateConfirmationView::OnDialogAccepted,
          weak_factory_.GetWeakPtr()))
      .set_margins(layout_provider->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kControl))
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(),
          2 * layout_provider->GetDistanceMetric(
                  DISTANCE_CONTROL_LIST_VERTICAL)))
      .AddChildren(
          views::Builder<views::Label>()
              .SetTextContext(views::style::CONTEXT_LABEL)
              .SetText(l10n_util::GetStringUTF16(IDS_WEBAPP_UPDATE_EXPLANATION))
              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
              .SetMultiLine(true),
          views::Builder<views::TableLayoutView>()
              .AddPaddingColumn(100, 0)
              .AddColumn(views::LayoutAlignment::kCenter,
                         views::LayoutAlignment::kStart,
                         views::TableLayout::kFixedSize,
                         views::TableLayout::ColumnSize::kUsePreferred,
                         kNameColumnWidth, 0)
              .AddPaddingColumn(views::TableLayout::kFixedSize,
                                distance_related_horizontal)
              .AddColumn(views::LayoutAlignment::kStretch,
                         views::LayoutAlignment::kCenter,
                         views::TableLayout::kFixedSize,
                         views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
              .AddPaddingColumn(views::TableLayout::kFixedSize,
                                distance_related_horizontal)
              .AddColumn(views::LayoutAlignment::kCenter,
                         views::LayoutAlignment::kStart,
                         views::TableLayout::kFixedSize,
                         views::TableLayout::ColumnSize::kUsePreferred,
                         kNameColumnWidth, 0)
              .AddPaddingColumn(100, 0)
              .AddRows(1, views::TableLayout::kFixedSize, 0)
              .AddChildren(
                  views::Builder<views::ImageView>()
                      .SetImageSize(image_size)
                      .SetImage(gfx::ImageSkia::CreateFrom1xBitmap(old_icon))
                      .SetAccessibleName(l10n_util::GetStringUTF16(
                          IDS_WEBAPP_UPDATE_CURRENT_ICON)),
                  views::Builder<views::ImageView>().SetImage(
                      ui::ImageModel::FromVectorIcon(
                          vector_icons::kForwardArrowIcon, ui::kColorIcon,
                          kArrowIconSizeDp)),
                  views::Builder<views::ImageView>()
                      .SetImageSize(image_size)
                      .SetImage(gfx::ImageSkia::CreateFrom1xBitmap(new_icon))
                      .SetAccessibleName(l10n_util::GetStringUTF16(
                          IDS_WEBAPP_UPDATE_NEW_ICON)))
              .AddPaddingRow(views::TableLayout::kFixedSize,
                             layout_provider->GetDistanceMetric(
                                 DISTANCE_CONTROL_LIST_VERTICAL))
              .AddRows(1, views::TableLayout::kFixedSize, 0)
              .AddChildren(
                  views::Builder<views::Label>()
                      .SetTextContext(views::style::CONTEXT_LABEL)
                      .SetText(old_title)
                      .SetMultiLine(true)
                      .SetHorizontalAlignment(gfx::ALIGN_CENTER)
                      .SizeToFit(kNameColumnWidth),
                  views::Builder<views::View>(),  // Skip the center column.
                  views::Builder<views::Label>()
                      .SetTextContext(views::style::CONTEXT_LABEL)
                      .SetText(new_title)
                      .SetMultiLine(true)
                      .SetHorizontalAlignment(gfx::ALIGN_CENTER)
                      .SizeToFit(kNameColumnWidth)))
      .BuildChildren();

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);
  DCHECK(provider);
  install_manager_observation_.Observe(&provider->install_manager());

  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::APP_IDENTITY_UPDATE_CONFIRMATION);
}

void WebAppIdentityUpdateConfirmationView::OnWebAppWillBeUninstalled(
    const web_app::AppId& app_id) {
  if (app_id == app_id_)
    GetWidget()->Close();
}

void WebAppIdentityUpdateConfirmationView::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
  GetWidget()->Close();
}

bool WebAppIdentityUpdateConfirmationView::ShouldShowCloseButton() const {
  return false;
}

void WebAppIdentityUpdateConfirmationView::OnDialogAccepted() {
  std::move(callback_).Run(web_app::AppIdentityUpdate::kAllowed);
}

void WebAppIdentityUpdateConfirmationView::OnWebAppUninstallDialogClosed(
    bool uninstalled) {
  if (uninstalled)
    GetWidget()->Close();  // An uninstall is already in progress.
}

bool WebAppIdentityUpdateConfirmationView::Cancel() {
  uninstall_dialog_ = std::make_unique<WebAppUninstallDialogViews>(
      profile_, GetWidget()->GetNativeWindow());
  uninstall_dialog_->ConfirmUninstall(
      app_id_, webapps::WebappUninstallSource::kAppMenu,
      base::BindOnce(
          &WebAppIdentityUpdateConfirmationView::OnWebAppUninstallDialogClosed,
          weak_factory_.GetWeakPtr()));
  return false;
}

BEGIN_METADATA(WebAppIdentityUpdateConfirmationView, views::DialogDelegateView)
END_METADATA

namespace chrome {

void ShowWebAppIdentityUpdateDialog(
    const std::string& app_id,
    bool title_change,
    bool icon_change,
    const std::u16string& old_title,
    const std::u16string& new_title,
    const SkBitmap& old_icon,
    const SkBitmap& new_icon,
    content::WebContents* web_contents,
    web_app::AppIdentityDialogCallback callback) {
  if (g_auto_resolve_app_identity_update_dialog_for_testing &&
      *g_auto_resolve_app_identity_update_dialog_for_testing == false) {
    std::move(callback).Run(web_app::AppIdentityUpdate::kSkipped);
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* dialog = new WebAppIdentityUpdateConfirmationView(
      profile, app_id, title_change, icon_change, old_title, new_title,
      old_icon, new_icon, std::move(callback));
  views::Widget* dialog_widget =
      constrained_window::CreateBrowserModalDialogViews(
          dialog, web_contents->GetTopLevelNativeWindow());
  dialog_widget->Show();

  if (g_auto_resolve_app_identity_update_dialog_for_testing &&
      *g_auto_resolve_app_identity_update_dialog_for_testing) {
    dialog->AcceptDialog();
  }
}

void SetAutoAcceptAppIdentityUpdateForTesting(bool auto_accept) {
  g_auto_resolve_app_identity_update_dialog_for_testing = auto_accept;
}

}  // namespace chrome
