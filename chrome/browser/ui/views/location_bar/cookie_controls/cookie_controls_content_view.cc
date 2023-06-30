// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_content_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

std::unique_ptr<views::View> CreateSeparator() {
  const int separator_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_HORIZONTAL_SEPARATOR_PADDING_PAGE_INFO_VIEW);
  int separator_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
  if (!features::IsChromeRefresh2023()) {
    // Distance for multi content list is used, but split in half, since there
    // is a separator in the middle of it. For ChromeRefresh2023, the separator
    // spacing is larger hence no need to split in half.
    separator_spacing /= 2;
  }
  auto separator = std::make_unique<views::Separator>();
  separator->SetProperty(views::kMarginsKey,
                         gfx::Insets::VH(separator_spacing, separator_padding));
  return separator;
}
}  // namespace

CookieControlsContentView::CookieControlsContentView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  AddChildView(CreateSeparator());

  AddContentLabels();
}

void CookieControlsContentView::AddContentLabels() {
  auto* provider = ChromeLayoutProvider::Get();
  const int vertical_margin =
      provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
  const int side_margin =
      provider->GetInsetsMetric(views::INSETS_DIALOG).left();

  auto* label_wrapper = AddChildView(std::make_unique<views::View>());
  label_wrapper->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  label_wrapper->SetProperty(views::kMarginsKey,
                             gfx::Insets::VH(vertical_margin, side_margin));
  title_ = label_wrapper->AddChildView(std::make_unique<views::Label>());
  title_->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  title_->SetTextStyle(views::style::STYLE_PRIMARY);
  title_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  description_ = label_wrapper->AddChildView(std::make_unique<views::Label>());
  description_->SetTextContext(views::style::CONTEXT_LABEL);
  description_->SetTextStyle(views::style::STYLE_SECONDARY);
  description_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  description_->SetMultiLine(true);
}

void CookieControlsContentView::UpdateContentLabels(
    const std::u16string& title,
    const std::u16string& description) {
  title_->SetText(title);
  description_->SetText(description);
  PreferredSizeChanged();
}

CookieControlsContentView::~CookieControlsContentView() = default;
