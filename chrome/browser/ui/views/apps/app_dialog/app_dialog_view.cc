// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_dialog/app_dialog_view.h"

#include <memory>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

AppDialogView::AppDialogView(const ui::ImageModel& image)
    : BubbleDialogDelegateView(nullptr, views::BubbleBorder::NONE) {
  SetIcon(image);
  SetShowIcon(true);
  SetShowCloseButton(false);
  SetModalType(ui::MODAL_TYPE_SYSTEM);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
}

AppDialogView::~AppDialogView() = default;

void AppDialogView::InitializeView(const std::u16string& heading_text) {
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

void AppDialogView::SetLabelText(const std::u16string& text) {
  DCHECK(label_);
  label_->SetText(text);
}

BEGIN_METADATA(AppDialogView, views::BubbleDialogDelegateView)
END_METADATA
