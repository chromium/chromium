// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/save_address_profile_view.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/autofill/autofill_bubble_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/editable_combobox/editable_combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

namespace {

constexpr int kIconSize = 16;

int ComboboxIconSize() {
  // Use the line height of the body small text. This allows the icons to adapt
  // if the user changes the font size.
  return views::TypographyProvider::Get().GetLineHeight(
      views::style::CONTEXT_MENU, views::style::STYLE_PRIMARY);
}

std::unique_ptr<views::ImageView> CreateAddressSectionIcon(
    const gfx::VectorIcon& icon) {
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(
      ui::ImageModel::FromVectorIcon(icon, ui::kColorIconSecondary, kIconSize));
  return icon_view;
}

void AddAddressSection(views::View* parent_view,
                       std::unique_ptr<views::ImageView> icon_view,
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
          gfx::Insets::VH(0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                                 views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  row->AddChildView(std::move(icon_view));
  row->AddChildView(std::move(view));
}

void AddAddressSection(views::View* parent_view,
                       std::unique_ptr<views::ImageView> icon_view,
                       const std::u16string& text,
                       int a11y_label_string_id) {
  auto text_label =
      std::make_unique<views::Label>(text, views::style::CONTEXT_LABEL);
  text_label->SetAccessibleName(
      l10n_util::GetStringFUTF16(a11y_label_string_id, text));
  text_label->SetMultiLine(true);
  text_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  return AddAddressSection(parent_view, std::move(icon_view),
                           std::move(text_label));
}

// Create a view that contains the address in an envelope style format.
std::unique_ptr<views::View> CreateStreetAddressView(
    const std::u16string& summary) {
  return views::Builder<views::Label>()
      .SetText(summary)
      .SetTextContext(views::style::CONTEXT_LABEL)
      .SetAccessibleName(l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_SAVE_PROMPT_ADDRESS_SECTION_A11Y_LABEL, summary))
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetMultiLine(true)
      .Build();
}

std::unique_ptr<views::EditableCombobox> CreateNicknameEditableCombobox() {
  // TODO(crbug.com/1167060): Update the icons
  // TODO(crbug.com/1167060): Use internationalized string.
  ui::SimpleComboboxModel::Item home(
      /*text=*/u"Home",
      /*dropdown_secondary_text=*/std::u16string(),
      /*icon=*/
      ui::ImageModel::FromVectorIcon(kNavigateHomeIcon, ui::kColorIcon,
                                     ComboboxIconSize()));

  ui::SimpleComboboxModel::Item work(
      /*text=*/u"Work",
      /*dropdown_secondary_text=*/std::u16string(),
      /*icon=*/
      ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                     ui::kColorIcon, ComboboxIconSize()));

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
  // Since this is a save prompt, original profile must not be set. Otherwise,
  // it would have been an update prompt.
  DCHECK(!controller_->GetOriginalProfile());

  // TODO(crbug.com/1167060): Accept action should consider the selected
  // nickname when saving the address.
  SetAcceptCallback(base::BindOnce(
      &SaveUpdateAddressProfileBubbleController::OnUserDecision,
      base::Unretained(controller_),
      AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted,
      std::nullopt));
  SetCancelCallback(
      base::BindOnce(&SaveUpdateAddressProfileBubbleController::OnUserDecision,
                     base::Unretained(controller_),
                     controller_->GetCancelCallbackValue(), std::nullopt));

