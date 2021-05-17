// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/update_address_profile_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/style/typography.h"

namespace autofill {

namespace {

constexpr int kColumnSetId = 0;
constexpr int kIconSize = 16;

int AddressDetailsIconSize() {
  // Use the line height of the body small text. This allows the icons to adapt
  // if the user changes the font size.
  return views::style::GetLineHeight(views::style::CONTEXT_LABEL,
                                     views::style::STYLE_PRIMARY);
}

const gfx::VectorIcon& GetVectorIconForType(ServerFieldType type) {
  // TODO(crbug.com/1167060): Update icons upon having final mocks.
  switch (type) {
    case NAME_FULL_WITH_HONORIFIC_PREFIX:
      return kUserAccountAvatarIcon;
    case EMAIL_ADDRESS:
      return vector_icons::kEmailIcon;
    case PHONE_HOME_WHOLE_NUMBER:
      return vector_icons::kCallIcon;
    default:
      return vector_icons::kLocationOnIcon;
  }
}

// Creates a view that displays all values in `diff_map`. `are_new_values`
// decides which set of values from `diff_map` are displayed.
std::unique_ptr<views::View> CreateValuesView(
    const base::flat_map<ServerFieldType,
                         std::pair<std::u16string, std::u16string>>& diff_map,
    bool are_new_values,
    ui::NativeTheme::ColorId icon_color) {
  auto view = std::make_unique<views::View>();
  view->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets(
              /*vertical=*/ChromeLayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_CONTROL_LIST_VERTICAL),
              /*horizontal=*/0));

  for (ServerFieldType type : kVisibleTypesForProfileDifferences) {
    const auto it = diff_map.find(type);
    if (it == diff_map.end())
      continue;
    const std::u16string& value =
        are_new_values ? it->second.first : it->second.second;
    // Don't add rows for empty original values.
    if (value.empty())
      continue;
    views::View* value_row =
        view->AddChildView(std::make_unique<views::View>());
    value_row->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetIgnoreDefaultMainAxisMargins(true)
        .SetCollapseMargins(true)
        .SetDefault(
            views::kMarginsKey,
            gfx::Insets(
                /*vertical=*/0,
                /*horizontal=*/ChromeLayoutProvider::Get()->GetDistanceMetric(
                    views::DISTANCE_RELATED_LABEL_HORIZONTAL)));

    auto icon_view = std::make_unique<views::ImageView>();
    icon_view->SetImage(ui::ImageModel::FromVectorIcon(
        GetVectorIconForType(type), icon_color, AddressDetailsIconSize()));

    value_row->AddChildView(std::move(icon_view));
    value_row->AddChildView(
        std::make_unique<views::Label>(value, views::style::CONTEXT_LABEL));
  }
  return view;
}

// Add a row in `layout` that contains a label and a view displays all the
// values in `values`. Labels are added only if `show_row_label` is true.
void AddValuesRow(
    views::GridLayout* layout,
    const base::flat_map<ServerFieldType,
                         std::pair<std::u16string, std::u16string>>& diff_map,
    bool show_row_label,
    views::Button::PressedCallback edit_button_callback) {
  bool are_new_values = !!edit_button_callback;
  layout->StartRow(/*vertical_resize=*/views::GridLayout::kFixedSize,
                   kColumnSetId);

  // TODO(crbug.com/1167060): Use internationalized string.
  if (show_row_label) {
    std::unique_ptr<views::Label> label(new views::Label(
        are_new_values ? u"New" : u"Old", views::style::CONTEXT_LABEL,
        views::style::STYLE_SECONDARY));
    layout->AddView(std::move(label), /*col_span=*/1, /*row_span=*/1,
                    /*h_align=*/views::GridLayout::LEADING,
                    /*v_align=*/views::GridLayout::LEADING);
  }
  ui::NativeTheme::ColorId icon_color =
      are_new_values ? ui::NativeTheme::kColorId_ProminentButtonColor
                     : ui::NativeTheme::kColorId_SecondaryIconColor;
  layout->AddView(CreateValuesView(diff_map, are_new_values, icon_color),
                  /*col_span=*/1,
                  /*row_span=*/1,
                  /*h_align=*/views::GridLayout::FILL,
                  /*v_align=*/views::GridLayout::FILL);
  if (are_new_values) {
    std::unique_ptr<views::ImageButton> edit_button =
        views::CreateVectorImageButtonWithNativeTheme(
            std::move(edit_button_callback), vector_icons::kEditIcon,
            kIconSize);
    // TODO(crbug.com/1167060): Use internationalized string.
    edit_button->SetAccessibleName(u"Edit Address");
    layout->AddView(std::move(edit_button), /*col_span=*/1, /*row_span=*/1,
                    /*h_align=*/views::GridLayout::LEADING,
                    /*v_align=*/views::GridLayout::LEADING);
  }
}

