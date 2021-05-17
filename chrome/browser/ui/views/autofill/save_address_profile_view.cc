// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/save_address_profile_view.h"

#include "base/strings/string_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/editable_combobox/editable_combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace autofill {

namespace {

constexpr int kIconSize = 16;

int AddressDetailsIconSize() {
  // Use the line height of the body small text. This allows the icons to adapt
  // if the user changes the font size.
  return views::style::GetLineHeight(views::style::CONTEXT_LABEL,
                                     views::style::STYLE_PRIMARY);
}

// Maps an AddressField to a ServerFieldType making sure
// NAME_FULL_WITH_HONORIFIC_PREFIX is returned instead of NAME_FULL for
// RECIPIENT type.
ServerFieldType AddressFieldToServerFieldTypeWithHonorificPrefix(
    ::i18n::addressinput::AddressField address_field) {
  ServerFieldType type = autofill::AddressFieldToServerFieldType(address_field);
  return type == NAME_FULL ? NAME_FULL_WITH_HONORIFIC_PREFIX : type;
}

int ComboboxIconSize() {
  // Use the line height of the body small text. This allows the icons to adapt
  // if the user changes the font size.
  return views::style::GetLineHeight(views::style::CONTEXT_MENU,
                                     views::style::STYLE_PRIMARY);
}

void AddAddressSection(views::View* parent_view,
                       const gfx::VectorIcon& icon,
                       std::unique_ptr<views::View> view) {
  views::View* row = parent_view->AddChildView(std::make_unique<views::View>());
  views::FlexLayout* row_layout =
      row->SetLayoutManager(std::make_unique<views::FlexLayout>());
  row_layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets(
              /*vertical=*/0,
              /*horizontal=*/ChromeLayoutProvider::Get()->GetDistanceMetric(
                  views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));

  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(ui::ImageModel::FromVectorIcon(
      icon, ui::NativeTheme::kColorId_SecondaryIconColor,
      AddressDetailsIconSize()));

  row->AddChildView(std::move(icon_view));
  row->AddChildView(std::move(view));
}

void AddAddressSection(views::View* parent_view,
                       const gfx::VectorIcon& icon,
                       const std::u16string& text) {
  auto text_label =
      std::make_unique<views::Label>(text, views::style::CONTEXT_LABEL);
  text_label->SetMultiLine(true);
  text_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  AddAddressSection(parent_view, icon, std::move(text_label));
}

std::unique_ptr<views::View> CreateAddressLineView() {
  auto line = std::make_unique<views::View>();
  views::FlexLayout* line_layout =
      line->SetLayoutManager(std::make_unique<views::FlexLayout>());
  line_layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true);
  return line;
}

std::unique_ptr<views::Label> CreateAddressComponentLabel(
    const std::u16string& text) {
  auto text_label =
      std::make_unique<views::Label>(text, views::style::CONTEXT_LABEL);
  text_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  text_label->SetMultiLine(true);
  return text_label;
}

// Create a view that contains the address in an envelope style format. Returns
// nullptr if the address is empty.
std::unique_ptr<views::View> CreateStreetAddressView(
    const AutofillProfile& profile,
    const std::string& locale) {
  auto address_view = std::make_unique<views::View>();
  views::FlexLayout* flex_layout =
      address_view->SetLayoutManager(std::make_unique<views::FlexLayout>());
  flex_layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey, gfx::Insets());

  const AutofillType kCountryCode(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE);
  const std::u16string& country_code = profile.GetInfo(kCountryCode, locale);

  std::vector<std::vector<::i18n::addressinput::AddressUiComponent>> components;
  autofill::GetAddressComponents(base::UTF16ToUTF8(country_code), locale,
                                 /*include_literals=*/true, &components,
                                 nullptr);

  for (const std::vector<::i18n::addressinput::AddressUiComponent>& line :
       components) {
    std::unique_ptr<views::View> line_view = CreateAddressLineView();
    std::vector<std::u16string> components_str;
    for (const ::i18n::addressinput::AddressUiComponent& component : line) {
      // AddressUiComponent can represent an address field such as City, or a
      // formatting literal such as "," or "-". If the literal field is empty,
      // then it represents a field, otherwise, it is a literal.
      std::u16string field_value =
          component.literal.empty()
              ? profile.GetInfo(
                    AddressFieldToServerFieldTypeWithHonorificPrefix(
                        component.field),
                    locale)
              : base::UTF8ToUTF16(component.literal);
      if (!field_value.empty())
        line_view->AddChildView(CreateAddressComponentLabel(field_value));
    }
    if (!line_view->children().empty())
      address_view->AddChildView(std::move(line_view));
  }
  // Append the country to the end.
  std::u16string country = profile.GetInfo(ADDRESS_HOME_COUNTRY, locale);
  if (!country.empty()) {
    std::unique_ptr<views::View> line_view = CreateAddressLineView();
    line_view->AddChildView(CreateAddressComponentLabel(country));
    address_view->AddChildView(std::move(line_view));
  }
  if (!address_view->children().empty())
    return address_view;
  return nullptr;
}