  SetProperty(views::kElementIdentifierKey, kTopViewId);
  SetTitle(controller_->GetWindowTitle());
  SetButtonLabel(ui::DIALOG_BUTTON_OK, controller_->GetOkButtonLabel());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(
                     IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_CANCEL_BUTTON_LABEL));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  std::u16string description = controller->GetBodyText();
  if (!description.empty()) {
    AddChildView(
        views::Builder<views::Label>()
            .SetText(description)
            .SetTextStyle(views::style::STYLE_SECONDARY)
            .SetPreferredSize(
                gfx::Size(views::LayoutProvider::Get()->GetDistanceMetric(
                              views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                              margins().width(),
                          0))
            .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
            .SetMultiLine(true)
            .Build());
  }

  views::FlexLayoutView* details_section =
      AddChildView(views::Builder<views::FlexLayoutView>()
                       .SetOrientation(views::LayoutOrientation::kHorizontal)
                       .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
                       .SetIgnoreDefaultMainAxisMargins(true)
                       .SetCollapseMargins(true)
                       .SetInteriorMargin(gfx::Insets::TLBR(
                           0, 0,
                           ChromeLayoutProvider::Get()->GetDistanceMetric(
                               views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING),
                           0))
                       .Build());

  address_components_view_ = details_section->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetProperty(views::kFlexBehaviorKey,
                       views::FlexSpecification(
                           views::MinimumFlexSizeRule::kPreferredSnapToMinimum,
                           views::MaximumFlexSizeRule::kUnbounded))
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBetweenChildSpacing(
              ChromeLayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_CONTROL_LIST_VERTICAL))
          .Build());

  edit_button_ =
      details_section->AddChildView(CreateEditButton(base::BindRepeating(
          &SaveUpdateAddressProfileBubbleController::OnEditButtonClicked,
          base::Unretained(controller_))));
  edit_button_->SetProperty(views::kElementIdentifierKey, kEditButtonViewId);

  std::u16string address = controller_->GetAddressSummary();
  if (!address.empty()) {
    std::unique_ptr<views::ImageView> icon =
        CreateAddressSectionIcon(vector_icons::kLocationOnIcon);
    address_section_icons_.push_back(icon.get());
    AddAddressSection(/*parent_view=*/address_components_view_, std::move(icon),
                      CreateStreetAddressView(address));
  }

  std::u16string phone = controller_->GetProfilePhone();
  if (!phone.empty()) {
    std::unique_ptr<views::ImageView> icon =
        CreateAddressSectionIcon(vector_icons::kCallIcon);
    address_section_icons_.push_back(icon.get());
    AddAddressSection(
        /*parent_view=*/address_components_view_, std::move(icon), phone,
        IDS_AUTOFILL_SAVE_PROMPT_PHONE_SECTION_A11Y_LABEL);
  }

  std::u16string email = controller_->GetProfileEmail();
  if (!email.empty()) {
    std::unique_ptr<views::ImageView> icon =
        CreateAddressSectionIcon(vector_icons::kEmailIcon);
    address_section_icons_.push_back(icon.get());
    AddAddressSection(
        /*parent_view=*/address_components_view_, std::move(icon), email,
        IDS_AUTOFILL_SAVE_PROMPT_EMAIL_SECTION_A11Y_LABEL);
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillAddressProfileSavePromptNicknameSupport)) {
    // TODO(crbug.com/1167060): Make sure the icon is vertically centered with
    // the editable combobox.
    AddAddressSection(/*parent_view=*/address_components_view_,
                      CreateAddressSectionIcon(vector_icons::kExtensionIcon),
                      CreateNicknameEditableCombobox());
  }

  std::u16string footer_message = controller_->GetFooterMessage();
  if (!footer_message.empty()) {
    SetFootnoteView(
        views::Builder<views::Label>()
            .SetText(footer_message)
            .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
            .SetMultiLine(true)
            .Build());
  }
  AlignIcons();

  Profile* browser_profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HatsService* hats_service = HatsServiceFactory::GetForProfile(
      browser_profile, /*create_if_necessary=*/true);
  CHECK(hats_service);
  hats_service->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerAutofillAddress, web_contents, 10000);
}

SaveAddressProfileView::~SaveAddressProfileView() = default;

bool SaveAddressProfileView::ShouldShowCloseButton() const {
  return true;
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
  absl::optional<SaveUpdateAddressProfileBubbleController::HeaderImages>
      images = controller_->GetHeaderImages();
  if (images) {
    GetBubbleFrameView()->SetHeaderView(
        std::make_unique<ThemeTrackingNonAccessibleImageView>(
            images->light, images->dark,
            base::BindRepeating(
                &views::BubbleDialogDelegate::GetBackgroundColor,
                base::Unretained(this))));
  }
}

void SaveAddressProfileView::AlignIcons() {
  CHECK(edit_button_);
  CHECK(address_components_view_);
  // Adjust margins to make sure the edit button is vertically centered with the
  // first line in the address components view.
  int label_line_height = views::TypographyProvider::Get().GetLineHeight(
      views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY);
  for (views::ImageView* icon_view : address_section_icons_) {
    DCHECK(icon_view);
    // Set views::kMarginsKey for flex layout to center the icon vertically with
    // the text in front of it. Label line height are guaranteed to be bigger
    // than kIconSize.
    icon_view->SetProperty(
        views::kMarginsKey,
        gfx::Insets::VH((label_line_height - kIconSize) / 2, 0));
  }

  int edit_button_height = edit_button_->GetPreferredSize().height();
  int height_difference = (edit_button_height - label_line_height) / 2;
  if (height_difference > 0) {
    // We need to push the `address_components_view` down.
    address_components_view_->SetProperty(
        views::kMarginsKey, gfx::Insets::VH(height_difference, 0));
    edit_button_->SetProperty(views::kMarginsKey, gfx::Insets());
  } else {
    // We need to push the `edit_button` down.
    address_components_view_->SetProperty(views::kMarginsKey, gfx::Insets());
    edit_button_->SetProperty(views::kMarginsKey,
                              gfx::Insets::VH(-height_difference, 0));
  }
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SaveAddressProfileView, kTopViewId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SaveAddressProfileView,
                                      kEditButtonViewId);

}  // namespace autofill
