// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/combined_selector_views.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/webauthn/combined_selector_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_utils.h"

namespace {

// This value is used to group all `CombinedSelectorRadioButton` which are under
// the same `CombinedSelectorRowView`. The grouping is used for traversal and
// selection. The value is selected voluntarily in order not to conflict
// grouping from any parent views.
constexpr static int kGroupId = 1327;

}  // namespace

CombinedSelectorRadioButton::CombinedSelectorRadioButton(Delegate* delegate,
                                                         int index)
    : views::RadioButton(u"", kGroupId), delegate_(delegate), index_(index) {}

views::View* CombinedSelectorRadioButton::GetSelectedViewForGroup(int group) {
  Views views;
  GetRadioButtonsInList(group, &views);

  const auto i = std::ranges::find_if(views, [](const views::View* view) {
    return static_cast<const CombinedSelectorRadioButton*>(view)->GetChecked();
  });
  return (i == views.cend()) ? nullptr : *i;
}

void CombinedSelectorRadioButton::SetChecked(bool checked) {
  if (checked == RadioButton::GetChecked()) {
    return;
  }
  if (checked) {
    Views other;
    GetRadioButtonsInList(GetGroup(), &other);
    for (views::View* peer : other) {
      if (peer != this && IsViewClass<CombinedSelectorRadioButton>(peer)) {
        static_cast<CombinedSelectorRadioButton*>(peer)->SetChecked(false);
      }
    }
    delegate_->OnRadioButtonChecked(index_);
  }
  Checkbox::SetChecked(checked);
}

bool CombinedSelectorRadioButton::IsGroupFocusTraversable() const {
  return true;
}

void CombinedSelectorRadioButton::GetRadioButtonsInList(int group,
                                                        Views* views) {
  auto* row_view = parent();
  if (!row_view) {
    return;
  }
  auto* list_view = row_view->parent();
  if (!list_view) {
    return;
  }
  list_view->GetViewsInGroup(group, views);
}

BEGIN_METADATA(CombinedSelectorRadioButton)
END_METADATA

CombinedSelectorTextColumnView::CombinedSelectorTextColumnView(
    const std::vector<std::u16string_view> texts) {
  AddColumn(views::LayoutAlignment::kStart, views::LayoutAlignment::kCenter,
            1.0f, views::TableLayout::ColumnSize::kFixed, 0, 0);
  AddRows(texts.size(), views::TableLayout::kFixedSize);
  for (size_t i = 0; i < texts.size(); i++) {
    auto* label_view = AddChildView(std::make_unique<views::Label>(
        texts.at(i), views::style::CONTEXT_LABEL,
        i == 0 ? views::style::STYLE_BODY_3_MEDIUM
               : views::style::STYLE_BODY_4));
    label_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }
}

BEGIN_METADATA(CombinedSelectorTextColumnView)
END_METADATA

CombinedSelectorRowView::CombinedSelectorRowView(
    const ui::ImageModel& icon,
    const std::vector<std::u16string_view> texts,
    RadioStatus radio_status,
    bool enabled,
    CombinedSelectorRadioButton::Delegate* radio_delegate,
    int index)
    : radio_status_(radio_status), enabled_(enabled) {
  SetBackground(views::CreateSolidBackground(ui::kColorSysSurface2));
  SetEnabled(enabled);

  GetViewAccessibility().SetRole(radio_status != RadioStatus::kNone
                                     ? ax::mojom::Role::kRadioButton
                                     : ax::mojom::Role::kButton);
  GetViewAccessibility().SetName(base::JoinString(texts, u"\n"));
  SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(8, 16)));

  const int icon_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  AddColumn(views::LayoutAlignment::kCenter, views::LayoutAlignment::kCenter,
            views::TableLayout::kFixedSize,
            views::TableLayout::ColumnSize::kUsePreferred,
            /*fixed_width=*/0,
            /*min_width=*/0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, icon_padding);
  AddColumn(views::LayoutAlignment::kStretch, views::LayoutAlignment::kStretch,
            /*horizontal_resize=*/1.0,
            views::TableLayout::ColumnSize::kUsePreferred,
            /*fixed_width=*/0,
            /*min_width=*/0);
  if (radio_status != RadioStatus::kNone) {
    AddPaddingColumn(views::TableLayout::kFixedSize, icon_padding)
        .AddColumn(views::LayoutAlignment::kCenter,
                   views::LayoutAlignment::kCenter,
                   views::TableLayout::kFixedSize,
                   views::TableLayout::ColumnSize::kUsePreferred,
                   /*fixed_width=*/0,
                   /*min_width=*/0);
  }
  AddRows(1, views::TableLayout::kFixedSize);

  AddChildView(std::make_unique<views::ImageView>(icon));
  AddChildView(std::make_unique<CombinedSelectorTextColumnView>(texts));
  MaybeAddRadioButton(radio_delegate, index);
}

void CombinedSelectorRowView::MaybeAddRadioButton(
    CombinedSelectorRadioButton::Delegate* delegate,
    int index) {
  if (radio_status_ == RadioStatus::kNone) {
    return;
  }
  auto radio_button =
      std::make_unique<CombinedSelectorRadioButton>(delegate, index);
  radio_button->SetChecked(radio_status_ == RadioStatus::kSelected);
  radio_button->SetEnabled(enabled_);
  radio_button->GetViewAccessibility().SetName(*this);
  AddChildView(std::move(radio_button));
}

BEGIN_METADATA(CombinedSelectorRowView)
END_METADATA

CombinedSelectorListView::CombinedSelectorListView(
    CombinedSelectorSheetModel* model,
    CombinedSelectorRadioButton::Delegate* delegate) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      /* betweeen_child_spacing= */ CombinedSelectorSheetView::kRowGap));

  for (size_t i = 0; i < model->dialog_model()->mechanisms.size(); i++) {
    const auto& mechanism = model->dialog_model()->mechanisms[i];
    auto image_model =
        ui::ImageModel::FromVectorIcon(*mechanism.icon, ui::kColorIcon, 20);
    AddChildView(std::make_unique<CombinedSelectorRowView>(
        image_model,
        std::vector<std::u16string_view>{mechanism.name, mechanism.description},
        model->GetSelectionStatus(i), !model->dialog_model()->ui_disabled_,
        delegate, i));
  }
}

BEGIN_METADATA(CombinedSelectorListView)
END_METADATA
