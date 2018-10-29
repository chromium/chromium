// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/local_card_migration_bubble_views.h"

#include <stddef.h>
#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/autofill/dialog_view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/ui/local_card_migration_bubble_controller.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"

namespace autofill {

namespace {
const int kMigrationBubbleGooglePayLogoWidth = 40;
const int kMigrationBubbleGooglePayLogoHeight = 16;
}  // namespace

LocalCardMigrationBubbleViews::LocalCardMigrationBubbleViews(
    views::View* anchor_view,
    const gfx::Point& anchor_point,
    content::WebContents* web_contents,
    LocalCardMigrationBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, anchor_point, web_contents),
      controller_(controller) {
  DCHECK(controller);
}

void LocalCardMigrationBubbleViews::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void LocalCardMigrationBubbleViews::Hide() {
  // If |controller_| is null, WindowClosing() won't invoke OnBubbleClosed(), so
  // do that here. This will clear out |controller_|'s reference to |this|. Note
  // that WindowClosing() happens only after the _asynchronous_ Close() task
  // posted in CloseBubble() completes, but we need to fix references sooner.
  if (controller_)
    controller_->OnBubbleClosed();
  controller_ = nullptr;
  CloseBubble();
}

bool LocalCardMigrationBubbleViews::Accept() {
  if (controller_)
    controller_->OnConfirmButtonClicked();
  return true;
}

bool LocalCardMigrationBubbleViews::Cancel() {
  if (controller_)
    controller_->OnCancelButtonClicked();
  return true;
}

bool LocalCardMigrationBubbleViews::Close() {
  return true;
}

int LocalCardMigrationBubbleViews::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

base::string16 LocalCardMigrationBubbleViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  // TODO(crbug.com/859254): Update OK button label once mock is finalized.
  return l10n_util::GetStringUTF16(
      button == ui::DIALOG_BUTTON_OK
          ? IDS_AUTOFILL_LOCAL_CARD_MIGRATION_BUBBLE_BUTTON_LABEL
          : IDS_NO_THANKS);
}

gfx::Size LocalCardMigrationBubbleViews::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void LocalCardMigrationBubbleViews::AddedToWidget() {
  auto title_container = std::make_unique<views::View>();
  title_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RELATED_CONTROL_VERTICAL_SMALL)));
  gfx::ImageSkia image = gfx::ImageSkiaOperations::CreateTiledImage(
      gfx::CreateVectorIcon(kGooglePayLogoIcon, gfx::kPlaceholderColor),
      /*x=*/0, /*y=*/0, kMigrationBubbleGooglePayLogoWidth,
      kMigrationBubbleGooglePayLogoHeight);
  views::ImageView* icon_view = new views::ImageView();
  icon_view->SetImage(&image);
  icon_view->SetHorizontalAlignment(views::ImageView::LEADING);
  icon_view->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_GOOGLE_PAY_LOGO_ACCESSIBLE_NAME));
  title_container->AddChildView(icon_view);

  auto* title = new views::Label(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_LOCAL_CARD_MIGRATION_BUBBLE_TITLE),
      views::style::CONTEXT_DIALOG_TITLE);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetMultiLine(true);
  title_container->AddChildView(title);

  GetBubbleFrameView()->SetTitleView(std::move(title_container));
}

bool LocalCardMigrationBubbleViews::ShouldShowCloseButton() const {
  return true;
}

void LocalCardMigrationBubbleViews::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

LocalCardMigrationBubbleViews::~LocalCardMigrationBubbleViews() {}

void LocalCardMigrationBubbleViews::Init() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto* explanatory_message = new views::Label(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_BUBBLE_BODY_TEXT),
      CONTEXT_BODY_TEXT_LARGE, ChromeTextStyle::STYLE_SECONDARY);
  explanatory_message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  explanatory_message->SetMultiLine(true);
  AddChildView(explanatory_message);
}

}  // namespace autofill
