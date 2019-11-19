// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/hover_list_view.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/vector_icons.h"

namespace {

constexpr int kPlaceHolderItemTag = -1;

class WebauthnHoverButton : public HoverButton {
 public:
  WebauthnHoverButton(views::ButtonListener* button_listener,
                      std::unique_ptr<views::View> icon_view,
                      const base::string16& title,
                      const base::string16& subtitle,
                      std::unique_ptr<views::View> secondary_view)
      : HoverButton(button_listener,
                    std::move(icon_view),
                    title,
                    subtitle,
                    std::move(secondary_view),
                    false) {}

  gfx::Insets GetInsets() const override {
    gfx::Insets ret = HoverButton::GetInsets();
    if (vert_inset_.has_value()) {
      ret.set_top(*vert_inset_);
      ret.set_bottom(*vert_inset_);
    }
    if (left_inset_.has_value()) {
      ret.set_left(*left_inset_);
    }
    ret.set_right(8);
    return ret;
  }

  void SetInsetForNoIcon() {
    // When there's no icon, insets within the underlying HoverButton take care
    // of the padding on the left and we don't want to add any more.
    left_inset_ = 0;
  }

  void SetVertInset(int vert_inset) { vert_inset_ = vert_inset; }

 private:
  base::Optional<int> left_inset_;
  base::Optional<int> vert_inset_;
};

std::unique_ptr<HoverButton> CreateHoverButtonForListItem(
    int item_tag,
    const gfx::VectorIcon* vector_icon,
    base::string16 item_title,
    base::string16 item_description,
    views::ButtonListener* listener,
    bool is_two_line_item,
    bool is_placeholder_item = false) {
  // Derive the icon color from the text color of an enabled label.
  auto color_reference_label = std::make_unique<views::Label>(
      base::string16(), CONTEXT_BODY_TEXT_SMALL, views::style::STYLE_PRIMARY);
  const SkColor icon_color = color_utils::DeriveDefaultIconColor(
      color_reference_label->GetEnabledColor());

  auto item_image = std::make_unique<views::ImageView>();
  if (vector_icon) {
    constexpr int kIconSize = 20;
    item_image->SetImage(
        gfx::CreateVectorIcon(*vector_icon, kIconSize, icon_color));
  }

  std::unique_ptr<views::ImageView> chevron_image = nullptr;
  // kTwoLineVertInset is the top and bottom padding of the HoverButton if
  // |is_two_line_item| is true. This ensures that the spacing between the two
  // lines isn't too large because HoverButton will otherwise spread the lines
  // evenly over the given vertical space.
  constexpr int kTwoLineVertInset = 6;

  if (!is_placeholder_item) {
    constexpr int kChevronSize = 8;
    chevron_image = std::make_unique<views::ImageView>();
    chevron_image->SetImage(gfx::CreateVectorIcon(views::kSubmenuArrowIcon,
                                                  kChevronSize, icon_color));

    int chevron_vert_inset = 0;
    if (is_two_line_item) {
      // Items that are sized for two lines use the top and bottom insets of the
      // chevron image to pad single-line items out to a uniform height of
      // |kHeight|.
      constexpr int kHeight = 56;
      chevron_vert_inset =
          (kHeight - (2 * kTwoLineVertInset) - kChevronSize) / 2;
    }
    chevron_image->SetBorder(views::CreateEmptyBorder(
        gfx::Insets(/*top=*/chevron_vert_inset, /*left=*/12,
                    /*bottom=*/chevron_vert_inset, /*right=*/0)));
  }

  auto hover_button = std::make_unique<WebauthnHoverButton>(
      listener, std::move(item_image), std::move(item_title),
      std::move(item_description), std::move(chevron_image));
  hover_button->set_tag(item_tag);
  if (!vector_icon) {
    hover_button->SetInsetForNoIcon();
  }
  if (is_two_line_item) {
    hover_button->SetVertInset(kTwoLineVertInset);
  }

  // Because there is an icon on both sides, set a custom border that has only
  // half of the normal padding horizontally.
  constexpr int kExtraVerticalPadding = 2;
  constexpr int kHorizontalPadding = 8;
  gfx::Insets padding(views::LayoutProvider::Get()->GetDistanceMetric(
                          DISTANCE_CONTROL_LIST_VERTICAL) +
                          kExtraVerticalPadding,
                      kHorizontalPadding);
  hover_button->SetBorder(views::CreateEmptyBorder(padding));

  if (is_placeholder_item) {
    hover_button->SetState(HoverButton::ButtonState::STATE_DISABLED);
    const auto background_color =
        hover_button->GetNativeTheme()->GetSystemColor(
            ui::NativeTheme::kColorId_BubbleBackground);
    hover_button->SetTitleTextStyle(views::style::STYLE_DISABLED,
                                    background_color);
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

  for (const auto item_tag : model_->GetItemTags()) {
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
                                       base::string16 item_text,
                                       base::string16 description_text,
                                       int item_tag) {
  auto hover_button = CreateHoverButtonForListItem(
      item_tag, icon, item_text, description_text, this, is_two_line_list_);

  auto* list_item_view_ptr = hover_button.release();
  item_container_->AddChildView(list_item_view_ptr);
  auto* separator = AddSeparatorAsChild(item_container_);
  tags_to_list_item_views_.emplace(
      item_tag, ListItemViews{list_item_view_ptr, separator});
}

void HoverListView::CreateAndAppendPlaceholderItem() {
  auto placeholder_item = CreateHoverButtonForListItem(
      kPlaceHolderItemTag, model_->GetPlaceholderIcon(),
      model_->GetPlaceholderText(), base::string16(), nullptr,
      true /* is_placeholder_item */);
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

void HoverListView::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  model_->OnListItemSelected(sender->tag());
}

int HoverListView::GetPreferredViewHeight() const {
  constexpr int kMaxViewHeight = 300;

  // |item_container_| has one separator at the top and list items which
  // contain one separator and one hover button.
  const auto separator_height = views::Separator().GetPreferredSize().height();
  int size = separator_height;
  for (auto iter = tags_to_list_item_views_.begin();
       iter != tags_to_list_item_views_.end(); ++iter) {
    size +=
        iter->second.item_view->GetPreferredSize().height() + separator_height;
  }
  int reserved_items =
      model_->GetPreferredItemCount() - tags_to_list_item_views_.size();
  if (reserved_items > 0) {
    auto dummy_hover_button = CreateHoverButtonForListItem(
        -1 /* tag */, &gfx::kNoneIcon, base::string16(), base::string16(),
        nullptr /* listener */, is_two_line_list_);
    const auto list_item_height =
        separator_height + dummy_hover_button->GetPreferredSize().height();
    size += list_item_height * reserved_items;
  }
  return std::min(kMaxViewHeight, size);
}
