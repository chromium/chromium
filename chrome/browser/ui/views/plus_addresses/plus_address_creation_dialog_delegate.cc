// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/plus_addresses/plus_address_creation_dialog_delegate.h"

#include <optional>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
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
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
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

constexpr int kProposedPlusAddressBackgroundCornerRadius = 8;
constexpr int kPlusAddressIconWidth = 24;
// TODO(crbug.com/342330801): Figure out the correct size for the refresh icon.
constexpr int kRefreshButtonIconWidth = 16;
constexpr int kGoogleGLogoWidth = 48;
constexpr int kPlusAddressIconColumnWidth = 64;
constexpr int kPlusAddressRefreshColumnWidth = 48;
constexpr int kPlusAddressLabelVerticalMargin = 24;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const gfx::VectorIcon& kGoogleGLogoIcon = vector_icons::kGoogleGLogoIcon;
const gfx::VectorIcon& kDarkGoogleGLogoIcon =
    vector_icons::kGoogleGLogoMonochromeIcon;
const gfx::VectorIcon& kLogoLargeIcon =
    plus_addresses::kPlusAddressLogoLargeIcon;
#else
const gfx::VectorIcon& kGoogleGLogoIcon = vector_icons::kProductIcon;
const gfx::VectorIcon& kDarkGoogleGLogoIcon = vector_icons::kProductIcon;
const gfx::VectorIcon& kLogoLargeIcon = vector_icons::kProductIcon;
#endif

