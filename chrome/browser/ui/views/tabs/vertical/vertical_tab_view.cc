// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"

#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_icon.h"
#include "components/browser_apis/tab_strip/tab_strip_api_data_model.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kTitleIconSpacing = 7;
constexpr int kTitleInactiveRightPadding = 11;
constexpr int kTitleHeight = 18;
constexpr int kVerticalTabHeight = 30;
// TODO(crbug.com/454686636): Determine what this min width should be.
constexpr int kVerticalTabExpandedMinWidth = 50;

class VerticalTabTitle : public views::Label {
  METADATA_HEADER(VerticalTabTitle, views::Label)
 public:
  VerticalTabTitle() {
    SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    SetElideBehavior(gfx::FADE_TAIL);
    SetHandlesTooltips(false);
    SetBackgroundColor(SK_ColorTRANSPARENT);
    SetProperty(views::kElementIdentifierKey, kVerticalTabTitleElementId);
  }
};

BEGIN_METADATA(VerticalTabTitle)
END_METADATA
}  // namespace

VerticalTabView::VerticalTabView(TabCollectionNode* collection_node)
    : collection_node_(collection_node),
      icon_(AddChildView(std::make_unique<VerticalTabIcon>(
          *collection_node_->data()->get_tab()))),
      title_(AddChildView(std::make_unique<VerticalTabTitle>())) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
  node_destroyed_subscription_ =
      collection_node_->RegisterWillDestroyCallback(base::BindOnce(
          &VerticalTabView::ResetCollectionNode, base::Unretained(this)));
  data_changed_subscription_ =
      collection_node_->RegisterDataChangedCallback(base::BindRepeating(
          &VerticalTabView::OnDataChanged, base::Unretained(this)));
  // TODO(crbug.com/444283717): Separate pinned and unpinned tabs.

  OnDataChanged();
}

VerticalTabView::~VerticalTabView() = default;

views::ProposedLayout VerticalTabView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  // TODO(crbug.com/444283717): Separate pinned and unpinned tabs.
  views::ProposedLayout layouts;

  // TODO(crbug.com/454686636): Handle collapsed state.
  const int width =
      std::max(kVerticalTabExpandedMinWidth, size_bounds.width().value_or(0));
  const int height = kVerticalTabHeight;
  layouts.host_size = gfx::Size(width, height);

  const gfx::Rect contents_rect = GetContentsBounds();

  // TabIcon calculates its preferred size by starting with the favicon size,
  // and enlarging it to fit the attention indicator and discard ring. Use its
  // preferred size instead of gfx::kFaviconSize.
  gfx::Size icon_size = icon_->GetPreferredSize();
  const int icon_padding = (height - icon_size.height()) / 2;
  const gfx::Rect icon_bounds(contents_rect.x() + icon_padding,
                              contents_rect.y() + icon_padding,
                              icon_size.width(), icon_size.height());
  layouts.child_layouts.emplace_back(icon_.get(), icon_->GetVisible(),
                                     icon_bounds);

  // kTitleIconSpacing is the space between the title and the favicon, however
  // icon_ has extra space for the attention indicator and discard ring, given
  // by its insets.
  const int title_bounds_x =
      icon_bounds.right() - icon_->GetInsets().right() + kTitleIconSpacing;
  const int title_bounds_y = contents_rect.y() + (height - kTitleHeight) / 2;
  const gfx::Rect title_bounds(
      title_bounds_x, title_bounds_y,
      width - title_bounds_x - kTitleInactiveRightPadding, kTitleHeight);
  layouts.child_layouts.emplace_back(title_.get(), title_->GetVisible(),
                                     title_bounds);
  return layouts;
}

void VerticalTabView::ResetCollectionNode() {
  collection_node_ = nullptr;
}

void VerticalTabView::OnDataChanged() {
  tabs_api::mojom::Tab tab = *collection_node_->data()->get_tab();
  icon_->SetData(tab);
  title_->SetText(base::UTF8ToUTF16(tab.title));
}

BEGIN_METADATA(VerticalTabView)
END_METADATA
