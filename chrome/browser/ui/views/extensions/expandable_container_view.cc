// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/expandable_container_view.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/property_effects.h"

// ExpandableContainerView::DetailsView ----------------------------------------
ExpandableContainerView::DetailsView::~DetailsView() = default;

ExpandableContainerView::DetailsView::DetailsView(
    const std::u16string& visible_details,
    const std::u16string& collapsed_details)
    : visible_details_(visible_details),
      all_details_(base::StrCat({visible_details, u"\n", collapsed_details})) {
  DCHECK(!visible_details.empty());
  DCHECK(!collapsed_details.empty());
  auto* layout_provider = ChromeLayoutProvider::Get();
  // Spacing between this and the "Hide Details" link.
  const int bottom_padding = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const int child_spacing = layout_provider->GetDistanceMetric(
      DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);

  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetInsideBorderInsets(gfx::Insets::TLBR(0, 0, bottom_padding, 0));
  SetBetweenChildSpacing(child_spacing);

  label_ =
      AddChildView(views::Builder<views::Label>()
                       .SetText(visible_details_)
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
  label_->SetText(expanded_ ? all_details_ : visible_details_);
  PreferredSizeChanged();
  OnPropertyChanged(&expanded_, views::PropertyEffects::kPaint);
}

bool ExpandableContainerView::DetailsView::GetExpanded() const {
  return expanded_;
}

std::u16string_view ExpandableContainerView::DetailsView::GetText() const {
  return label_->GetText();
}

BEGIN_METADATA(ExpandableContainerView, DetailsView)
ADD_PROPERTY_METADATA(bool, Expanded)
END_METADATA

// ExpandableContainerView -----------------------------------------------------

ExpandableContainerView::ExpandableContainerView(
    const std::u16string& visible_details,
    const std::u16string& collapsed_details) {
  DCHECK(!visible_details.empty());
  DCHECK(!collapsed_details.empty());
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  details_view_ = AddChildView(
      std::make_unique<DetailsView>(visible_details, collapsed_details));
  auto details_link = std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_ALL));
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
      expanded ? IDS_EXTENSIONS_SHOW_ALL : IDS_EXTENSIONS_SHOW_LESS));
}

std::u16string_view ExpandableContainerView::GetDetailsTextForTest() const {
  return details_view_->GetText();
}

BEGIN_METADATA(ExpandableContainerView)
END_METADATA
