// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_dialog/app_dialog_view.h"

#include <memory>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

AppDialogView::AppDialogView(const gfx::ImageSkia& image)
    : BubbleDialogDelegateView(nullptr, views::BubbleBorder::NONE) {
  SetIcon(image);
  SetShowIcon(true);
  SetShowCloseButton(false);
  SetModalType(ui::MODAL_TYPE_SYSTEM);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
}

AppDialogView::~AppDialogView() = default;

void AppDialogView::InitializeView(const base::string16& heading_text) {
  SetButtons(ui::DIALOG_BUTTON_OK);
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  label_ = AddChildView(std::make_unique<views::Label>(heading_text));
  label_->SetMultiLine(true);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetAllowCharacterBreak(true);
}

void AppDialogView::SetLabelText(const base::string16& text) {
  DCHECK(label_);
  label_->SetText(text);
}
