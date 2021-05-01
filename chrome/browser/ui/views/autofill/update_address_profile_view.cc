// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/update_address_profile_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace autofill {

namespace {

constexpr int kColumnSetId = 0;
constexpr int kIconSize = 16;

// New and old values appear in the update prompt in the same order as the order
// of the types in this array.
const ServerFieldType user_visibe_type[] = {NAME_HONORIFIC_PREFIX,
                                            NAME_FULL,
                                            ADDRESS_HOME_STREET_ADDRESS,
                                            ADDRESS_HOME_CITY,
                                            ADDRESS_HOME_ZIP,
                                            ADDRESS_HOME_COUNTRY,
                                            EMAIL_ADDRESS,
                                            PHONE_HOME_WHOLE_NUMBER,
                                            COMPANY_NAME};

const gfx::VectorIcon& GetVectorIconForType(ServerFieldType type) {
  // TODO(crbug.com/1167060): Update icons upon having final mocks.
  switch (type) {
    case NAME_FULL:
    case NAME_HONORIFIC_PREFIX:
      return kUserAccountAvatarIcon;
    case EMAIL_ADDRESS:
      return kWebIcon;
    case PHONE_HOME_WHOLE_NUMBER:
      return vector_icons::kCallIcon;
    default:
      return vector_icons::kLocationOnIcon;
  }
}

std::unique_ptr<views::ImageView> CreateIconViewForType(ServerFieldType type,
                                                        bool for_new_value) {
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(ui::ImageModel::FromVectorIcon(
      GetVectorIconForType(type),
      for_new_value ? ui::NativeTheme::kColorId_ProminentButtonColor
                    : ui::NativeTheme::kColorId_DefaultIconColor,
      kIconSize));
  return icon_view;
}

// Creates a view that displays all values in `differences`. `are_new_values`
// decides which set of values from `differences` are displayed.
std::unique_ptr<views::View> CreateValuesView(
    const std::vector<ProfileValueDifference>& differences,
    bool are_new_values) {
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

  for (const ProfileValueDifference& difference : differences) {
    views::View* value_row =
        view->AddChildView(std::make_unique<views::View>());
    value_row->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetCollapseMargins(true)
        .SetDefault(
            views::kMarginsKey,
            gfx::Insets(
                /*vertical=*/0,
                /*horizontal=*/ChromeLayoutProvider::Get()->GetDistanceMetric(
                    views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));

    value_row->AddChildView(
        CreateIconViewForType(difference.type, are_new_values));
    value_row->AddChildView(std::make_unique<views::Label>(
        are_new_values ? difference.first_value : difference.second_value,
        views::style::CONTEXT_LABEL));
  }
  return view;
}

// Add a row in `layout` that contains a label and a view displays all the
// values in `differences`. `are_new_values` controls the displayed label and
// which set of values from `differences` are displayed.
void AddValuesRow(views::GridLayout* layout,
                  const std::vector<ProfileValueDifference>& differences,
                  views::Button::PressedCallback edit_button_callback) {
  bool are_new_values = !!edit_button_callback;
  layout->StartRow(/*vertical_resize=*/views::GridLayout::kFixedSize,
                   kColumnSetId);

  // TODO(crbug.com/1167060): Use internationalized string.
  std::unique_ptr<views::Label> label(new views::Label(
      are_new_values ? u"New" : u"Old", views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY));
  layout->AddView(std::move(label), /*col_span=*/1, /*row_span=*/1,
                  /*h_align=*/views::GridLayout::LEADING,
                  /*v_align=*/views::GridLayout::LEADING);
  layout->AddView(CreateValuesView(differences, are_new_values),
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

  SetAcceptCallback(base::BindOnce(
      &SaveUpdateAddressProfileBubbleController::OnUserDecision,
      base::Unretained(controller_),
      AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted));
  SetCancelCallback(base::BindOnce(
      &SaveUpdateAddressProfileBubbleController::OnUserDecision,
      base::Unretained(controller_),
      AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined));

  base::flat_map<ServerFieldType, std::pair<std::u16string, std::u16string>>
      differences =
          AutofillProfileComparator::GetSettingsVisibleProfileDifferenceMap(
              controller_->GetProfileToSave(),
              *controller_->GetOriginalProfile(),
              g_browser_process->GetApplicationLocale());
  std::vector<ProfileValueDifference> diff_vector;
  for (auto type : user_visibe_type) {
    const auto it = differences.find(type);
    if (it == differences.end())
      continue;
    diff_vector.emplace_back(
        ProfileValueDifference{type, it->second.first, it->second.second});
  }

  // Build the GridLayout column set.
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  const int column_divider = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  column_set->AddColumn(
      /*h_align=*/views::GridLayout::LEADING,
      /*v_align=*/views::GridLayout::LEADING,
      /*resize_percent=*/views::GridLayout::kFixedSize,
      /*size_type=*/views::GridLayout::ColumnSize::kUsePreferred,
      /*fixed_width=*/0, /*min_width=*/0);
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize, column_divider);
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
      layout, diff_vector,
      /*edit_button_callback=*/
      base::BindRepeating(
          &SaveUpdateAddressProfileBubbleController::OnEditButtonClicked,
          base::Unretained(controller_)));
  layout->AddPaddingRow(views::GridLayout::kFixedSize,
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_CONTROL_LIST_VERTICAL));
  AddValuesRow(layout, diff_vector, /*edit_button_callback=*/{});

  // TODO(crbug.com/1167060): Add support for dark mode.
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
