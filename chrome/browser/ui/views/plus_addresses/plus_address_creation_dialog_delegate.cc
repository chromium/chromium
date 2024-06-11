// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/plus_addresses/plus_address_creation_dialog_delegate.h"

#include <optional>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller_desktop.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_id.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/theme_tracking_image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "components/plus_addresses/resources/vector_icons.h"
#endif

namespace plus_addresses {

namespace {
const float kDescriptionWidthPercent = 0.8;
const int kProposedPlusAddressBackgroundCornerRadius = 8;
const int kPlusAddressIconWidth = 24;
// TODO(b/342330801): Figure out the correct size for the refresh icon.
const int kRefreshButtonIconWidth = 16;
const int kGoogleGLogoWidth = 48;
const int kPlusAddressLogoWidth = 96;
const int kPlusAddressIconColumnWidth = 64;
const int kPlusAddressRefreshColumnWidth = 48;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const gfx::VectorIcon& kGoogleGLogoIcon = vector_icons::kGoogleGLogoIcon;
const gfx::VectorIcon& kDarkGoogleGLogoIcon =
    vector_icons::kGoogleGLogoMonochromeIcon;
const gfx::VectorIcon& kPlusAddressLogoIcon =
    plus_addresses::kPlusAddressesLogoIcon;
const gfx::VectorIcon& kPlusAddressLogoLargeIcon =
    vector_icons::kPlusAddressLogoLargeIcon;
#else
const gfx::VectorIcon& kGoogleGLogoIcon = vector_icons::kProductIcon;
const gfx::VectorIcon& kDarkGoogleGLogoIcon = vector_icons::kProductIcon;
const gfx::VectorIcon& kPlusAddressLogoIcon = vector_icons::kProductIcon;
const gfx::VectorIcon& kPlusAddressLogoLargeIcon = vector_icons::kProductIcon;
#endif

int GetPlusAddressLabelVerticalMargin() {
  return base::FeatureList::IsEnabled(features::kPlusAddressUIRedesign) ? 24
                                                                        : 10;
}
}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressDescriptionTextElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressErrorTextElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressConfirmButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressCancelButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressRefreshButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressSuggestedEmailElementId);