std::unique_ptr<views::EditableCombobox> CreateNicknameEditableCombobox() {
  // TODO(crbug.com/1167060): Update the icons
  // TODO(crbug.com/1167060): Use internationalized string.
  ui::SimpleComboboxModel::Item home(
      /*text=*/u"Home",
      /*dropdown_secondary_text=*/std::u16string(),
      /*icon=*/
      ui::ImageModel::FromVectorIcon(kNavigateHomeIcon,
                                     ui::NativeTheme::kColorId_DefaultIconColor,
                                     ComboboxIconSize()));

  ui::SimpleComboboxModel::Item work(
      /*text=*/u"Work",
      /*dropdown_secondary_text=*/std::u16string(),
      /*icon=*/
      ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                     ui::NativeTheme::kColorId_DefaultIconColor,
                                     ComboboxIconSize()));

  std::vector<ui::SimpleComboboxModel::Item> nicknames{std::move(home),
                                                       std::move(work)};

  auto combobox = std::make_unique<views::EditableCombobox>(
      std::make_unique<ui::SimpleComboboxModel>(std::move(nicknames)),
      /*filter_on_edit=*/true);

  combobox->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  // TODO(crbug.com/1167060): Use internationalized string.
  combobox->SetAccessibleName(u"Address Label");
  return combobox;
}

}  // namespace

SaveAddressProfileView::SaveAddressProfileView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    SaveUpdateAddressProfileBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kAutofillAddressProfileSavePrompt));
  // Since this is a save prompt, original profile must not be set. Otherwise,
  // it would have been an update prompt.
  DCHECK(!controller_->GetOriginalProfile());

  // TODO(crbug.com/1167060): Accept action should consider the selected
  // nickname when saving the address.
  SetAcceptCallback(base::BindOnce(
      &SaveUpdateAddressProfileBubbleController::OnUserDecision,
      base::Unretained(controller_),
      AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted));
  SetCancelCallback(base::BindOnce(
      &SaveUpdateAddressProfileBubbleController::OnUserDecision,
      base::Unretained(controller_),
      AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined));

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true);

  views::View* address_components_view =
      AddChildView(std::make_unique<views::View>());
  address_components_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          views::MinimumFlexSizeRule::kPreferredSnapToMinimum,
          views::MaximumFlexSizeRule::kUnbounded));

  // TODO(crbug.com/1167060): Update icons upon having final mocks
  std::unique_ptr<views::ImageButton> edit_button =
      views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(
              &SaveUpdateAddressProfileBubbleController::OnEditButtonClicked,
              base::Unretained(controller_)),
          vector_icons::kEditIcon, kIconSize);
  // TODO(crbug.com/1167060): Use internationalized string.
  edit_button->SetAccessibleName(u"Edit Address");
  AddChildView(std::move(edit_button));

  address_components_view
      ->SetLayoutManager(std::make_unique<views::FlexLayout>())
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

  const std::string locale = g_browser_process->GetApplicationLocale();
  const AutofillProfile& profile = controller_->GetProfileToSave();

  std::unique_ptr<views::View> street_address_view =
      CreateStreetAddressView(profile, locale);
  if (street_address_view) {
    AddAddressSection(/*parent_view=*/address_components_view,
                      vector_icons::kLocationOnIcon,
                      std::move(street_address_view));
  }

  std::u16string phone = profile.GetInfo(PHONE_HOME_WHOLE_NUMBER, locale);
  if (!phone.empty())
    AddAddressSection(/*parent_view=*/address_components_view,
                      vector_icons::kCallIcon, phone);

  std::u16string email = profile.GetInfo(EMAIL_ADDRESS, locale);
  if (!email.empty())
    AddAddressSection(/*parent_view=*/address_components_view,
                      vector_icons::kEmailIcon, email);

  if (base::FeatureList::IsEnabled(
          features::kAutofillAddressProfileSavePromptNicknameSupport)) {
    // TODO(crbug.com/1167060): Make sure the icon is vertically centered with
    // the editable combobox.
    AddAddressSection(/*parent_view=*/address_components_view,
                      vector_icons::kExtensionIcon,
                      CreateNicknameEditableCombobox());
  }
}

bool SaveAddressProfileView::ShouldShowCloseButton() const {
  return true;
}

std::u16string SaveAddressProfileView::GetWindowTitle() const {
  return controller_->GetWindowTitle();
}

void SaveAddressProfileView::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

void SaveAddressProfileView::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void SaveAddressProfileView::Hide() {
  CloseBubble();

  // If |controller_| is null, WindowClosing() won't invoke OnBubbleClosed(), so
  // do that here. This will clear out |controller_|'s reference to |this|. Note
  // that WindowClosing() happens only after the _asynchronous_ Close() task
  // posted in CloseBubble() completes, but we need to fix references sooner.
  if (controller_)
    controller_->OnBubbleClosed();

  controller_ = nullptr;
}

void SaveAddressProfileView::AddedToWidget() {
  // TODO(crbug.com/1167060): Update upon having final mocks.
  // Set the header image.
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  auto image_view = std::make_unique<ThemeTrackingNonAccessibleImageView>(
      *bundle.GetImageSkiaNamed(IDR_SAVE_ADDRESS),
      *bundle.GetImageSkiaNamed(IDR_SAVE_ADDRESS_DARK),
      base::BindRepeating(&views::BubbleFrameView::GetBackgroundColor,
                          base::Unretained(GetBubbleFrameView())));
  GetBubbleFrameView()->SetHeaderView(std::move(image_view));
}

}  // namespace autofill
