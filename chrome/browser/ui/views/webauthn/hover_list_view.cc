// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/hover_list_view.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/webauthn/webauthn_hover_button.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace {

std::unique_ptr<WebAuthnHoverButton> CreateHoverButtonForListItem(
    const ui::ImageModel& icon,
    std::u16string item_title,
    std::u16string item_description,
    bool enabled,
    views::Button::PressedCallback callback) {
  constexpr int kChevronSize = 20;
  auto secondary_view =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kSubmenuArrowChromeRefreshIcon,
          enabled ? ui::kColorIcon : ui::kColorIconDisabled, kChevronSize));

  const int kIconSize = 24;
  auto item_image = std::make_unique<views::ImageView>(icon);
  item_image->SetImageSize(gfx::Size(kIconSize, kIconSize));

  return std::make_unique<WebAuthnHoverButton>(
      std::move(callback), std::move(item_image), item_title, item_description,
      std::move(secondary_view), enabled);
}

}  // namespace

// HoverListView ---------------------------------------------------------

HoverListView::HoverListView(std::unique_ptr<HoverListModel> model)
    : model_(std::move(model)) {
  DCHECK(model_);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto item_container = std::make_unique<views::View>();
  item_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      0 /* betweeen_child_spacing */));

  item_container_ = item_container.get();
  item_container_->AddChildView(std::make_unique<views::Separator>());

  for (const auto item_tag : model_->GetButtonTags()) {
    AppendListItemView(model_->GetItemIcon(item_tag),
                       model_->GetItemText(item_tag),
                       model_->GetDescriptionText(item_tag),
                       model_->IsButtonEnabled(item_tag), item_tag);
  }

  scroll_view_ = new views::ScrollView();
  scroll_view_->SetContents(std::move(item_container));
  AddChildView(scroll_view_.get());
  scroll_view_->ClipHeightTo(GetPreferredViewHeight(),
                             GetPreferredViewHeight());
}

HoverListView::~HoverListView() = default;

void HoverListView::AppendListItemView(const ui::ImageModel& icon,
                                       std::u16string item_text,
                                       std::u16string description_text,
                                       bool enabled,
                                       int item_tag) {
  auto hover_button = CreateHoverButtonForListItem(
      icon, item_text, description_text, enabled,
      base::BindRepeating(&HoverListModel::OnListItemSelected,
                          base::Unretained(model_.get()), item_tag));

  auto* list_item_view_ptr = hover_button.release();
  item_container_->AddChildView(list_item_view_ptr);
  auto* separator =
      item_container_->AddChildView(std::make_unique<views::Separator>());
  tags_to_list_item_views_.emplace(
      item_tag, ListItemViews{list_item_view_ptr, separator});
}

views::Button& HoverListView::GetTopListItemView() const {
  DCHECK(!tags_to_list_item_views_.empty());
  return *tags_to_list_item_views_.begin()->second.item_view;
}

void HoverListView::RequestFocus() {
  if (tags_to_list_item_views_.empty()) {
    return;
  }

  GetTopListItemView().RequestFocus();
}

int HoverListView::GetPreferredViewHeight() const {
  constexpr int kMaxViewHeight = 300;

  // |item_container_| has one separator at the top and list items which
  // contain one separator and one hover button.
  const auto separator_height = views::Separator().GetPreferredSize().height();
  int size = separator_height;
  for (const auto& iter : tags_to_list_item_views_) {
    size +=
        iter.second.item_view->GetPreferredSize().height() + separator_height;
  }
  int reserved_items =
      model_->GetPreferredItemCount() - tags_to_list_item_views_.size();
  if (reserved_items > 0) {
    auto dummy_hover_button = CreateHoverButtonForListItem(
        ui::ImageModel(), std::u16string(), std::u16string(),
        /*enabled=*/true, views::Button::PressedCallback());
    const auto list_item_height =
        separator_height + dummy_hover_button->GetPreferredSize().height();
    size += list_item_height * reserved_items;
  }
  return std::min(kMaxViewHeight, size);
}

BEGIN_METADATA(HoverListView)
ADD_READONLY_PROPERTY_METADATA(int, PreferredViewHeight)
END_METADATA