PlusAddressCreationDialogDelegate::PlusAddressCreationDialogDelegate(
    base::WeakPtr<PlusAddressCreationController> controller,
    content::WebContents* web_contents,
    const std::string& primary_email_address,
    bool offer_refresh)
    : views::BubbleDialogDelegate(/*anchor_view=*/nullptr,
                                  views::BubbleBorder::Arrow::NONE),
      controller_(controller),
      web_contents_(web_contents) {
  const bool redesign_enabled =
      base::FeatureList::IsEnabled(features::kPlusAddressUIRedesign);
  // This delegate is owned & deleted by the PlusAddressCreationController.
  SetOwnedByWidget(false);
  RegisterDeleteDelegateCallback(base::BindOnce(
      [](base::WeakPtr<PlusAddressCreationController> controller) {
        controller->OnDialogDestroyed();
      },
      controller));
  SetModalType(ui::MODAL_TYPE_CHILD);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  SetShowCloseButton(!redesign_enabled);

  std::unique_ptr<views::View> primary_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .Build();

  // Create hero image.
  std::unique_ptr<views::ImageView> logo_image;
  if (redesign_enabled) {
    logo_image = std::make_unique<views::ThemeTrackingImageView>(
        ui::ImageModel::FromVectorIcon(kGoogleGLogoIcon, gfx::kPlaceholderColor,
                                       kGoogleGLogoWidth),
        ui::ImageModel::FromVectorIcon(kDarkGoogleGLogoIcon, ui::kColorIcon,
                                       kGoogleGLogoWidth),
        base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                            base::Unretained(this)));
    logo_image->SetProperty(
        views::kMarginsKey,
        gfx::Insets::VH(GetPlusAddressLabelVerticalMargin(), 0));
  } else {
    logo_image =
        views::Builder<views::ImageView>()
            .SetImage(ui::ImageModel::FromVectorIcon(
                kPlusAddressLogoIcon, ui::kColorIcon, kPlusAddressLogoWidth))
            .Build();
  }
  primary_view->AddChildView(std::move(logo_image));

  // Add title view.
  views::StyledLabel* modal_title = primary_view->AddChildView(
      views::Builder<views::StyledLabel>()
          .SetTextContext(views::style::STYLE_PRIMARY)
          .SetText(l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_TITLE))
          .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
          .SetDefaultTextStyle(views::style::STYLE_BODY_1_BOLD)
          .Build());

  views::StyledLabel* description_paragraph = primary_view->AddChildView(
      views::Builder<views::StyledLabel>()
          .SetHorizontalAlignment(gfx::ALIGN_CENTER)
          .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
          .Build());

  description_paragraph->SetProperty(views::kElementIdentifierKey,
                                     kPlusAddressDescriptionTextElementId);

  const std::u16string u16_primary_email_address =
      base::UTF8ToUTF16(primary_email_address);

  if (redesign_enabled) {
    modal_title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    description_paragraph->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    description_paragraph->SetText(l10n_util::GetStringFUTF16(
        IDS_PLUS_ADDRESS_MODAL_DESCRIPTION, {u16_primary_email_address}));
    description_paragraph->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(views::LayoutProvider::Get()->GetDistanceMetric(
                              views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING),
                          0, 0, 0));
  } else {
    modal_title->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    // Set the description text & update the styling.
    std::vector<size_t> description_offsets;
    // Prepend the settings link text with a newline to render it on one line.
    std::u16string settings_text = base::StrCat(
        {u"\n", l10n_util::GetStringUTF16(
                    IDS_PLUS_ADDRESS_MODAL_PLUS_ADDRESS_LINK_TEXT)});
    description_paragraph->SetText(l10n_util::GetStringFUTF16(
        IDS_PLUS_ADDRESS_MODAL_PLUS_ADDRESS_DESCRIPTION_START, {settings_text},
        &description_offsets));
    description_paragraph->SetHorizontalAlignment(gfx::ALIGN_CENTER);

    // Split the difference on both sides of the description.
    int horizontal_margin = (1 - kDescriptionWidthPercent) *
                            ChromeLayoutProvider::Get()->GetDistanceMetric(
                                views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) /
                            2;
    description_paragraph->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, horizontal_margin, 0, horizontal_margin));

    gfx::Range settings_text_range(
        description_offsets[0],
        description_offsets[0] + settings_text.length());
    views::StyledLabel::RangeStyleInfo settings_text_style =
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &PlusAddressCreationDialogDelegate::OpenSettingsLink,
            // Safe because this delegate outlives the Widget (and this view).
            base::Unretained(this), web_contents));
    description_paragraph->AddStyleRange(settings_text_range,
                                         settings_text_style);

    // Add the primary email address separately to avoid width constriction.
    views::StyledLabel* primary_email_address_view = primary_view->AddChildView(
        views::Builder<views::StyledLabel>()
            .SetHorizontalAlignment(gfx::ALIGN_CENTER)
            .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
            .Build());

    // Set the primary email address & update the styling.
    std::vector<size_t> email_address_offsets;
    primary_email_address_view->SetText(l10n_util::GetStringFUTF16(
        IDS_PLUS_ADDRESS_MODAL_PLUS_ADDRESS_DESCRIPTION_END,
        {u16_primary_email_address}, &email_address_offsets));

    views::StyledLabel::RangeStyleInfo email_address_style;
    email_address_style.text_style = views::style::TextStyle::STYLE_EMPHASIZED;
    primary_email_address_view->AddStyleRange(
        gfx::Range(
            email_address_offsets[0],
            email_address_offsets[0] + u16_primary_email_address.length()),
        email_address_style);
  }

  // Create a bubble for the plus address to be displayed in.
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int kRectangleRadius =
      redesign_enabled ? kProposedPlusAddressBackgroundCornerRadius
                       : provider->GetCornerRadiusMetric(
                             views::ShapeContextTokens::kDialogRadius);

  std::unique_ptr<views::Background> background =
      views::CreateThemedRoundedRectBackground(
          // TODO(b/342330801): Figure out the correct color for the background
          // and move the definition to the mixer.
          redesign_enabled ? ui::kColorSysHeaderContainer
                           : ui::kColorSubtleEmphasisBackground,
          kRectangleRadius);

  plus_address_label_container_ =
      primary_view->AddChildView(views::Builder<views::TableLayoutView>()
                                     .SetBackground(std::move(background))
                                     .Build());

  const bool add_plus_address_icon = redesign_enabled;
  plus_address_label_container_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(GetPlusAddressLabelVerticalMargin(), 0));
  if (add_plus_address_icon) {
    plus_address_label_container_->AddColumn(
        views::LayoutAlignment::kCenter, views::LayoutAlignment::kCenter,
        views::TableLayout::kFixedSize, views::TableLayout::ColumnSize::kFixed,
        kPlusAddressIconColumnWidth, 0);
  } else if (offer_refresh) {
    plus_address_label_container_->AddPaddingColumn(
        views::TableLayout::kFixedSize, kPlusAddressRefreshColumnWidth);
  }
  plus_address_label_container_->AddColumn(
      add_plus_address_icon ? views::LayoutAlignment::kStart
                            : views::LayoutAlignment::kCenter,
      views::LayoutAlignment::kCenter, 1.0f,
      views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
  if (offer_refresh) {
    plus_address_label_container_->AddColumn(
        views::LayoutAlignment::kStart, views::LayoutAlignment::kStretch,
        views::TableLayout::kFixedSize, views::TableLayout::ColumnSize::kFixed,
        kPlusAddressRefreshColumnWidth, 0);
  } else if (add_plus_address_icon) {
    plus_address_label_container_->AddPaddingColumn(
        views::TableLayout::kFixedSize, kPlusAddressIconColumnWidth);
  }
  plus_address_label_container_->AddRows(1, views::TableLayout::kFixedSize);

  if (add_plus_address_icon) {
    plus_address_label_container_->AddChildView(
        views::Builder<views::ImageView>()
            .SetImage(ui::ImageModel::FromVectorIcon(kPlusAddressLogoLargeIcon,
                                                     ui::kColorIcon,
                                                     kPlusAddressIconWidth))
            .Build());
  }

  plus_address_label_ = plus_address_label_container_->AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_PLUS_ADDRESS_MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER))
          .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
          .SetTextStyle(views::style::STYLE_PRIMARY)
          .Build());
  if (redesign_enabled) {
    plus_address_label_->SetTextContext(views::style::CONTEXT_LABEL);
    plus_address_label_->SetTextStyle(STYLE_SECONDARY_MONOSPACED);
  }
  plus_address_label_->SetProperty(views::kElementIdentifierKey,
                                   kPlusAddressSuggestedEmailElementId);
  plus_address_label_->SetSelectable(true);
  plus_address_label_->SetLineHeight(2 * plus_address_label_->GetLineHeight());

  // The refresh button.
  if (offer_refresh) {
    refresh_button_ = plus_address_label_container_->AddChildView(
        views::CreateVectorImageButton(base::BindRepeating(
            &PlusAddressCreationDialogDelegate::OnRefreshClicked,
            base::Unretained(this))));
    views::SetImageFromVectorIconWithColorId(
        refresh_button_, vector_icons::kReloadIcon, ui::kColorIcon,
        ui::kColorIcon, kRefreshButtonIconWidth);
    refresh_button_->SetProperty(views::kElementIdentifierKey,
                                 kPlusAddressRefreshButtonElementId);
    refresh_button_->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        IDS_PLUS_ADDRESS_MODAL_REFRESH_BUTTON_ACCESSIBLE_NAME));
    refresh_button_->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, 8)));
  }

  // Create and hide label for bug report instruction.
  std::vector<size_t> error_link_offsets;
  std::u16string error_link_text =
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_ERROR_REPORT_LINK_TEXT);
  error_report_label_ = primary_view->AddChildView(
      views::Builder<views::StyledLabel>()
          .SetHorizontalAlignment(redesign_enabled ? gfx::ALIGN_LEFT
                                                   : gfx::ALIGN_CENTER)
          .SetText(l10n_util::GetStringFUTF16(
              IDS_PLUS_ADDRESS_MODAL_REPORT_ERROR_INSTRUCTION_DESKTOP,
              {error_link_text}, &error_link_offsets))
          .SetTextContext(views::style::CONTEXT_BUBBLE_FOOTER)
          .SetDefaultTextStyle(views::style::STYLE_HINT)
          .SetVisible(false)
          .Build());
  error_report_label_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(GetPlusAddressLabelVerticalMargin(), 0));
  error_report_label_->SetProperty(views::kElementIdentifierKey,
                                   kPlusAddressErrorTextElementId);
  // Update style for error link.
  gfx::Range error_link_range(error_link_offsets[0],
                              error_link_offsets[0] + error_link_text.length());
  views::StyledLabel::RangeStyleInfo error_link_text_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &PlusAddressCreationDialogDelegate::OpenErrorReportLink,
          // Safe because this delegate outlives the Widget (and this view).
          base::Unretained(this), web_contents));
  error_report_label_->AddStyleRange(error_link_range, error_link_text_style);

  // Avoid using the builtin DialogDelegate buttons so that we can use
  // GetWidget()->Close() to close the UI when ready.
  SetButtons(ui::DIALOG_BUTTON_NONE);

  // Initialize buttons.
  views::BoxLayoutView* buttons_view = primary_view->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd)
          .SetBetweenChildSpacing(provider->GetDistanceMetric(
              views::DistanceMetric::DISTANCE_RELATED_BUTTON_HORIZONTAL))
          .Build());

  cancel_button_ =
      buttons_view->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(
              &PlusAddressCreationDialogDelegate::HandleButtonPress,
              // Safe because this delegate outlives the Widget (and this view).
              base::Unretained(this), PlusAddressViewButtonType::kCancel),
          l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_CANCEL_TEXT)));
  cancel_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_CANCEL_TEXT));
  cancel_button_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_CANCEL_TEXT));
  cancel_button_->SizeToPreferredSize();
  cancel_button_->SetProperty(views::kElementIdentifierKey,
                              kPlusAddressCancelButtonElementId);
  if (redesign_enabled) {
    cancel_button_->SetStyle(ui::ButtonStyle::kTonal);
  }

  confirm_button_ =
      buttons_view->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(
              &PlusAddressCreationDialogDelegate::HandleButtonPress,
              // Safe because this delegate outlives the Widget (and this view).
              base::Unretained(this), PlusAddressViewButtonType::kConfirm),
          l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_OK_TEXT)));
  confirm_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_OK_TEXT));
  confirm_button_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_OK_TEXT));
  confirm_button_->SizeToPreferredSize();
  confirm_button_->SetStyle(ui::ButtonStyle::kProminent);
  confirm_button_->SetEnabled(false);
  confirm_button_->SetProperty(views::kElementIdentifierKey,
                               kPlusAddressConfirmButtonElementId);

  SetContentsView(std::move(primary_view));
}

