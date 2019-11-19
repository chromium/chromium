// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/ui/payments/local_card_migration_bubble_controller.h"
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
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const int kMigrationBubbleGooglePayLogoWidth = 40;
#endif
const int kMigrationBubbleGooglePayLogoHeight = 16;
}  // namespace

LocalCardMigrationBubbleViews::LocalCardMigrationBubbleViews(
    views::View* anchor_view,
    content::WebContents* web_contents,
    LocalCardMigrationBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  DCHECK(controller);
  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_OK);
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_BUBBLE_BUTTON_LABEL));
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

gfx::Size LocalCardMigrationBubbleViews::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
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
      gfx::CreateVectorIcon(kGooglePayLogoIcon,
                            GetNativeTheme()->ShouldUseDarkColors()
                                ? gfx::kGoogleGrey200
                                : gfx::kGoogleGrey700),
      /*x=*/0, /*y=*/0, kMigrationBubbleGooglePayLogoWidth,
      kMigrationBubbleGooglePayLogoHeight);
#else
  gfx::ImageSkia image = gfx::CreateVectorIcon(
      kCreditCardIcon, kMigrationBubbleGooglePayLogoHeight,
      GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_DefaultIconColor));
#endif
  views::ImageView* icon_view = new views::ImageView();
  icon_view->SetImage(image);
  icon_view->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  icon_view->SetAccessibleName(
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

bool LocalCardMigrationBubbleViews::ShouldShowCloseButton() const {
  return true;
}

base::string16 LocalCardMigrationBubbleViews::GetWindowTitle() const {
  return controller_ ? l10n_util::GetStringUTF16(
                           IDS_AUTOFILL_LOCAL_CARD_MIGRATION_BUBBLE_TITLE)
                     : base::string16();
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
  auto* explanatory_message =
      new views::Label(l10n_util::GetStringUTF16(
                           IDS_AUTOFILL_LOCAL_CARD_MIGRATION_BUBBLE_BODY_TEXT),
                       CONTEXT_BODY_TEXT_LARGE, views::style::STYLE_SECONDARY);
  explanatory_message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  explanatory_message->SetMultiLine(true);
  AddChildView(explanatory_message);
  SetID(DialogViewId::MAIN_CONTENT_VIEW_MIGRATION_BUBBLE);
}

}  // namespace autofill
