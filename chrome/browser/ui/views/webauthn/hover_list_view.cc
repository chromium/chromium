// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/hover_list_view.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/webauthn/webauthn_hover_button.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/vector_icons.h"

namespace {

enum class ItemType {
  kButton,
  kPlaceholder,
  kThrobber,
};

std::unique_ptr<WebAuthnHoverButton> CreateHoverButtonForListItem(
    const gfx::VectorIcon* vector_icon,
    std::u16string item_title,
    std::u16string item_description,
    views::Button::PressedCallback callback,
    bool is_two_line_item,
    ItemType item_type = ItemType::kButton) {
  auto item_image = std::make_unique<views::ImageView>();
  // TODO - Icon color should be set and updated in OnThemeChanged
  const SkColor icon_color = color_utils::DeriveDefaultIconColor(
      views::style::GetColor(*item_image, views::style::CONTEXT_LABEL,
                             views::style::STYLE_PRIMARY));
  if (vector_icon) {
    constexpr int kIconSize = 20;
    item_image->SetImage(
        gfx::CreateVectorIcon(*vector_icon, kIconSize, icon_color));
  }

  std::unique_ptr<views::View> secondary_view;

  switch (item_type) {
    case ItemType::kPlaceholder:
      // No secondary view in this case.
      break;

    case ItemType::kButton: {
      constexpr int kChevronSize = 8;
      auto chevron_image = std::make_unique<views::ImageView>();
      chevron_image->SetImage(gfx::CreateVectorIcon(views::kSubmenuArrowIcon,
                                                    kChevronSize, icon_color));
      secondary_view = std::move(chevron_image);
      break;
    }

    case ItemType::kThrobber: {
      auto throbber = std::make_unique<views::Throbber>();
      throbber->Start();
      secondary_view = std::move(throbber);
      // A border isn't set for kThrobber items because they are assumed to
      // always have a description.
      DCHECK(!item_description.empty());
      break;
    }
  }

  auto hover_button = std::make_unique<WebAuthnHoverButton>(
      std::move(callback), std::move(item_image), item_title, item_description,
      std::move(secondary_view), is_two_line_item);

  switch (item_type) {
    case ItemType::kPlaceholder: {
      hover_button->SetState(HoverButton::ButtonState::STATE_DISABLED);
      const auto background_color =
          hover_button->GetNativeTheme()->GetSystemColor(
              ui::NativeTheme::kColorId_BubbleBackground);
      hover_button->SetTitleTextStyle(views::style::STYLE_DISABLED,
                                      background_color);
      break;
    }

    case ItemType::kButton:
      // No extra styling.
      break;

    case ItemType::kThrobber:
      hover_button->SetState(HoverButton::ButtonState::STATE_DISABLED);
      break;
  }

  return hover_button;
}

views::Separator* AddSeparatorAsChild(views::View* view) {
  auto* separator = new views::Separator();
  separator->SetColor(gfx::kGoogleGrey300);
  view->AddChildView(separator);
  return separator;
}

}  // namespace

// HoverListView ---------------------------------------------------------

HoverListView::HoverListView(std::unique_ptr<HoverListModel> model)
    : model_(std::move(model)), is_two_line_list_(model_->StyleForTwoLines()) {
  DCHECK(model_);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto item_container = std::make_unique<views::View>();
  item_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      0 /* betweeen_child_spacing */));

  item_container_ = item_container.get();
  AddSeparatorAsChild(item_container_);

  for (const auto item_tag : model_->GetThrobberTags()) {
    auto button = CreateHoverButtonForListItem(
        model_->GetItemIcon(item_tag), model_->GetItemText(item_tag),
        model_->GetDescriptionText(item_tag),
        base::BindRepeating(&HoverListModel::OnListItemSelected,
                            base::Unretained(model_.get()), item_tag),
        true, ItemType::kThrobber);
    throbber_views_.push_back(button.get());
    item_container_->AddChildView(button.release());
    AddSeparatorAsChild(item_container_);
  }

  for (const auto item_tag : model_->GetButtonTags()) {
    AppendListItemView(model_->GetItemIcon(item_tag),
                       model_->GetItemText(item_tag),
                       model_->GetDescriptionText(item_tag), item_tag);
  }

  if (tags_to_list_item_views_.empty() &&
      model_->ShouldShowPlaceholderForEmptyList()) {
    CreateAndAppendPlaceholderItem();
  }

  scroll_view_ = new views::ScrollView();
  scroll_view_->SetContents(std::move(item_container));
  AddChildView(scroll_view_);
  scroll_view_->ClipHeightTo(GetPreferredViewHeight(),
                             GetPreferredViewHeight());

  model_->SetObserver(this);
}

HoverListView::~HoverListView() {
  model_->RemoveObserver();
}