// Helper function that opens a link in `web_contents` if `web_contents` is not
// null and the link is not empty.
void OpenLink(content::WebContents* web_contents, const GURL& url) {
  if (!web_contents || url.is_empty()) {
    return;
  }
  web_contents->OpenURL(
      content::OpenURLParams(url, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PageTransition::PAGE_TRANSITION_LINK,
                             /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
}

// Opens a link to report errors with plus addresses.
void OpenErrorReportingLink(content::WebContents* web_contents) {
  OpenLink(web_contents, GURL(features::kPlusAddressErrorReportUrl.Get()));
}

// Opens a link to learn more about plus addresses.
void OpenLearnMoreLink(content::WebContents* web_contents) {
  OpenLink(web_contents, GURL(features::kPlusAddressLearnMoreUrl.Get()));
}

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView, kTopViewId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressTitleElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressDescriptionTextElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressNoticeElementId);
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
    bool offer_refresh,
    bool show_notice)
    : views::BubbleDialogDelegate(/*anchor_view=*/nullptr,
                                  views::BubbleBorder::Arrow::NONE),
      controller_(controller),
      web_contents_(web_contents) {
  // This delegate is owned & deleted by the PlusAddressCreationController.
  SetOwnedByWidget(false);
  RegisterDeleteDelegateCallback(base::BindOnce(
      [](base::WeakPtr<PlusAddressCreationController> controller) {
        controller->OnDialogDestroyed();
      },
      controller));
  SetModalType(ui::mojom::ModalType::kChild);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  std::unique_ptr<views::View> primary_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .Build();
  primary_view->SetProperty(views::kElementIdentifierKey, kTopViewId);

  // Create hero image.
  auto logo_image = std::make_unique<views::ThemeTrackingImageView>(
      ui::ImageModel::FromVectorIcon(kGoogleGLogoIcon, gfx::kPlaceholderColor,
                                     kGoogleGLogoWidth),
      ui::ImageModel::FromVectorIcon(kDarkGoogleGLogoIcon, ui::kColorIcon,
                                     kGoogleGLogoWidth),
      base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                          base::Unretained(this)));
  logo_image->SetProperty(views::kMarginsKey,
                          gfx::Insets::VH(kPlusAddressLabelVerticalMargin, 0));
  primary_view->AddChildView(std::move(logo_image));

  // The title.
  primary_view->AddChildView(
      views::Builder<views::StyledLabel>()
          .SetProperty(views::kElementIdentifierKey, kPlusAddressTitleElementId)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetTextContext(views::style::STYLE_PRIMARY)
          .SetText(l10n_util::GetStringUTF16(
              show_notice ? IDS_PLUS_ADDRESS_MODAL_TITLE_NOTICE
                          : IDS_PLUS_ADDRESS_MODAL_TITLE))
          .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
          .SetDefaultTextStyle(views::style::STYLE_BODY_1_BOLD)
          .Build());

  // The description.
  primary_view->AddChildView(
      views::Builder<views::StyledLabel>()
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
          .SetProperty(views::kElementIdentifierKey,
                       kPlusAddressDescriptionTextElementId)
          .SetProperty(views::kMarginsKey,
                       gfx::Insets::TLBR(
                           views::LayoutProvider::Get()->GetDistanceMetric(
                               views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING),
                           0, 0, 0))
          .SetText(show_notice
                       ? l10n_util::GetStringUTF16(
                             IDS_PLUS_ADDRESS_MODAL_DESCRIPTION_NOTICE)
                       : l10n_util::GetStringFUTF16(
                             IDS_PLUS_ADDRESS_MODAL_DESCRIPTION,
                             {base::UTF8ToUTF16(primary_email_address)}))
          .Build());

  // Create a bubble for the plus address to be displayed in.
  std::unique_ptr<views::Background> background =
      views::CreateThemedRoundedRectBackground(
          // TODO(b/342330801): Figure out the correct color for the background
          // and move the definition to the mixer.
          ui::kColorSysHeaderContainer,
          kProposedPlusAddressBackgroundCornerRadius);

  plus_address_label_container_ =
      primary_view->AddChildView(views::Builder<views::TableLayoutView>()
                                     .SetBackground(std::move(background))
                                     .Build());

  plus_address_label_container_->SetProperty(
      views::kMarginsKey, gfx::Insets::VH(kPlusAddressLabelVerticalMargin, 0));
  plus_address_label_container_->AddColumn(
      views::LayoutAlignment::kCenter, views::LayoutAlignment::kCenter,
      views::TableLayout::kFixedSize, views::TableLayout::ColumnSize::kFixed,
      kPlusAddressIconColumnWidth, 0);
  plus_address_label_container_->AddColumn(
      views::LayoutAlignment::kStart, views::LayoutAlignment::kCenter, 1.0f,
      views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
  if (offer_refresh) {
    plus_address_label_container_->AddColumn(
        views::LayoutAlignment::kStart, views::LayoutAlignment::kStretch,
        views::TableLayout::kFixedSize, views::TableLayout::ColumnSize::kFixed,
        kPlusAddressRefreshColumnWidth, 0);
  } else {
    plus_address_label_container_->AddPaddingColumn(
        views::TableLayout::kFixedSize, kPlusAddressIconColumnWidth);
  }
  plus_address_label_container_->AddRows(1, views::TableLayout::kFixedSize);

  plus_address_label_container_->AddChildView(
      views::Builder<views::ImageView>()
          .SetImage(ui::ImageModel::FromVectorIcon(
              kLogoLargeIcon, ui::kColorIcon, kPlusAddressIconWidth))
          .Build());

  plus_address_label_ = plus_address_label_container_->AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_PLUS_ADDRESS_MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER))
          .SetTextContext(views::style::CONTEXT_LABEL)
          .SetTextStyle(STYLE_SECONDARY_MONOSPACED)
          .SetProperty(views::kElementIdentifierKey,
                       kPlusAddressSuggestedEmailElementId)
          .SetSelectable(true)
          .Build());
  plus_address_label_->SetLineHeight(2 * plus_address_label_->GetLineHeight());

  // The refresh button.
  if (offer_refresh) {
    refresh_button_ = plus_address_label_container_->AddChildView(
        views::CreateVectorImageButtonWithNativeTheme(
            base::BindRepeating(
                &PlusAddressCreationDialogDelegate::OnRefreshClicked,
                base::Unretained(this)),
            vector_icons::kReloadIcon, kRefreshButtonIconWidth));
    views::InstallCircleHighlightPathGenerator(refresh_button_.get());
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
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetText(l10n_util::GetStringFUTF16(
              IDS_PLUS_ADDRESS_MODAL_REPORT_ERROR_INSTRUCTION_DESKTOP,
              {error_link_text}, &error_link_offsets))
          .SetTextContext(views::style::CONTEXT_BUBBLE_FOOTER)
          .SetDefaultTextStyle(views::style::STYLE_HINT)
          .SetVisible(false)
          .SetProperty(views::kMarginsKey,
                       gfx::Insets::VH(kPlusAddressLabelVerticalMargin, 0))
          .SetProperty(views::kElementIdentifierKey,
                       kPlusAddressErrorTextElementId)
          .Build());
  // Update style for error link.
  gfx::Range error_link_range(error_link_offsets[0],
                              error_link_offsets[0] + error_link_text.length());
  const auto error_link_text_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          base::BindRepeating(&OpenErrorReportingLink, web_contents));
  error_report_label_->AddStyleRange(error_link_range, error_link_text_style);

  if (show_notice) {
    std::vector<size_t> replacement_offsets;
    const std::u16string learn_more_link_text = l10n_util::GetStringUTF16(
        IDS_PLUS_ADDRESS_MODAL_NOTICE_LEARN_MORE_LINK_TEXT);
    auto* notice_label = primary_view->AddChildView(
        views::Builder<views::StyledLabel>()
            .SetProperty(views::kElementIdentifierKey,
                         kPlusAddressNoticeElementId)
            .SetHorizontalAlignment(gfx::ALIGN_LEFT)
            .SetDefaultTextStyle(views::style::TextStyle::STYLE_BODY_5)
            .SetDefaultEnabledColorId(ui::kColorLabelForegroundSecondary)
            .SetText(l10n_util::GetStringFUTF16(
                IDS_PLUS_ADDRESS_MODAL_NOTICE,
                /*replacements=*/
                {base::UTF8ToUTF16(primary_email_address),
                 learn_more_link_text},
                &replacement_offsets))
            .SetProperty(
                views::kMarginsKey,
                gfx::Insets::TLBR(0, 0, kPlusAddressLabelVerticalMargin, 0))
            .Build());
    auto learn_more_link_text_style =
        views::StyledLabel::RangeStyleInfo::CreateForLink(
            base::BindRepeating(&OpenLearnMoreLink, web_contents));
    learn_more_link_text_style.text_style =
        views::style::TextStyle::STYLE_LINK_5;
    notice_label->AddStyleRange(
        gfx::Range(replacement_offsets[1],
                   replacement_offsets[1] + learn_more_link_text.length()),
        learn_more_link_text_style);
  }

  // Avoid using the builtin DialogDelegate buttons so that we can use
  // GetWidget()->Close() to close the UI when ready.
  SetButtons(ui::DIALOG_BUTTON_NONE);

  // Initialize buttons.
  primary_view->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd)
          .SetBetweenChildSpacing(
              ChromeLayoutProvider::Get()->GetDistanceMetric(
                  views::DistanceMetric::DISTANCE_RELATED_BUTTON_HORIZONTAL))
          .AddChildren(
              views::Builder<views::MdTextButton>()
                  .SetCallback(base::BindRepeating(
                      &PlusAddressCreationDialogDelegate::HandleButtonPress,
                      // Safe because this delegate outlives the Widget (and
                      // this view).
                      base::Unretained(this),
                      PlusAddressViewButtonType::kCancel))
                  .SetText(l10n_util::GetStringUTF16(
                      IDS_PLUS_ADDRESS_MODAL_CANCEL_TEXT))
                  .SetTooltipText(l10n_util::GetStringUTF16(
                      IDS_PLUS_ADDRESS_MODAL_CANCEL_TEXT))
                  .SetProperty(views::kElementIdentifierKey,
                               kPlusAddressCancelButtonElementId)
                  .SetStyle(ui::ButtonStyle::kTonal)
                  .SetAccessibleName(l10n_util::GetStringUTF16(
                      IDS_PLUS_ADDRESS_MODAL_CANCEL_TEXT)),
              views::Builder<views::MdTextButton>()
                  .CopyAddressTo(&confirm_button_)
                  .SetCallback(base::BindRepeating(
                      &PlusAddressCreationDialogDelegate::HandleButtonPress,
                      // Safe because this delegate outlives the Widget (and
                      // this view).
                      base::Unretained(this),
                      PlusAddressViewButtonType::kConfirm))
                  .SetText(
                      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_OK_TEXT))
                  .SetTooltipText(
                      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_OK_TEXT))
                  .SetStyle(ui::ButtonStyle::kProminent)
                  .SetEnabled(false)
                  .SetProperty(views::kElementIdentifierKey,
                               kPlusAddressConfirmButtonElementId)
                  .SetAccessibleName(l10n_util::GetStringUTF16(
                      IDS_PLUS_ADDRESS_MODAL_OK_TEXT)))
          .Build());

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
        base::UTF8ToUTF16(*maybe_plus_profile->plus_address));
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
