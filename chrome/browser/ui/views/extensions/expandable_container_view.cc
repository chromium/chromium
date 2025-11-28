// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/expandable_container_view.h"

#include <string>
#include <utility>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"

// ExpandableContainerView::DetailsView ----------------------------------------
ExpandableContainerView::DetailsView::~DetailsView() = default;

ExpandableContainerView::DetailsView::DetailsView(
    const std::u16string& details) {
  auto* layout_provider = ChromeLayoutProvider::Get();
  // Spacing between this and the "Hide Details" link.
  const int bottom_padding = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const int child_spacing = layout_provider->GetDistanceMetric(
      DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);

  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetInsideBorderInsets(gfx::Insets::TLBR(0, 0, bottom_padding, 0));
  SetBetweenChildSpacing(child_spacing);

  AddChildView(views::Builder<views::Label>()
                   .SetText(details)
                   .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                   .SetTextStyle(views::style::STYLE_SECONDARY)
                   .SetMultiLine(true)
                   .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                   .Build());
}

void ExpandableContainerView::DetailsView::SetExpanded(bool expanded) {
  if (expanded == expanded_) {
    return;
  }
  expanded_ = expanded;
  SetVisible(expanded_);
  OnPropertyChanged(&expanded_, views::PropertyEffects::kPaint);
}

bool ExpandableContainerView::DetailsView::GetExpanded() const {
  return expanded_;
}

BEGIN_METADATA(ExpandableContainerView, DetailsView)
ADD_PROPERTY_METADATA(bool, Expanded)
END_METADATA

// ExpandableContainerView -----------------------------------------------------

ExpandableContainerView::ExpandableContainerView(
    const std::u16string& details) {
  DCHECK(!details.empty());
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  details_view_ = AddChildView(std::make_unique<DetailsView>(details));
  details_view_->SetVisible(false);
  auto details_link = std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_DETAILS));
  details_link->SetCallback(base::BindRepeating(
      &ExpandableContainerView::ToggleDetailLevel, base::Unretained(this)));
  details_link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  details_link_ = AddChildView(std::move(details_link));
}

ExpandableContainerView::~ExpandableContainerView() = default;

void ExpandableContainerView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void ExpandableContainerView::ToggleDetailLevel() {
  const bool expanded = details_view_->GetExpanded();
  details_view_->SetExpanded(!expanded);
  details_link_->SetText(l10n_util::GetStringUTF16(
      expanded ? IDS_EXTENSIONS_SHOW_DETAILS : IDS_EXTENSIONS_HIDE_DETAILS));
}

BEGIN_METADATA(ExpandableContainerView)
END_METADATA