void HoverListView::AppendListItemView(const gfx::VectorIcon* icon,
                                       std::u16string item_text,
                                       std::u16string description_text,
                                       int item_tag) {
  auto hover_button = CreateHoverButtonForListItem(
      icon, item_text, description_text,
      base::BindRepeating(&HoverListModel::OnListItemSelected,
                          base::Unretained(model_.get()), item_tag),
      is_two_line_list_);

  auto* list_item_view_ptr = hover_button.release();
  item_container_->AddChildView(list_item_view_ptr);
  auto* separator = AddSeparatorAsChild(item_container_);
  tags_to_list_item_views_.emplace(
      item_tag, ListItemViews{list_item_view_ptr, separator});
}

void HoverListView::CreateAndAppendPlaceholderItem() {
  auto placeholder_item = CreateHoverButtonForListItem(
      model_->GetPlaceholderIcon(), model_->GetPlaceholderText(),
      std::u16string(), views::Button::PressedCallback(),
      /*is_two_line_item=*/false, ItemType::kPlaceholder);
  item_container_->AddChildView(placeholder_item.get());
  auto* separator = AddSeparatorAsChild(item_container_);
  placeholder_list_item_view_.emplace(
      ListItemViews{placeholder_item.release(), separator});
}

void HoverListView::AddListItemView(int item_tag) {
  CHECK(!base::Contains(tags_to_list_item_views_, item_tag));
  if (placeholder_list_item_view_) {
    RemoveListItemView(*placeholder_list_item_view_);
    placeholder_list_item_view_.reset();
  }

  AppendListItemView(model_->GetItemIcon(item_tag),
                     model_->GetItemText(item_tag),
                     model_->GetDescriptionText(item_tag), item_tag);

  // TODO(hongjunchoi): The enclosing dialog may also need to be resized,
  // similarly to what is done in
  // AuthenticatorRequestDialogView::ReplaceSheetWith().
  Layout();
}

void HoverListView::RemoveListItemView(int item_tag) {
  auto view_it = tags_to_list_item_views_.find(item_tag);
  if (view_it == tags_to_list_item_views_.end())
    return;

  RemoveListItemView(view_it->second);
  tags_to_list_item_views_.erase(view_it);

  // Removed list item may have not been the bottom-most view in the scroll
  // view. To enforce that all remaining items are re-shifted to the top,
  // invalidate all child views.
  //
  // TODO(hongjunchoi): Restructure HoverListView and |scroll_view_| so that
  // InvalidateLayout() does not need to be explicitly called when items are
  // removed from the list. See: https://crbug.com/904968
  item_container_->InvalidateLayout();

  if (tags_to_list_item_views_.empty() &&
      model_->ShouldShowPlaceholderForEmptyList()) {
    CreateAndAppendPlaceholderItem();
  }

  // TODO(hongjunchoi): The enclosing dialog may also need to be resized,
  // similarly to what is done in
  // AuthenticatorRequestDialogView::ReplaceSheetWith().
  Layout();
}

void HoverListView::RemoveListItemView(ListItemViews list_item) {
  DCHECK(item_container_->Contains(list_item.item_view));
  DCHECK(item_container_->Contains(list_item.separator_view));
  item_container_->RemoveChildView(list_item.item_view);
  item_container_->RemoveChildView(list_item.separator_view);
}

views::Button& HoverListView::GetTopListItemView() const {
  DCHECK(!tags_to_list_item_views_.empty());
  return *tags_to_list_item_views_.begin()->second.item_view;
}

void HoverListView::RequestFocus() {
  if (tags_to_list_item_views_.empty())
    return;

  GetTopListItemView().RequestFocus();
}

void HoverListView::OnListItemAdded(int item_tag) {
  AddListItemView(item_tag);
}

void HoverListView::OnListItemRemoved(int removed_item_tag) {
  RemoveListItemView(removed_item_tag);
}

void HoverListView::OnListItemChanged(int changed_list_item_tag,
                                      HoverListModel::ListItemChangeType type) {
  if (type == HoverListModel::ListItemChangeType::kAddToViewComponent) {
    AddListItemView(changed_list_item_tag);
  } else {
    RemoveListItemView(changed_list_item_tag);
  }
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
  for (const auto* iter : throbber_views_) {
    size += iter->GetPreferredSize().height() + separator_height;
  }
  int reserved_items =
      model_->GetPreferredItemCount() - tags_to_list_item_views_.size();
  if (reserved_items > 0) {
    auto dummy_hover_button = CreateHoverButtonForListItem(
        &gfx::kNoneIcon, std::u16string(), std::u16string(),
        views::Button::PressedCallback(), is_two_line_list_);
    const auto list_item_height =
        separator_height + dummy_hover_button->GetPreferredSize().height();
    size += list_item_height * reserved_items;
  }
  return std::min(kMaxViewHeight, size);
}

BEGIN_METADATA(HoverListView, views::View)
ADD_READONLY_PROPERTY_METADATA(int, PreferredViewHeight)
END_METADATA
