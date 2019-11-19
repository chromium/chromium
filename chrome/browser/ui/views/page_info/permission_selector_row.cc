// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/permission_selector_row.h"

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/page_info/page_info_ui.h"
#include "chrome/browser/ui/page_info/permission_menu_model.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "components/strings/grit/components_strings.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// The text context / style of the |PermissionSelectorRow| combobox and label.
constexpr int kPermissionRowTextContext = views::style::CONTEXT_LABEL;
constexpr int kPermissionRowTextStyle = views::style::STYLE_PRIMARY;

}  // namespace

namespace internal {

// This class adapts a |PermissionMenuModel| into a |ui::ComboboxModel| so that
// |PermissionCombobox| can use it.
class ComboboxModelAdapter : public ui::ComboboxModel {
 public:
  explicit ComboboxModelAdapter(PermissionMenuModel* model) : model_(model) {}
  ~ComboboxModelAdapter() override {}

  void OnPerformAction(int index);

  // Returns the checked index of the underlying PermissionMenuModel, of which
  // there must be exactly one. This is used to choose which index is selected
  // in the PermissionCombobox.
  int GetCheckedIndex();

  // ui::ComboboxModel:
  int GetItemCount() const override;
  base::string16 GetItemAt(int index) override;

 private:
  PermissionMenuModel* model_;
};

void ComboboxModelAdapter::OnPerformAction(int index) {
  int command_id = model_->GetCommandIdAt(index);
  model_->ExecuteCommand(command_id, 0);
}

int ComboboxModelAdapter::GetCheckedIndex() {
  int checked_index = -1;
  for (int i = 0; i < model_->GetItemCount(); ++i) {
    int command_id = model_->GetCommandIdAt(i);
    if (model_->IsCommandIdChecked(command_id)) {
      // This function keeps track of |checked_index| instead of returning early
      // here so that it can DCHECK that there's exactly one selected item,
      // which is not normally guaranteed by MenuModel, but *is* true of
      // PermissionMenuModel.
      DCHECK_EQ(checked_index, -1);
      checked_index = i;
    }
  }
  return checked_index;
}

int ComboboxModelAdapter::GetItemCount() const {
  DCHECK(model_);
  return model_->GetItemCount();
}

base::string16 ComboboxModelAdapter::GetItemAt(int index) {
  return model_->GetLabelAt(index);
}

// The |PermissionCombobox| provides a combobox for selecting a permission type.
class PermissionCombobox : public views::Combobox,
                           public views::ComboboxListener {
 public:
  PermissionCombobox(ComboboxModelAdapter* model,
                     bool enabled,
                     bool use_default);
  ~PermissionCombobox() override;

  void UpdateSelectedIndex(bool use_default);

  void set_min_width(int width) { min_width_ = width; }

  // views::Combobox:
  gfx::Size CalculatePreferredSize() const override;

 private:
  // views::ComboboxListener:
  void OnPerformAction(Combobox* combobox) override;

  ComboboxModelAdapter* model_;

  // Minimum width for |PermissionCombobox|.
  int min_width_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PermissionCombobox);
};

PermissionCombobox::PermissionCombobox(ComboboxModelAdapter* model,
                                       bool enabled,
                                       bool use_default)
    : views::Combobox(model), model_(model) {
  set_listener(this);
  SetEnabled(enabled);
  UpdateSelectedIndex(use_default);
  set_size_to_largest_label(false);
}

PermissionCombobox::~PermissionCombobox() {}

void PermissionCombobox::UpdateSelectedIndex(bool use_default) {
  int index = model_->GetCheckedIndex();
  if (use_default && index == -1) {
    index = 0;
  }
  SetSelectedIndex(index);
}

gfx::Size PermissionCombobox::CalculatePreferredSize() const {
  gfx::Size preferred_size = Combobox::CalculatePreferredSize();
  preferred_size.SetToMax(gfx::Size(min_width_, 0));
  return preferred_size;
}

void PermissionCombobox::OnPerformAction(Combobox* combobox) {
  model_->OnPerformAction(combobox->GetSelectedIndex());
}

}  // namespace internal

///////////////////////////////////////////////////////////////////////////////
// PermissionSelectorRow
///////////////////////////////////////////////////////////////////////////////

