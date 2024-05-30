// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_item_list.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/payments/content/payment_request_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace payments {

namespace {

constexpr SkColor kCheckmarkColor = 0xFF609265;

constexpr auto kRowInsets = gfx::Insets::TLBR(
    kPaymentRequestRowVerticalInsets,
    kPaymentRequestRowHorizontalInsets,
    kPaymentRequestRowVerticalInsets,
    kPaymentRequestRowHorizontalInsets + kPaymentRequestRowExtraRightInset);

// The space between the checkmark, extra view, and edit button.
constexpr int kExtraViewSpacing = 16;

constexpr int kEditIconSize = 16;

}  // namespace

PaymentRequestItemList::Item::Item(base::WeakPtr<PaymentRequestSpec> spec,
                                   base::WeakPtr<PaymentRequestState> state,
                                   PaymentRequestItemList* list,
                                   bool selected,
                                   bool clickable,
                                   bool show_edit_button)
    : PaymentRequestRowView(
          base::BindRepeating(&Item::ButtonPressed, base::Unretained(this)),
          clickable,
          kRowInsets),
      spec_(spec),
      state_(state),
      list_(list),
      selected_(selected),
      show_edit_button_(show_edit_button) {}

PaymentRequestItemList::Item::~Item() {}

void PaymentRequestItemList::Item::Init() {
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  views::View* content_view =
      AddChildView(CreateContentView(&accessible_item_description_));
  content_view->SetCanProcessEventsWithinSubtree(false);
  layout->SetFlexForView(content_view, 1);

  // The container view contains the checkmark shown next to the selected
  // profile, an optional extra_view and an optional edit_button.
  views::View* container = AddChildView(std::make_unique<views::View>());
  views::BoxLayout* container_layout =
      container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kExtraViewSpacing));
  container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  container_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  container->AddChildView(CreateCheckmark(selected() && GetClickable()));

  if (std::unique_ptr<views::View> extra_view = CreateExtraView())
    container->AddChildView(std::move(extra_view));

  if (show_edit_button_) {
    auto edit_button = views::CreateVectorImageButton(
        base::BindRepeating(&Item::EditButtonPressed, base::Unretained(this)));
    edit_button->SetBorder(nullptr);
    edit_button->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(vector_icons::kEditIcon, ui::kColorIcon,
                                       kEditIconSize));
    views::InkDrop::Get(edit_button.get())->SetBaseColorId(ui::kColorIcon);
    edit_button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    edit_button->SetID(static_cast<int>(DialogViewID::EDIT_ITEM_BUTTON));
    edit_button->GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_PAYMENTS_EDIT));
    container->AddChildView(std::move(edit_button));
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
  checkmark->SetCanProcessEventsWithinSubtree(false);
  checkmark->SetImage(
      ui::ImageModel::FromVectorIcon(views::kMenuCheckIcon, kCheckmarkColor));
  checkmark->SetVisible(selected);
  checkmark->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  return checkmark;
}

std::unique_ptr<views::View> PaymentRequestItemList::Item::CreateExtraView() {
  return nullptr;
}

void PaymentRequestItemList::Item::UpdateAccessibleName() {
  std::u16string accessible_content =
      selected_ ? l10n_util::GetStringFUTF16(
                      IDS_PAYMENTS_ROW_ACCESSIBLE_NAME_SELECTED_FORMAT,
                      GetNameForDataType(), accessible_item_description_)
                : l10n_util::GetStringFUTF16(
                      IDS_PAYMENTS_ROW_ACCESSIBLE_NAME_FORMAT,
                      GetNameForDataType(), accessible_item_description_);
  GetViewAccessibility().SetName(accessible_content);
}

void PaymentRequestItemList::Item::ButtonPressed() {
  if (selected_) {
    // |dialog()| may be null in tests
    if (list_->dialog())
      list_->dialog()->GoBack();
  } else if (CanBeSelected()) {
    list()->SelectItem(this);
  } else {
    PerformSelectionFallback();
  }
}

BEGIN_METADATA(PaymentRequestItemList, Item)
END_METADATA

PaymentRequestItemList::PaymentRequestItemList(
    base::WeakPtr<PaymentRequestDialogView> dialog)
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
      gfx::Insets::VH(kPaymentRequestRowVerticalInsets, 0), 0));

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
