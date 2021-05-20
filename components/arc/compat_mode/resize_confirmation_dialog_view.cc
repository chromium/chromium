// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/resize_confirmation_dialog_view.h"

#include <memory>

#include "base/bind.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"

namespace arc {

namespace {

std::unique_ptr<views::View> MakeOverlayDialogContainerView(
    std::unique_ptr<views::View> dialog_view) {
  constexpr SkColor kScrimColor = SkColorSetA(gfx::kGoogleGrey900, 0x99);

  auto container = views::Builder<views::FlexLayoutView>()
                       .SetInteriorMargin(gfx::Insets(0, 32))
                       .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
                       .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
                       .SetBackground(views::CreateSolidBackground(kScrimColor))
                       .Build();
  dialog_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero));

  container->AddChildView(std::move(dialog_view));

  return container;
}

}  // namespace

ResizeConfirmationDialogView::ResizeConfirmationDialogView(
    ResizeConfirmationCallback callback)
    : callback_(std::move(callback)) {
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);
  SetInsideBorderInsets(gfx::Insets(24, 24, 20, 24));
  SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL));

  constexpr int kCornerRadius = 12;
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW,
      GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_DialogBackground));
  border->SetCornerRadius(kCornerRadius);
  SetBackground(std::make_unique<views::BubbleBackground>(border.get()));
  SetBorder(std::move(border));

  AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_ASH_ARC_APP_COMPAT_RESIZE_CONFIRM_TITLE))
          .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
          .SetMultiLine(true)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetAllowCharacterBreak(true)
          .SetFontList(views::style::GetFont(
                           views::style::TextContext::CONTEXT_DIALOG_TITLE,
                           views::style::TextStyle::STYLE_PRIMARY)
                           .DeriveWithWeight(gfx::Font::Weight::MEDIUM))
          .Build());

  AddChildView(MakeContentsView());
  AddChildView(MakeButtonsView());
}

ResizeConfirmationDialogView::~ResizeConfirmationDialogView() = default;

gfx::Size ResizeConfirmationDialogView::CalculatePreferredSize() const {
  gfx::Size size = views::View::CalculatePreferredSize();

  views::LayoutProvider* provider = views::LayoutProvider::Get();
  size.set_width(provider->GetDistanceMetric(
      views::DistanceMetric::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  return size;
}

std::unique_ptr<views::View> ResizeConfirmationDialogView::MakeContentsView() {
  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetBetweenChildSpacing(19)
      .SetProperty(views::kMarginsKey, gfx::Insets(0, 0, 23, 0))
      .AddChildren(
          {views::Builder<views::Label>()
               .SetText(l10n_util::GetStringUTF16(
                   IDS_ASH_ARC_APP_COMPAT_RESIZE_CONFIRM_BODY))
               .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
               .SetHorizontalAlignment(gfx::ALIGN_LEFT)
               .SetMultiLine(true),
           views::Builder<views::Checkbox>()
               .CopyAddressTo(&do_not_ask_checkbox_)
               .SetText(l10n_util::GetStringUTF16(
                   IDS_ASH_ARC_APP_COMPAT_RESIZE_CONFIRM_DONT_ASK_ME))})
      .Build();
}

std::unique_ptr<views::View> ResizeConfirmationDialogView::MakeButtonsView() {
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
      .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd)
      .SetBetweenChildSpacing(provider->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_RELATED_BUTTON_HORIZONTAL))
      .AddChildren({views::Builder<views::MdTextButton>()  // Cancel button.
                        .CopyAddressTo(&cancel_button_)
                        .SetCallback(base::BindRepeating(
                            &ResizeConfirmationDialogView::OnButtonClicked,
                            base::Unretained(this), false))
                        .SetText(l10n_util::GetStringUTF16(IDS_APP_CANCEL))
                        .SetProminent(false)
                        .SetIsDefault(false),
                    views::Builder<views::MdTextButton>()  // Accept button.
                        .CopyAddressTo(&accept_button_)
                        .SetCallback(base::BindRepeating(
                            &ResizeConfirmationDialogView::OnButtonClicked,
                            base::Unretained(this), true))
                        .SetText(l10n_util::GetStringUTF16(
                            IDS_ASH_ARC_APP_COMPAT_RESIZE_CONFIRM_ACCEPT))
                        .SetProminent(true)
                        .SetIsDefault(true)})
      .Build();
}

void ResizeConfirmationDialogView::OnButtonClicked(bool accept) {
  DCHECK(callback_);
  std::move(callback_).Run(accept, do_not_ask_checkbox_->GetChecked());
}

void ShowResizeConfirmationDialog(aura::Window* parent,
                                  ResizeConfirmationCallback callback) {
  auto* shell_surface_base = exo::GetShellSurfaceBaseForWindow(parent);
  if (!shell_surface_base || shell_surface_base->HasOverlay())
    return;

  auto remove_overlay = base::BindOnce(
      [](aura::Window* window) {
        auto* shell_surface_base = exo::GetShellSurfaceBaseForWindow(window);
        if (shell_surface_base && shell_surface_base->HasOverlay())
          shell_surface_base->RemoveOverlay();
      },
      base::Unretained(parent));
  exo::ShellSurfaceBase::OverlayParams params(MakeOverlayDialogContainerView(
      std::make_unique<ResizeConfirmationDialogView>(
          std::move(callback).Then(std::move(remove_overlay)))));
  params.translucent = true;
  shell_surface_base->AddOverlay(std::move(params));
}

}  // namespace arc
