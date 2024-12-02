// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/update_address_profile_view.h"

#include <algorithm>

#include "base/ranges/algorithm.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/views/autofill/autofill_bubble_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/style/typography.h"

namespace autofill {

namespace {

constexpr int kIconSize = 16;
constexpr int kValuesLabelWidth = 190;

const gfx::VectorIcon& GetVectorIconForType(FieldType type) {
  switch (type) {
    case NAME_FULL:
      return kAccountCircleIcon;
    case ADDRESS_HOME_ADDRESS:
      return vector_icons::kLocationOnIcon;
    case EMAIL_ADDRESS:
      return vector_icons::kEmailIcon;
    case PHONE_HOME_WHOLE_NUMBER:
      return vector_icons::kCallIcon;
    default:
      NOTREACHED();
  }
}

// Creates a view that displays all values in `diff`. `are_new_values`
// decides which set of values from `diff` are displayed.
std::unique_ptr<views::View> CreateValuesView(
    const std::vector<ProfileValueDifference>& diff,
    bool are_new_values,
    ui::ColorId icon_color) {
  auto view = std::make_unique<views::View>();
  view->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                              DISTANCE_CONTROL_LIST_VERTICAL),
                          0));

  for (const ProfileValueDifference& diff_entry : diff) {
    const std::u16string& value =
        are_new_values ? diff_entry.first_value : diff_entry.second_value;
    // Don't add rows for empty original values.
    if (value.empty())
      continue;
    views::View* value_row =
        view->AddChildView(std::make_unique<views::View>());
    value_row->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
        .SetIgnoreDefaultMainAxisMargins(true)
        .SetCollapseMargins(true)
        .SetDefault(
            views::kMarginsKey,
            gfx::Insets::VH(0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                                   views::DISTANCE_RELATED_LABEL_HORIZONTAL)));

    auto label_view =
        std::make_unique<views::Label>(value, views::style::CONTEXT_LABEL);
    label_view->SetMultiLine(true);
    label_view->SizeToFit(kValuesLabelWidth);
    label_view->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

    auto icon_view = std::make_unique<views::ImageView>();
    icon_view->SetImage(ui::ImageModel::FromVectorIcon(
        GetVectorIconForType(diff_entry.type), icon_color, kIconSize));

    // The container aligns the icon vertically in the middle of the first label
    // line, the icon size is expected to be smaller than the label height.
    auto icon_container =
        views::Builder<views::BoxLayoutView>()
            .SetPreferredSize(gfx::Size(kIconSize, label_view->GetLineHeight()))
            .SetCrossAxisAlignment(
                views::BoxLayout::CrossAxisAlignment::kCenter)
            .Build();

    icon_container->AddChildView(std::move(icon_view));
    value_row->AddChildView(std::move(icon_container));
    value_row->AddChildView(std::move(label_view));
  }
  return view;
}

// Add a row in `layout` that contains a label and a view displays all the
// values in `values`. Labels are added only if `show_row_label` is true.
void AddValuesRow(views::TableLayoutView* layout_view,
                  const std::vector<ProfileValueDifference>& diff,
                  bool show_row_label,
                  views::Button::PressedCallback edit_button_callback) {
  bool are_new_values = !!edit_button_callback;
  layout_view->AddRows(1, /*vertical_resize=*/views::TableLayout::kFixedSize);

  if (show_row_label) {
    auto label = std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(
            are_new_values
                ? IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_NEW_VALUES_SECTION_LABEL
                : IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OLD_VALUES_SECTION_LABEL),
        views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY);
    layout_view->AddChildView(std::move(label));
  }
  ui::ColorId icon_color = are_new_values ? ui::kColorButtonBackgroundProminent
                                          : ui::kColorIconSecondary;
  layout_view->AddChildView(CreateValuesView(diff, are_new_values, icon_color));
  if (are_new_values) {
    std::unique_ptr<views::ImageButton> edit_button =
        CreateEditButton(std::move(edit_button_callback));

    edit_button->SetProperty(views::kElementIdentifierKey,
                             UpdateAddressProfileView::kEditButtonViewId);
    layout_view->AddChildView(std::move(edit_button));
  }
}

// Returns true if there is there is at least one entry in `diff` with
// non-empty second value.
bool HasNonEmptySecondValues(const std::vector<ProfileValueDifference>& diff) {
  return std::ranges::any_of(diff, [](const ProfileValueDifference& entry) {
    return !entry.second_value.empty();
  });
}

// Returns true if there is an entry coressponding to type ADDRESS_HOME_ADDRESS.
bool HasAddressEntry(const std::vector<ProfileValueDifference>& diff) {
  return std::ranges::any_of(diff, [](const ProfileValueDifference& entry) {
    return entry.type == ADDRESS_HOME_ADDRESS;
  });
}

}  // namespace

