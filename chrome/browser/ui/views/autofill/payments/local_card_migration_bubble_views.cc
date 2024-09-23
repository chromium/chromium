// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/local_card_migration_bubble_views.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/ui/payments/local_card_migration_bubble_controller.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"

namespace autofill {

namespace {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const int kMigrationBubbleGooglePayLogoWidth = 40;
#endif
const int kMigrationBubbleGooglePayLogoHeight = 16;
}  // namespace

LocalCardMigrationBubbleViews::LocalCardMigrationBubbleViews(
    views::View* anchor_view,
    content::WebContents* web_contents,
    LocalCardMigrationBubbleController* controller)
    : AutofillLocationBarBubble(anchor_view, web_contents),
      controller_(controller) {
  DCHECK(controller);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_AUTOFILL_LOCAL_CARD_MIGRATION_BUBBLE_BUTTON_LABEL));
  SetCancelCallback(
      base::BindOnce(&LocalCardMigrationBubbleViews::OnDialogCancelled,
                     base::Unretained(this)));
  SetAcceptCallback(
      base::BindOnce(&LocalCardMigrationBubbleViews::OnDialogAccepted,
                     base::Unretained(this)));

  SetShowCloseButton(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

void LocalCardMigrationBubbleViews::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void LocalCardMigrationBubbleViews::Hide() {
  // If |controller_| is null, WindowClosing() won't invoke OnBubbleClosed(), so
  // do that here. This will clear out |controller_|'s reference to |this|. Note
  // that WindowClosing() happens only after the _asynchronous_ Close() task
  // posted in CloseBubble() completes, but we need to fix references sooner.
  CloseBubble();

  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

void LocalCardMigrationBubbleViews::OnDialogAccepted() {
  if (controller_) {
    controller_->OnConfirmButtonClicked();
  }
}

void LocalCardMigrationBubbleViews::OnDialogCancelled() {
  if (controller_) {
    controller_->OnCancelButtonClicked();
  }
}

void LocalCardMigrationBubbleViews::AddedToWidget() {
  auto title_container = std::make_unique<views::View>();
  title_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RELATED_CONTROL_VERTICAL_SMALL)));
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // kGooglePayLogoIcon is square, and CreateTiledImage() will clip it whereas
  // setting the icon size would rescale it incorrectly.
  gfx::ImageSkia image = gfx::ImageSkiaOperations::CreateTiledImage(
      gfx::CreateVectorIcon(
          vector_icons::kGooglePayLogoIcon,
          GetColorProvider()->GetColor(kColorPaymentsGooglePayLogo)),
      /*x=*/0, /*y=*/0, kMigrationBubbleGooglePayLogoWidth,
      kMigrationBubbleGooglePayLogoHeight);
#else
  gfx::ImageSkia image = gfx::CreateVectorIcon(
      kCreditCardIcon, kMigrationBubbleGooglePayLogoHeight,
      GetColorProvider()->GetColor(ui::kColorIcon));
#endif
  views::ImageView* icon_view = new views::ImageView();
  icon_view->SetImage(ui::ImageModel::FromImageSkia(image));
  icon_view->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  icon_view->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_GOOGLE_PAY_LOGO_ACCESSIBLE_NAME));
  title_container->AddChildView(icon_view);

  auto* title =
      new views::Label(GetWindowTitle(), views::style::CONTEXT_DIALOG_TITLE);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // Need to set title's preferred size otherwise the long title
  // would not be two-lined but would change the width of bubble.
  title->SetPreferredSize(gfx::Size(0, 0));
  title->SetMultiLine(true);
  title_container->AddChildView(title);

  GetBubbleFrameView()->SetTitleView(std::move(title_container));
}

std::u16string LocalCardMigrationBubbleViews::GetWindowTitle() const {
  return controller_ ? l10n_util::GetStringUTF16(
                           IDS_AUTOFILL_LOCAL_CARD_MIGRATION_BUBBLE_TITLE)
                     : std::u16string();
}

void LocalCardMigrationBubbleViews::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
    controller_ = nullptr;
  }
}

void LocalCardMigrationBubbleViews::OnWidgetDestroying(views::Widget* widget) {
  LocationBarBubbleDelegateView::OnWidgetDestroying(widget);
  if (!widget->IsClosed())
    return;
  DCHECK_NE(widget->closed_reason(),
            views::Widget::ClosedReason::kCancelButtonClicked);
}

LocalCardMigrationBubbleViews::~LocalCardMigrationBubbleViews() = default;

void LocalCardMigrationBubbleViews::Init() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto* explanatory_message = new views::Label(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_BUBBLE_BODY_TEXT),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);
  explanatory_message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  explanatory_message->SetMultiLine(true);
  AddChildView(explanatory_message);
  SetID(DialogViewId::MAIN_CONTENT_VIEW_MIGRATION_BUBBLE);
}

BEGIN_METADATA(LocalCardMigrationBubbleViews)
END_METADATA

}  // namespace autofill