PlusAddressCreationDialogDelegate::~PlusAddressCreationDialogDelegate() {
  plus_address_label_ = nullptr;
}

void PlusAddressCreationDialogDelegate::OnWidgetInitialized() {
  if (views::BubbleFrameView* frame = GetBubbleFrameView(); frame) {
    frame->close_button()->SetCallback(
        views::Button::PressedCallback(base::BindRepeating(
            &PlusAddressCreationDialogDelegate::HandleButtonPress,
            // Safe because this outlives the BubbleFrameView.
            base::Unretained(this), PlusAddressViewButtonType::kClose)));
  }
}

void PlusAddressCreationDialogDelegate::OpenSettingsLink(
    content::WebContents* web_contents) {
  if (web_contents && !features::kPlusAddressManagementUrl.Get().empty()) {
    web_contents->OpenURL(
        content::OpenURLParams(GURL(features::kPlusAddressManagementUrl.Get()),
                               content::Referrer(),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui::PageTransition::PAGE_TRANSITION_LINK,
                               /*is_renderer_initiated=*/false),
        /*navigation_handle_callback=*/{});
  }
}

// TODO(b/313670457) Test open link behaviors when migrate to Kombucha.
void PlusAddressCreationDialogDelegate::OpenErrorReportLink(
    content::WebContents* web_contents) {
  if (web_contents && !features::kPlusAddressErrorReportUrl.Get().empty()) {
    web_contents->OpenURL(
        content::OpenURLParams(GURL(features::kPlusAddressErrorReportUrl.Get()),
                               content::Referrer(),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui::PageTransition::PAGE_TRANSITION_LINK,
                               /*is_renderer_initiated=*/false),
        /*navigation_handle_callback=*/{});
  }
}

