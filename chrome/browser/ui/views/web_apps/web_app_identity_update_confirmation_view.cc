// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_identity_update_confirmation_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/web_apps/web_app_uninstall_dialog_view.h"
#include "chrome/browser/web_applications/components/web_app_callback_app_identity.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/style/typography.h"

namespace {
const int kArrowIconSizeDp = 32;

bool g_auto_accept_app_identity_update_for_testing = false;

}  // namespace

WebAppIdentityUpdateConfirmationView::~WebAppIdentityUpdateConfirmationView() =
    default;

WebAppIdentityUpdateConfirmationView::WebAppIdentityUpdateConfirmationView(
    const std::string& app_id,
    bool title_change,
    bool icon_change,
    const std::u16string& old_title,
    const std::u16string& new_title,
    const SkBitmap& old_icon,
    const SkBitmap& new_icon,
    content::WebContents* web_contents,
    web_app::AppIdentityDialogCallback callback)
    : app_id_(app_id),
      callback_(std::move(callback)),
      web_contents_(web_contents) {
  DCHECK(!callback_.is_null());

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(IDS_WEBAPP_UPDATE_NEGATIVE_BUTTON));
  SetModalType(ui::MODAL_TYPE_WINDOW);
  DCHECK(title_change || icon_change);
  SetTitle(title_change
               ? (icon_change ? IDS_WEBAPP_UPDATE_DIALOG_TITLE_NAME_AND_ICON
                              : IDS_WEBAPP_UPDATE_DIALOG_TITLE_NAME)
               : IDS_WEBAPP_UPDATE_DIALOG_TITLE_ICON);

  SetAcceptCallback(
      base::BindOnce(&WebAppIdentityUpdateConfirmationView::OnDialogAccepted,
                     base::Unretained(this)));

  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  set_margins(layout_provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kControl));
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  // The headline column set is simply a single column that fills up the row.
  constexpr int kColumnSetIdHeadline = 0;
  views::ColumnSet* column_set_headline =
      layout->AddColumnSet(kColumnSetIdHeadline);
  column_set_headline->AddColumn(
      views::GridLayout::FILL, views::GridLayout::CENTER,
      views::GridLayout::kFixedSize,
      views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  // The main column set is |padding|col1|padding|arrow|padding|col2|padding|,
  // where col1 and col2 contain the 'before' and 'after' values (either text or
  // icon) and the first and last columns grow as needed to fill upp the rest.
  constexpr int kColumnSetIdMain = 1;
  views::ColumnSet* column_set_main = layout->AddColumnSet(kColumnSetIdMain);

  // Padding column on the far left side of the dialog. Grows as needed to keep
  // the views centered.
  column_set_main->AddPaddingColumn(/*resize_percent= */ 100, /* width= */ 0);
  // Column showing the 'before' icon/text.
  column_set_main->AddColumn(views::GridLayout::CENTER,
                             views::GridLayout::CENTER,
                             views::GridLayout::kFixedSize,
                             views::GridLayout::ColumnSize::kUsePreferred,
                             /* fixed_width= */ 0, /* min_width= */ 0);
  // Padding between the left side and the arrow.
  column_set_main->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  // Column showing the arrow to indicate what is before and what is after.
  column_set_main->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                             views::GridLayout::kFixedSize,
                             views::GridLayout::ColumnSize::kUsePreferred,
                             /* fixed_width= */ 0,
                             /* min_width= */ 0);
  // Padding between the arrow and the right side.
  column_set_main->AddPaddingColumn(
      views::GridLayout::FILL, layout_provider->GetDistanceMetric(
                                   views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  // Column showing the 'after' icon/text.
  column_set_main->AddColumn(views::GridLayout::CENTER,
                             views::GridLayout::CENTER,
                             views::GridLayout::kFixedSize,
                             views::GridLayout::ColumnSize::kUsePreferred,
                             /* fixed_width= */ 0, /* min_width= */ 0);
  // Padding column on the far right side of the dialog. Grows as needed to keep
  // the views centered.
  column_set_main->AddPaddingColumn(/*resize_percent= */ 100, /* width= */ 0);

  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetIdHeadline);

  auto message_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_WEBAPP_UPDATE_EXPLANATION),
      views::style::CONTEXT_LABEL);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_label->SetMultiLine(true);
  message_label->SizeToFit(fixed_width());
  layout->AddView(std::move(message_label));

  layout->AddPaddingRow(
      views::GridLayout::kFixedSize,
      2 * layout_provider->GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL));

  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetIdMain);

  auto old_icon_image_view = std::make_unique<views::ImageView>();
  gfx::Size image_size(web_app::kWebAppIconSmall, web_app::kWebAppIconSmall);
  old_icon_image_view->SetImageSize(image_size);
  old_icon_image_view->SetImage(gfx::ImageSkia::CreateFrom1xBitmap(old_icon));
  old_icon_image_view->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_WEBAPP_UPDATE_CURRENT_ICON));
  layout->AddView(std::move(old_icon_image_view));

  auto arrow =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          kKeyboardArrowRightIcon, ui::NativeTheme::kColorId_DefaultIconColor,
          kArrowIconSizeDp));
  layout->AddView(std::move(arrow));

  auto new_icon_image_view = std::make_unique<views::ImageView>();
  new_icon_image_view->SetImageSize(image_size);
  new_icon_image_view->SetImage(gfx::ImageSkia::CreateFrom1xBitmap(new_icon));
  new_icon_image_view->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_WEBAPP_UPDATE_NEW_ICON));
  layout->AddView(std::move(new_icon_image_view));

  layout->AddPaddingRow(
      views::GridLayout::kFixedSize,
      layout_provider->GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL));

  auto old_title_label =
      std::make_unique<views::Label>(old_title, views::style::CONTEXT_LABEL);
  auto new_title_label =
      std::make_unique<views::Label>(new_title, views::style::CONTEXT_LABEL);
  old_title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  new_title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  old_title_label->SetMultiLine(true);
  new_title_label->SetMultiLine(true);

  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetIdMain);
  layout->AddView(std::move(old_title_label));
  layout->SkipColumns(1);
  layout->AddView(std::move(new_title_label));

  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::APP_IDENTITY_UPDATE_CONFIRMATION);
}

bool WebAppIdentityUpdateConfirmationView::ShouldShowCloseButton() const {
  return false;
}

void WebAppIdentityUpdateConfirmationView::OnDialogAccepted() {
  std::move(callback_).Run(web_app::AppIdentityUpdate::kAllowed);
}

bool WebAppIdentityUpdateConfirmationView::Cancel() {
  uninstall_dialog_ = std::make_unique<WebAppUninstallDialogViews>(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
      web_contents_->GetTopLevelNativeWindow());
  uninstall_dialog_->ConfirmUninstall(
      app_id_, webapps::WebappUninstallSource::kAppMenu, base::DoNothing());
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
  auto* dialog = new WebAppIdentityUpdateConfirmationView(
      app_id, title_change, icon_change, old_title, new_title, old_icon,
      new_icon, web_contents, std::move(callback));
  views::Widget* dialog_widget =
      constrained_window::CreateBrowserModalDialogViews(
          dialog, web_contents->GetTopLevelNativeWindow());
  dialog_widget->Show();

  if (g_auto_accept_app_identity_update_for_testing) {
    dialog->AcceptDialog();
  }
}

void SetAutoAcceptAppIdentityUpdateForTesting(bool auto_accept) {
  g_auto_accept_app_identity_update_for_testing = auto_accept;
}

}  // namespace chrome
