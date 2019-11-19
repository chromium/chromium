// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_item_list.h"

#include <algorithm>
#include <utility>

#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/payments/content/payment_request_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace payments {

namespace {

constexpr SkColor kCheckmarkColor = 0xFF609265;

constexpr gfx::Insets kRowInsets = gfx::Insets(
    kPaymentRequestRowVerticalInsets,
    kPaymentRequestRowHorizontalInsets,
    kPaymentRequestRowVerticalInsets,
    kPaymentRequestRowHorizontalInsets + kPaymentRequestRowExtraRightInset);

// The space between the checkmark, extra view, and edit button.
constexpr int kExtraViewSpacing = 16;

constexpr int kEditIconSize = 16;

}  // namespace

PaymentRequestItemList::Item::Item(PaymentRequestSpec* spec,
                                   PaymentRequestState* state,
                                   PaymentRequestItemList* list,
                                   bool selected,
                                   bool clickable,
                                   bool show_edit_button)
    : PaymentRequestRowView(this, clickable, kRowInsets),
      spec_(spec),
      state_(state),
      list_(list),
      selected_(selected),
      show_edit_button_(show_edit_button) {}

PaymentRequestItemList::Item::~Item() {}

void PaymentRequestItemList::Item::Init() {
  std::unique_ptr<views::View> content =
      CreateContentView(&accessible_item_description_);

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  // Add a column for the item's content view.
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING, 1.0,
                     views::GridLayout::USE_PREF, 0, 0);

  // Add a column for the checkmark shown next to the selected profile.
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                     views::GridLayout::kFixedSize, views::GridLayout::USE_PREF,
                     0, 0);

  std::unique_ptr<views::View> extra_view = CreateExtraView();
  if (extra_view) {
    columns->AddPaddingColumn(views::GridLayout::kFixedSize, kExtraViewSpacing);
    // Add a column for the extra_view, which comes after the checkmark.
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                       views::GridLayout::kFixedSize,
                       views::GridLayout::USE_PREF, 0, 0);
  }

  if (show_edit_button_) {
    columns->AddPaddingColumn(views::GridLayout::kFixedSize, kExtraViewSpacing);
    // Add a column for the edit_button if it exists.
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                       views::GridLayout::kFixedSize, views::GridLayout::FIXED,
                       kEditIconSize, kEditIconSize);
  }

  layout->StartRow(views::GridLayout::kFixedSize, 0);
  content->set_can_process_events_within_subtree(false);
  layout->AddView(std::move(content));

  layout->AddView(CreateCheckmark(selected() && clickable()));

  if (extra_view)
    layout->AddView(std::move(extra_view));

  if (show_edit_button_) {
    auto edit_button = views::CreateVectorImageButton(this);
    const SkColor icon_color =
        color_utils::DeriveDefaultIconColor(SK_ColorBLACK);
    edit_button->SetImage(views::Button::STATE_NORMAL,
                          gfx::CreateVectorIcon(vector_icons::kEditIcon,
                                                kEditIconSize, icon_color));
    edit_button->set_ink_drop_base_color(icon_color);
    edit_button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    edit_button->SetID(static_cast<int>(DialogViewID::EDIT_ITEM_BUTTON));
    edit_button->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_PAYMENTS_EDIT));
    layout->AddView(std::move(edit_button));
  }

  UpdateAccessibleName();
}

void PaymentRequestItemList::Item::SetSelected(bool selected, bool notify) {
  selected_ = selected;

  for (views::View* child : children())
    if (child->GetID() == static_cast<int>(DialogViewID::CHECKMARK_VIEW)) {
      child->SetVisible(selected);
      break;
    }

  UpdateAccessibleName();

  if (notify)
    SelectedStateChanged();
}

std::unique_ptr<views::ImageView> PaymentRequestItemList::Item::CreateCheckmark(
    bool selected) {
  std::unique_ptr<views::ImageView> checkmark =
      std::make_unique<views::ImageView>();
  checkmark->SetID(static_cast<int>(DialogViewID::CHECKMARK_VIEW));
  checkmark->set_can_process_events_within_subtree(false);
  checkmark->SetImage(
      gfx::CreateVectorIcon(views::kMenuCheckIcon, kCheckmarkColor));
  checkmark->SetVisible(selected);
  checkmark->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  return checkmark;
}

std::unique_ptr<views::View> PaymentRequestItemList::Item::CreateExtraView() {
  return nullptr;
}

void PaymentRequestItemList::Item::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  if (sender->GetID() == static_cast<int>(DialogViewID::EDIT_ITEM_BUTTON)) {
    EditButtonPressed();
  } else if (selected_) {
    // |dialog()| may be null in tests
    if (list_->dialog())
      list_->dialog()->GoBack();
  } else if (CanBeSelected()) {
    list()->SelectItem(this);
  } else {
    PerformSelectionFallback();
  }
}

void PaymentRequestItemList::Item::UpdateAccessibleName() {
  base::string16 accessible_content =
      selected_ ? l10n_util::GetStringFUTF16(
                      IDS_PAYMENTS_ROW_ACCESSIBLE_NAME_SELECTED_FORMAT,
                      GetNameForDataType(), accessible_item_description_)
                : l10n_util::GetStringFUTF16(
                      IDS_PAYMENTS_ROW_ACCESSIBLE_NAME_FORMAT,
                      GetNameForDataType(), accessible_item_description_);
  SetAccessibleName(accessible_content);
}

PaymentRequestItemList::PaymentRequestItemList(PaymentRequestDialogView* dialog)
    : selected_item_(nullptr), dialog_(dialog) {}

PaymentRequestItemList::~PaymentRequestItemList() {}

void PaymentRequestItemList::AddItem(
    std::unique_ptr<PaymentRequestItemList::Item> item) {
  DCHECK_EQ(this, item->list());
  if (!items_.empty())
    item->set_previous_row(items_.back()->AsWeakPtr());
  items_.push_back(std::move(item));
  if (items_.back()->selected()) {
    if (selected_item_)
      selected_item_->SetSelected(/*selected=*/false, /*notify=*/false);
    selected_item_ = items_.back().get();
  }
}

void PaymentRequestItemList::Clear() {
  items_.clear();
  selected_item_ = nullptr;
}

std::unique_ptr<views::View> PaymentRequestItemList::CreateListView() {
  std::unique_ptr<views::View> content_view = std::make_unique<views::View>();

  content_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(kPaymentRequestRowVerticalInsets, 0), 0));

  for (auto& item : items_)
    content_view->AddChildView(item.release());

  return content_view;
}

void PaymentRequestItemList::SelectItem(PaymentRequestItemList::Item* item) {
  DCHECK_EQ(this, item->list());
  if (selected_item_ == item)
    return;

  UnselectSelectedItem();

  selected_item_ = item;
  item->SetSelected(/*selected=*/true, /*notify=*/true);
}

void PaymentRequestItemList::UnselectSelectedItem() {
  // It's possible that no item is currently selected, either during list
  // creation or in the middle of the selection operation when the previously
  // selected item has been deselected but the new one isn't selected yet.
  if (selected_item_)
    selected_item_->SetSelected(/*selected=*/false, /*notify=*/true);

  selected_item_ = nullptr;
}

}  // namespace payments
