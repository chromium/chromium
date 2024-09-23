// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/local_card_migration_error_dialog_view.h"

#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_dialog_factory.h"
#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace autofill {

LocalCardMigrationErrorDialogView::LocalCardMigrationErrorDialogView(
    LocalCardMigrationDialogController* controller)
    : controller_(controller) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetCancelCallback(
      base::BindOnce(&LocalCardMigrationDialogController::OnDoneButtonClicked,
                     base::Unretained(controller_)));

  // The error dialog should be a modal dialog blocking the whole browser
  // which is consistent with other dialogs. It should make sure that the
  // user can see the error message.
  SetModalType(ui::mojom::ModalType::kWindow);
  SetShowCloseButton(false);
  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH));

  set_close_on_deactivate(false);
  set_margins(gfx::Insets());
}

LocalCardMigrationErrorDialogView::~LocalCardMigrationErrorDialogView() {
  if (controller_) {
    controller_->OnDialogClosed();
    controller_ = nullptr;
  }
}

void LocalCardMigrationErrorDialogView::ShowDialog(
    content::WebContents& web_contents) {
  Init();
  constrained_window::CreateBrowserModalDialogViews(
      this, web_contents.GetTopLevelNativeWindow())
      ->Show();
}

void LocalCardMigrationErrorDialogView::CloseDialog() {
  GetWidget()->Close();
  if (controller_) {
    controller_->OnDialogClosed();
    controller_ = nullptr;
  }
}

void LocalCardMigrationErrorDialogView::Init() {
  if (!children().empty()) {
    return;
  }

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kMigrationDialogMainContainerChildSpacing));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  auto* image = new views::ImageView();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  image->SetImage(
      rb.GetImageSkiaNamed(GetNativeTheme()->ShouldUseDarkColors()
                               ? IDR_AUTOFILL_MIGRATION_DIALOG_HEADER_DARK
                               : IDR_AUTOFILL_MIGRATION_DIALOG_HEADER));
  image->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_GOOGLE_PAY_LOGO_ACCESSIBLE_NAME));
  AddChildView(image);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  auto* error_view = new views::View();
  auto* horizontal_layout =
      error_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          provider->GetDistanceMetric(
              views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  horizontal_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  error_view->SetBorder(views::CreateEmptyBorder(kMigrationDialogInsets));
  auto* error_image = new views::ImageView();
  error_image->SetImage(ui::ImageModel::FromVectorIcon(
      kBrowserToolsErrorIcon, ui::kColorAlertHighSeverity));
  error_view->AddChildView(error_image);

  auto* error_message = new views::Label(
      l10n_util::GetPluralStringFUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_MESSAGE_ERROR,
          controller_->GetCardList().size()),
      views::style::CONTEXT_DIALOG_BODY_TEXT, ChromeTextStyle::STYLE_RED);
  error_view->AddChildView(error_message);

  AddChildView(error_view);
}

LocalCardMigrationDialog* CreateLocalCardMigrationErrorDialogView(
    LocalCardMigrationDialogController* controller) {
  return new LocalCardMigrationErrorDialogView(controller);
}

BEGIN_METADATA(LocalCardMigrationErrorDialogView)
END_METADATA

}  // namespace autofill