void PlusAddressCreationDialogDelegate::HideRefreshButton() {
  if (!refresh_button_) {
    return;
  }
  std::unique_ptr<views::View> button =
      refresh_button_->parent()->RemoveChildViewT(refresh_button_.get());
  refresh_button_ = nullptr;
  plus_address_label_->SetProperty(views::kMarginsKey, gfx::Insets());
}

void PlusAddressCreationDialogDelegate::ShowReserveResult(
    const PlusProfileOrError& maybe_plus_profile) {
  CHECK(plus_address_label_);

  if (maybe_plus_profile.has_value()) {
    plus_address_label_->SetText(
        base::UTF8ToUTF16(maybe_plus_profile->plus_address));
    confirm_button_->SetEnabled(true);
  } else {
    ShowErrorStateUI();
  }
}

void PlusAddressCreationDialogDelegate::ShowConfirmResult(
    const PlusProfileOrError& maybe_plus_profile) {
  CHECK(plus_address_label_);
  CHECK(GetWidget());
  CHECK(GetBubbleFrameView());

  // Stop indicating loading now that we have the server response.
  GetBubbleFrameView()->SetProgress(std::nullopt);

  if (maybe_plus_profile.has_value()) {
    GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kAcceptButtonClicked);
  } else {
    ShowErrorStateUI();
    confirm_button_->SetEnabled(false);
  }
}