PermissionSelectorRow::PermissionSelectorRow(
    Profile* profile,
    const GURL& url,
    const PageInfoUI::PermissionInfo& permission,
    views::GridLayout* layout)
    : profile_(profile), icon_(nullptr), combobox_(nullptr) {
  const int list_item_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
                                    DISTANCE_CONTROL_LIST_VERTICAL) /
                                2;
  layout->StartRowWithPadding(1.0, PageInfoBubbleView::kPermissionColumnSetId,
                              views::GridLayout::kFixedSize, list_item_padding);

  // Create the permission icon and label.
  icon_ = layout->AddView(std::make_unique<NonAccessibleImageView>());
  // Create the label that displays the permission type.
  auto label = std::make_unique<views::Label>(
      PageInfoUI::PermissionTypeToUIString(permission.type),
      CONTEXT_BODY_TEXT_LARGE);
  icon_->SetImage(
      PageInfoUI::GetPermissionIcon(permission, label->GetEnabledColor()));
  label_ = layout->AddView(std::move(label));
  // Create the menu model.
  menu_model_ = std::make_unique<PermissionMenuModel>(
      profile, url, permission,
      base::Bind(&PermissionSelectorRow::PermissionChanged,
                 base::Unretained(this)));

  // Create the permission combobox.
  InitializeComboboxView(layout, permission);

  // Show the permission decision reason, if it was not the user.
  base::string16 reason =
      PageInfoUI::PermissionDecisionReasonToUIString(profile, permission, url);
  if (!reason.empty()) {
    layout->StartRow(1.0, PageInfoBubbleView::kPermissionColumnSetId);
    layout->SkipColumns(1);
    auto secondary_label = std::make_unique<views::Label>(reason);
    secondary_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    secondary_label->SetEnabledColor(PageInfoUI::GetSecondaryTextColor());
    // The |secondary_label| should wrap when it's too long instead of
    // stretching its parent view horizontally, but also ensure long strings
    // aren't wrapped too early.
    int preferred_width = secondary_label->GetPreferredSize().width();
    secondary_label->SetMultiLine(true);

    views::ColumnSet* column_set =
        layout->GetColumnSet(PageInfoBubbleView::kPermissionColumnSetId);
    DCHECK(column_set);
    // Secondary labels in Harmony may not overlap into space shared with the
    // combobox column.
    const int column_span = 1;

    // Long labels that cannot fit in the existing space under the permission
    // label should be allowed to use up to |kMaxSecondaryLabelWidth| for
    // display.
    constexpr int kMaxSecondaryLabelWidth = 140;
    if (preferred_width > kMaxSecondaryLabelWidth) {
      layout->AddView(std::move(secondary_label), column_span, 1.0,
                      views::GridLayout::LEADING, views::GridLayout::CENTER,
                      kMaxSecondaryLabelWidth, 0);
    } else {
      layout->AddView(std::move(secondary_label), column_span, 1.0,
                      views::GridLayout::FILL, views::GridLayout::CENTER);
    }
  }
  layout->AddPaddingRow(views::GridLayout::kFixedSize,
                        CalculatePaddingBeneathPermissionRow(!reason.empty()));
}

PermissionSelectorRow::~PermissionSelectorRow() {
  // Gross. On paper the Combobox and the ComboboxModelAdapter are both owned by
  // this class, but actually, the Combobox is owned by View and will be
  // destroyed in ~View(), which runs *after* ~PermissionSelectorRow() is done,
  // which means the Combobox gets destroyed after its ComboboxModel, which
  // causes an explosion when the Combobox attempts to stop observing the
  // ComboboxModel. This hack ensures the Combobox is deleted before its
  // ComboboxModel.
  delete combobox_;
}

int PermissionSelectorRow::CalculatePaddingBeneathPermissionRow(
    bool has_reason) {
  const int list_item_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
                                    DISTANCE_CONTROL_LIST_VERTICAL) /
                                2;
  if (!has_reason)
    return list_item_padding;

  const int combobox_height = MinHeightForPermissionRow();
  // Match the amount of padding above the |PermissionSelectorRow| title text
  // here by calculating its full height of this |PermissionSelectorRow| and
  // subtracting the line height, then dividing everything by two. Note it is
  // assumed the combobox is the tallest part of the row.
  return (list_item_padding * 2 + combobox_height -
          views::style::GetLineHeight(kPermissionRowTextContext,
                                      kPermissionRowTextStyle)) /
         2;
}

int PermissionSelectorRow::MinHeightForPermissionRow() {
  return ChromeLayoutProvider::Get()->GetControlHeightForFont(
      kPermissionRowTextContext, kPermissionRowTextStyle,
      combobox_->GetFontList());
}

void PermissionSelectorRow::AddObserver(
    PermissionSelectorRowObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PermissionSelectorRow::InitializeComboboxView(
    views::GridLayout* layout,
    const PageInfoUI::PermissionInfo& permission) {
  bool button_enabled =
      permission.source == content_settings::SETTING_SOURCE_USER;
  combobox_model_adapter_.reset(
      new internal::ComboboxModelAdapter(menu_model_.get()));
  auto combobox = std::make_unique<internal::PermissionCombobox>(
      combobox_model_adapter_.get(), button_enabled, true);
  combobox->SetEnabled(button_enabled);
  combobox->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_SELECTOR_TOOLTIP,
      PageInfoUI::PermissionTypeToUIString(permission.type)));
  combobox_ = layout->AddView(std::move(combobox));
}

void PermissionSelectorRow::PermissionChanged(
    const PageInfoUI::PermissionInfo& permission) {
  // Change the permission icon to reflect the selected setting.
  icon_->SetImage(
      PageInfoUI::GetPermissionIcon(permission, label_->GetEnabledColor()));

  bool use_default = permission.setting == CONTENT_SETTING_DEFAULT;
  auto* combobox = static_cast<internal::PermissionCombobox*>(combobox_);
  combobox->UpdateSelectedIndex(use_default);

  for (PermissionSelectorRowObserver& observer : observer_list_) {
    observer.OnPermissionChanged(permission);
  }
}

int PermissionSelectorRow::GetComboboxWidth() const {
  return combobox_->Combobox::GetPreferredSize().width();
}

void PermissionSelectorRow::SetMinComboboxWidth(int width) {
  auto* combobox = static_cast<internal::PermissionCombobox*>(combobox_);
  combobox->set_min_width(width);
}
