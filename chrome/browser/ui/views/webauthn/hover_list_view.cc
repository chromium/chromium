// Copyright 2018 The Chromium Authors. All rights reserved.
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

enum class ItemType {
  kButton,
  kPlaceholder,
  kThrobber,
};

class ListItemVectorIconView : public views::ImageView {
 public:
  METADATA_HEADER(ListItemVectorIconView);
  ListItemVectorIconView(const gfx::VectorIcon* vector_icon, int icon_size)
      : vector_icon_(vector_icon), icon_size_(icon_size) {}
  ~ListItemVectorIconView() override = default;

  // views::ImageView:
  void OnThemeChanged() override {
    ImageView::OnThemeChanged();
    const SkColor icon_color =
        color_utils::DeriveDefaultIconColor(views::style::GetColor(
            *this, views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY));
    SetImage(gfx::CreateVectorIcon(*vector_icon_, icon_size_, icon_color));
  }

 private:
  raw_ptr<const gfx::VectorIcon> vector_icon_;
  int icon_size_;
};

BEGIN_METADATA(ListItemVectorIconView, views::ImageView)
END_METADATA

class ListItemHoverButton : public WebAuthnHoverButton {
 public:
  METADATA_HEADER(ListItemHoverButton);
  ListItemHoverButton(PressedCallback callback,
                      std::unique_ptr<views::ImageView> item_image,
                      std::u16string item_title,
                      std::u16string item_description,
                      std::unique_ptr<views::View> secondary_view,
                      bool is_two_line_item,
                      ItemType item_type)
      : WebAuthnHoverButton(std::move(callback),
                            std::move(item_image),
                            std::move(item_title),
                            std::move(item_description),
                            std::move(secondary_view),
                            is_two_line_item),
        item_type_(item_type) {
    if (item_type_ == ItemType::kPlaceholder ||
        item_type_ == ItemType::kThrobber) {
      SetState(HoverButton::ButtonState::STATE_DISABLED);
    }
  }
  ~ListItemHoverButton() override = default;

  // WebAuthnHoverButton:
  void OnThemeChanged() override {
    WebAuthnHoverButton::OnThemeChanged();
    if (item_type_ != ItemType::kPlaceholder)
      return;
    SetTitleTextStyle(views::style::STYLE_DISABLED,
                      GetColorProvider()->GetColor(ui::kColorBubbleBackground));
  }

 private:
  ItemType item_type_;
};

BEGIN_METADATA(ListItemHoverButton, WebAuthnHoverButton)
END_METADATA

std::unique_ptr<WebAuthnHoverButton> CreateHoverButtonForListItem(
    const gfx::VectorIcon* vector_icon,
    std::u16string item_title,
    std::u16string item_description,
    views::Button::PressedCallback callback,
    bool is_two_line_item,
    ItemType item_type = ItemType::kButton) {
  std::unique_ptr<views::View> secondary_view;

  switch (item_type) {
    case ItemType::kPlaceholder:
      // No secondary view in this case.
      break;

    case ItemType::kButton: {
      constexpr int kChevronSize = 8;
      secondary_view = std::make_unique<ListItemVectorIconView>(
          &vector_icons::kSubmenuArrowIcon, kChevronSize);
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

  constexpr int kIconSize = 20;
  std::unique_ptr<views::ImageView> item_image =
      vector_icon
          ? std::make_unique<ListItemVectorIconView>(vector_icon, kIconSize)
          : std::make_unique<views::ImageView>();

  return std::make_unique<ListItemHoverButton>(
      std::move(callback), std::move(item_image), item_title, item_description,
      std::move(secondary_view), is_two_line_item, item_type);
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
  item_container_->AddChildView(std::make_unique<views::Separator>());

  for (const auto item_tag : model_->GetThrobberTags()) {
    auto button = CreateHoverButtonForListItem(
        model_->GetItemIcon(item_tag), model_->GetItemText(item_tag),
        model_->GetDescriptionText(item_tag),
        base::BindRepeating(&HoverListModel::OnListItemSelected,
                            base::Unretained(model_.get()), item_tag),
        true, ItemType::kThrobber);
    throbber_views_.push_back(button.get());
    item_container_->AddChildView(button.release());
    item_container_->AddChildView(std::make_unique<views::Separator>());
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
  AddChildView(scroll_view_.get());
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
  auto* separator =
      item_container_->AddChildView(std::make_unique<views::Separator>());
  tags_to_list_item_views_.emplace(
      item_tag, ListItemViews{list_item_view_ptr, separator});
}

void HoverListView::CreateAndAppendPlaceholderItem() {
  auto placeholder_item = CreateHoverButtonForListItem(
      model_->GetPlaceholderIcon(), model_->GetPlaceholderText(),
      std::u16string(), views::Button::PressedCallback(),
      /*is_two_line_item=*/false, ItemType::kPlaceholder);
  item_container_->AddChildView(placeholder_item.get());
  auto* separator =
      item_container_->AddChildView(std::make_unique<views::Separator>());
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