void PlusAddressCreationDialogDelegate::HandleButtonPress(
    PlusAddressViewButtonType type) {
  CHECK(GetBubbleFrameView());

  switch (type) {
    case PlusAddressViewButtonType::kConfirm: {
      controller_->OnConfirmed();
      // Show a progress bar that loops until the Confirm request is resolved.
      GetBubbleFrameView()->SetProgress(-1);
      return;
    }
    case PlusAddressViewButtonType::kCancel: {
      controller_->OnCanceled();
      GetWidget()->CloseWithReason(
          views::Widget::ClosedReason::kCancelButtonClicked);
      return;
    }
    case PlusAddressViewButtonType::kClose: {
      controller_->OnCanceled();
      GetWidget()->CloseWithReason(
          views::Widget::ClosedReason::kCloseButtonClicked);
      return;
    }
  }
}

void PlusAddressCreationDialogDelegate::ShowErrorStateUI() {
  CHECK(GetWidget() && web_contents_);
  plus_address_label_container_->SetVisible(false);
  // Show the error report instructions.
  error_report_label_->SetVisible(true);
  // Update the size of modal.
  constrained_window::UpdateWebContentsModalDialogPosition(
      GetWidget(),
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_)
          ->delegate()
          ->GetWebContentsModalDialogHost());
}

void PlusAddressCreationDialogDelegate::OnRefreshClicked() {
  plus_address_label_->SetText(l10n_util::GetStringUTF16(
      IDS_PLUS_ADDRESS_MODAL_REFRESH_TEMPORARY_LABEL_CONTENT));
  confirm_button_->SetEnabled(false);
  controller_->OnRefreshClicked();
}

}  // namespace plus_addresses