UpdateAddressProfileView::UpdateAddressProfileView(
    std::unique_ptr<UpdateAddressBubbleController> controller,
    views::View* anchor_view,
    content::WebContents* web_contents)
    : AddressBubbleBaseView(anchor_view, web_contents),
      controller_(std::move(controller)) {
  auto* layout_provider = views::LayoutProvider::Get();

  SetAcceptCallback(base::BindOnce(
      &UpdateAddressBubbleController::OnUserDecision,
      base::Unretained(controller_.get()),
      AutofillClient::AddressPromptUserDecision::kAccepted, std::nullopt));
  SetCancelCallback(base::BindOnce(
      &UpdateAddressBubbleController::OnUserDecision,
      base::Unretained(controller_.get()),
      AutofillClient::AddressPromptUserDecision::kDeclined, std::nullopt));

  SetProperty(views::kElementIdentifierKey, kTopViewId);
  SetTitle(controller_->GetWindowTitle());
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(
                     IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_CANCEL_BUTTON_LABEL));

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::VH(layout_provider->GetDistanceMetric(
                                      DISTANCE_CONTROL_LIST_VERTICAL),
                                  0));

  std::vector<ProfileValueDifference> profile_diff = GetProfileDifferenceForUi(
      controller_->GetProfileToSave(), controller_->GetOriginalProfile(),
      g_browser_process->GetApplicationLocale());

  std::u16string subtitle = GetProfileDescription(
      controller_->GetOriginalProfile(),
      g_browser_process->GetApplicationLocale(),
      /*include_address_and_contacts=*/!HasAddressEntry(profile_diff));
  if (!subtitle.empty()) {
    views::Label* subtitle_label = AddChildView(std::make_unique<views::Label>(
        subtitle, views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
    subtitle_label->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_LEFT);
  }

  auto* main_content_view =
      AddChildView(std::make_unique<views::TableLayoutView>());

  bool has_non_empty_original_values = HasNonEmptySecondValues(profile_diff);

  // Build the TableLayoutView columns.
  const int column_divider = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  if (has_non_empty_original_values) {
    // Label column exists only if there is a section for original values.
    main_content_view
        ->AddColumn(
            /*h_align=*/views::LayoutAlignment::kStart,
            /*v_align=*/views::LayoutAlignment::kStart,
            /*horizontal_resize=*/views::TableLayout::kFixedSize,
            /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
            /*fixed_width=*/0, /*min_width=*/0)
        .AddPaddingColumn(views::TableLayout::kFixedSize, column_divider);
  }
  main_content_view
      ->AddColumn(
          /*h_align=*/views::LayoutAlignment::kStretch,
          /*v_align=*/views::LayoutAlignment::kStretch,
          /*horizontal_resize=*/1.0,
          /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
          /*fixed_width=*/0, /*min_width=*/0)
      .AddColumn(
          /*h_align=*/views::LayoutAlignment::kStart,
          /*v_align=*/views::LayoutAlignment::kStart,
          /*horizontal_resize=*/views::TableLayout::kFixedSize,
          /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
          /*fixed_width=*/0, /*min_width=*/0);

  AddValuesRow(
      main_content_view, profile_diff,
      /*show_row_label=*/has_non_empty_original_values,
      /*edit_button_callback=*/
      base::BindRepeating(&UpdateAddressBubbleController::OnEditButtonClicked,
                          base::Unretained(controller_.get())));

  if (has_non_empty_original_values) {
    main_content_view->AddPaddingRow(
        views::TableLayout::kFixedSize,
        layout_provider->GetDistanceMetric(
            DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE));
    AddValuesRow(main_content_view, profile_diff, /*show_row_label=*/true,
                 /*edit_button_callback=*/{});
  }

  std::u16string footer_message = controller_->GetFooterMessage();
  if (!footer_message.empty()) {
    SetFootnoteView(
        views::Builder<views::Label>()
            .SetText(footer_message)
            .SetTextContext(views::style::CONTEXT_BUBBLE_FOOTER)
            .SetTextStyle(views::style::STYLE_SECONDARY)
            .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
            .SetMultiLine(true)
            .Build());
  }

  set_fixed_width(std::max(
      main_content_view->GetPreferredSize().width() + margins().width(),
      layout_provider->GetDistanceMetric(
          views::DISTANCE_BUBBLE_PREFERRED_WIDTH)));
}

UpdateAddressProfileView::~UpdateAddressProfileView() = default;

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

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(UpdateAddressProfileView, kTopViewId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(UpdateAddressProfileView,
                                      kEditButtonViewId);

}  // namespace autofill