// Returns true if there is there is at least one entry in `diff_map` with
// non-empty second value.
bool HasNonEmptySecondValues(
    const base::flat_map<ServerFieldType,
                         std::pair<std::u16string, std::u16string>>& diff_map) {
  return base::ranges::any_of(
      diff_map,
      [](const std::pair<autofill::ServerFieldType,
                         std::pair<std::u16string, std::u16string>>& entry) {
        return !entry.second.second.empty();
      });
}

}  // namespace

UpdateAddressProfileView::UpdateAddressProfileView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    SaveUpdateAddressProfileBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kAutofillAddressProfileSavePrompt));
  // Since this is an update prompt, original profile must be set. Otherwise, it
  // would have been a save prompt.
  DCHECK(controller_->GetOriginalProfile());

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  SetAcceptCallback(base::BindOnce(
      &SaveUpdateAddressProfileBubbleController::OnUserDecision,
      base::Unretained(controller_),
      AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted));
  SetCancelCallback(base::BindOnce(
      &SaveUpdateAddressProfileBubbleController::OnUserDecision,
      base::Unretained(controller_),
      AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined));

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets(
              /*vertical=*/ChromeLayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_CONTROL_LIST_VERTICAL),
              /*horizontal=*/0));

  views::Label* subtitle_label = AddChildView(std::make_unique<views::Label>(
      GetDescriptionForProfileToUpdate(
          *controller_->GetOriginalProfile(),
          g_browser_process->GetApplicationLocale()),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  subtitle_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  views::View* main_content_view =
      AddChildView(std::make_unique<views::View>());

  base::flat_map<ServerFieldType, std::pair<std::u16string, std::u16string>>
      profile_diff_map = AutofillProfileComparator::GetProfileDifferenceMap(
          controller_->GetProfileToSave(), *controller_->GetOriginalProfile(),
          autofill::ServerFieldTypeSet(
              std::begin(kVisibleTypesForProfileDifferences),
              std::end(kVisibleTypesForProfileDifferences)),
          g_browser_process->GetApplicationLocale());

  bool has_non_empty_original_values =
      HasNonEmptySecondValues(profile_diff_map);

  // Build the GridLayout column set.
  views::GridLayout* layout = main_content_view->SetLayoutManager(
      std::make_unique<views::GridLayout>());
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  const int column_divider = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  if (has_non_empty_original_values) {
    // Label column exists only if there is a section for original values.
    column_set->AddColumn(
        /*h_align=*/views::GridLayout::LEADING,
        /*v_align=*/views::GridLayout::LEADING,
        /*resize_percent=*/views::GridLayout::kFixedSize,
        /*size_type=*/views::GridLayout::ColumnSize::kUsePreferred,
        /*fixed_width=*/0, /*min_width=*/0);
    column_set->AddPaddingColumn(views::GridLayout::kFixedSize, column_divider);
  }
  column_set->AddColumn(
      /*h_align=*/views::GridLayout::FILL,
      /*v_align=*/views::GridLayout::FILL,
      /*resize_percent=*/1.0,
      /*size_type=*/views::GridLayout::ColumnSize::kUsePreferred,
      /*fixed_width=*/0, /*min_width=*/0);
  column_set->AddColumn(
      /*h_align=*/views::GridLayout::LEADING,
      /*v_align=*/views::GridLayout::LEADING,
      /*resize_percent=*/views::GridLayout::kFixedSize,
      /*size_type=*/views::GridLayout::ColumnSize::kUsePreferred,
      /*fixed_width=*/0, /*min_width=*/0);

  AddValuesRow(
      layout, profile_diff_map,
      /*show_row_label=*/has_non_empty_original_values,
      /*edit_button_callback=*/
      base::BindRepeating(
          &SaveUpdateAddressProfileBubbleController::OnEditButtonClicked,
          base::Unretained(controller_)));

  if (has_non_empty_original_values) {
    layout->AddPaddingRow(views::GridLayout::kFixedSize,
                          ChromeLayoutProvider::Get()->GetDistanceMetric(
                              DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE));
    AddValuesRow(layout, profile_diff_map, /*show_row_label=*/true,
                 /*edit_button_callback=*/{});
  }
}

bool UpdateAddressProfileView::ShouldShowCloseButton() const {
  return true;
}

std::u16string UpdateAddressProfileView::GetWindowTitle() const {
  return controller_->GetWindowTitle();
}

void UpdateAddressProfileView::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

void UpdateAddressProfileView::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void UpdateAddressProfileView::Hide() {
  CloseBubble();

  // If |controller_| is null, WindowClosing() won't invoke OnBubbleClosed(), so
  // do that here. This will clear out |controller_|'s reference to |this|. Note
  // that WindowClosing() happens only after the _asynchronous_ Close() task
  // posted in CloseBubble() completes, but we need to fix references sooner.
  if (controller_)
    controller_->OnBubbleClosed();

  controller_ = nullptr;
}

}  // namespace autofill
