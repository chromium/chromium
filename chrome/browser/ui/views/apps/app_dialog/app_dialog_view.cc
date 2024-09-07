// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_dialog/app_dialog_view.h"

#include <memory>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kSubtitleBottomMargin = 4;
constexpr int kTitleTopMargin = 4;

}  // namespace

AppDialogView::AppDialogView(const ui::ImageModel& image)
    : BubbleDialogDelegateView(nullptr, views::BubbleBorder::NONE) {
  SetIcon(image);
  SetShowIcon(true);
  SetShowCloseButton(false);
  SetModalType(ui::mojom::ModalType::kSystem);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
}

AppDialogView::~AppDialogView() = default;

std::optional<std::u16string> AppDialogView::GetTitleTextForTesting() const {
  if (title_) {
    return title_->GetText();
  }
  return std::nullopt;
}

void AppDialogView::InitializeView() {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
}

void AppDialogView::AddTitle(const std::u16string& title_text) {
  title_ = AddChildView(std::make_unique<views::Label>(
      title_text, views::style::CONTEXT_DIALOG_TITLE,
      views::style::STYLE_HEADLINE_3));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetMultiLine(true);
  title_->SetProperty(views::kMarginsKey,
                      gfx::Insets::TLBR(kTitleTopMargin, 0, 0, 0));
  SetAccessibleTitle(title_text);
}

void AppDialogView::AddSubtitle(const std::u16string& subtitle_text) {
  subtitle_ = AddChildView(std::make_unique<views::Label>(
      subtitle_text, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY));
  subtitle_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  subtitle_->SetMultiLine(true);
  subtitle_->SetProperty(views::kMarginsKey,
                         gfx::Insets::TLBR(0, 0, kSubtitleBottomMargin, 0));
}

void AppDialogView::SetTitleText(const std::u16string& text) {
  CHECK(title_);
  title_->SetText(text);
}

void AppDialogView::SetSubtitleText(const std::u16string& text) {
  CHECK(subtitle_);
  subtitle_->SetText(text);
}

std::u16string AppDialogView::GetAccessibleWindowTitle() const {
  if (title_) {
    return title_->GetText();
  }
  return std::u16string();
}

BEGIN_METADATA(AppDialogView)
END_METADATA
