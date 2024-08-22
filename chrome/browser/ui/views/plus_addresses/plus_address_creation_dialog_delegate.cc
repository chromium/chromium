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
#include "ui/base/mojom/dialog_button.mojom.h"
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
#include "ui/views/layout/layout_types.h"
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

std::unique_ptr<views::View> CreateTitle(bool show_notice) {
  return views::Builder<views::StyledLabel>()
      .SetProperty(views::kElementIdentifierKey,
                   PlusAddressCreationView::kPlusAddressTitleElementId)
      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
      .SetTextContext(views::style::STYLE_PRIMARY)
      .SetText(l10n_util::GetStringUTF16(
          show_notice ? IDS_PLUS_ADDRESS_MODAL_TITLE_NOTICE
                      : IDS_PLUS_ADDRESS_MODAL_TITLE))
      .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
      .SetDefaultTextStyle(views::style::STYLE_BODY_1_BOLD)
      .Build();
}

std::unique_ptr<views::View> CreateDescription(
    bool show_notice,
    std::string_view primary_email_address) {
  return views::Builder<views::StyledLabel>()
      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
      .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
      .SetProperty(
          views::kElementIdentifierKey,
          PlusAddressCreationView::kPlusAddressDescriptionTextElementId)
      .SetProperty(
          views::kMarginsKey,
          gfx::Insets::TLBR(views::LayoutProvider::Get()->GetDistanceMetric(
                                views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING),
                            0, 0, 0))
      .SetText(show_notice ? l10n_util::GetStringUTF16(
                                 IDS_PLUS_ADDRESS_MODAL_DESCRIPTION_NOTICE)
                           : l10n_util::GetStringFUTF16(
                                 IDS_PLUS_ADDRESS_MODAL_DESCRIPTION,
                                 {base::UTF8ToUTF16(primary_email_address)}))
      .Build();
}

// Creates a view that contains the box with an icon, the proposed plus address,
// and if `offer_refresh` is true, a refresh button.
std::unique_ptr<views::TableLayoutView> CreatePlusAddressLabelContainer(
    bool offer_refresh) {
  // Create a bubble for the plus address to be displayed in.
  std::unique_ptr<views::Background> background =
      views::CreateThemedRoundedRectBackground(
          // TODO(b/342330801): Figure out the correct color for the background
          // and move the definition to the mixer.
          ui::kColorSysHeaderContainer,
          kProposedPlusAddressBackgroundCornerRadius);

  using Alignment = views::LayoutAlignment;
  using ColumnSize = views::TableLayout::ColumnSize;
  auto container = views::Builder<views::TableLayoutView>()
                       .SetBackground(std::move(background))
                       .Build();
  container->SetProperty(views::kMarginsKey,
                         gfx::Insets::VH(kPlusAddressLabelVerticalMargin, 0));
  container->AddColumn(Alignment::kCenter, Alignment::kCenter,
                       views::TableLayout::kFixedSize, ColumnSize::kFixed,
                       kPlusAddressIconColumnWidth, 0);
  container->AddColumn(Alignment::kStart, Alignment::kCenter, 1.0f,
                       ColumnSize::kUsePreferred, 0, 0);
  if (offer_refresh) {
    container->AddColumn(Alignment::kStart, Alignment::kStretch,
                         views::TableLayout::kFixedSize, ColumnSize::kFixed,
                         kPlusAddressRefreshColumnWidth, 0);
  } else {
    container->AddPaddingColumn(views::TableLayout::kFixedSize,
                                kPlusAddressIconColumnWidth);
  }
  return container;
}

// Creates the label that holds the plus address or a progress message.
std::unique_ptr<views::Label> CreatePlusAddressLabel() {
  auto label =
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_PLUS_ADDRESS_MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER))
          .SetTextContext(views::style::CONTEXT_LABEL)
          .SetTextStyle(STYLE_SECONDARY_MONOSPACED)
          .SetProperty(
              views::kElementIdentifierKey,
              PlusAddressCreationView::kPlusAddressSuggestedEmailElementId)
          .SetSelectable(true)
          .Build();
  label->SetLineHeight(2 * label->GetLineHeight());
  return label;
}

// Creates a (by-default invisible) label for reporting errors.
std::unique_ptr<views::View> CreateErrorReportLabel(
    content::WebContents* web_contents) {
  std::vector<size_t> error_link_offsets;
  std::u16string error_link_text =
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_ERROR_REPORT_LINK_TEXT);
  auto label =
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
                       PlusAddressCreationView::kPlusAddressErrorTextElementId)
          .Build();
  // Update the style for the error link.
  gfx::Range error_link_range(error_link_offsets[0],
                              error_link_offsets[0] + error_link_text.length());
  const auto error_link_text_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          base::BindRepeating(&OpenErrorReportingLink, web_contents));
  label->AddStyleRange(error_link_range, error_link_text_style);
  return label;
}

// Creates the view containing the legal notice and a link to learn more.
std::unique_ptr<views::View> CreateNotice(
    content::WebContents* web_contents,
    std::string_view primary_email_address) {
  std::vector<size_t> replacement_offsets;
  const std::u16string learn_more_link_text = l10n_util::GetStringUTF16(
      IDS_PLUS_ADDRESS_MODAL_NOTICE_LEARN_MORE_LINK_TEXT);
  auto notice =
      views::Builder<views::StyledLabel>()
          .SetProperty(views::kElementIdentifierKey,
                       PlusAddressCreationView::kPlusAddressNoticeElementId)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetDefaultTextStyle(views::style::TextStyle::STYLE_BODY_5)
          .SetDefaultEnabledColorId(ui::kColorLabelForegroundSecondary)
          .SetText(l10n_util::GetStringFUTF16(
              IDS_PLUS_ADDRESS_MODAL_NOTICE,
              /*replacements=*/
              {base::UTF8ToUTF16(primary_email_address), learn_more_link_text},
              &replacement_offsets))
          .SetProperty(
              views::kMarginsKey,
              gfx::Insets::TLBR(0, 0, kPlusAddressLabelVerticalMargin, 0))
          .Build();
  auto learn_more_link_text_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          base::BindRepeating(&OpenLearnMoreLink, web_contents));
  learn_more_link_text_style.text_style = views::style::TextStyle::STYLE_LINK_5;
  notice->AddStyleRange(
      gfx::Range(replacement_offsets[1],
                 replacement_offsets[1] + learn_more_link_text.length()),
      learn_more_link_text_style);
  return notice;
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

  // Avoid using the builtin DialogDelegate buttons so that we can use
  // GetWidget()->Close() to close the UI when ready.
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  // This view will act as the contents view for the bubble. It is used to be
  // able to set empty margins on `this` and, instead, set them on
  // `primary_view`. This allows adding an element at the top of the dialog that
  // has no margins.
  std::unique_ptr<views::View> wrapper_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .Build();
  std::unique_ptr<views::View> primary_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetProperty(views::kElementIdentifierKey, kTopViewId)
          .SetProperty(views::kMarginsKey, margins())
          .Build();
  set_margins(gfx::Insets());

  primary_view->AddChildView(CreateLogo());
  primary_view->AddChildView(CreateTitle(show_notice));
  primary_view->AddChildView(
      CreateDescription(show_notice, primary_email_address));

  // The container that contains the suggested plus address (or a loading
  // message).
  plus_address_label_container_ = primary_view->AddChildView(
      CreatePlusAddressLabelContainer(offer_refresh));
  plus_address_label_container_->AddRows(1, views::TableLayout::kFixedSize);
  plus_address_label_container_->AddChildView(
      views::Builder<views::ImageView>()
          .SetImage(ui::ImageModel::FromVectorIcon(
              kLogoLargeIcon, ui::kColorIcon, kPlusAddressIconWidth))
          .Build());
  plus_address_label_ =
      plus_address_label_container_->AddChildView(CreatePlusAddressLabel());
  if (offer_refresh) {
    refresh_button_ =
        plus_address_label_container_->AddChildView(CreateRefreshButton());
  }

  // The error report label is hidden by default.
  error_report_label_ =
      primary_view->AddChildView(CreateErrorReportLabel(web_contents));

  if (show_notice) {
    primary_view->AddChildView(
        CreateNotice(web_contents, primary_email_address));
  }
  primary_view->AddChildView(CreateButtons());

  wrapper_view->AddChildView(std::move(primary_view));
  SetContentsView(std::move(wrapper_view));
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

std::unique_ptr<views::View> PlusAddressCreationDialogDelegate::CreateLogo() {
  auto logo = std::make_unique<views::ThemeTrackingImageView>(
      ui::ImageModel::FromVectorIcon(kGoogleGLogoIcon, gfx::kPlaceholderColor,
                                     kGoogleGLogoWidth),
      ui::ImageModel::FromVectorIcon(kDarkGoogleGLogoIcon, ui::kColorIcon,
                                     kGoogleGLogoWidth),
      base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                          base::Unretained(this)));
  logo->SetProperty(views::kMarginsKey,
                    gfx::Insets::VH(kPlusAddressLabelVerticalMargin, 0));
  return logo;
}

std::unique_ptr<views::ImageButton>
PlusAddressCreationDialogDelegate::CreateRefreshButton() {
  auto button = views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(&PlusAddressCreationDialogDelegate::OnRefreshClicked,
                          base::Unretained(this)),
      vector_icons::kReloadIcon, kRefreshButtonIconWidth);
  views::InstallCircleHighlightPathGenerator(button.get());
  button->SetProperty(views::kElementIdentifierKey,
                      kPlusAddressRefreshButtonElementId);
  button->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_PLUS_ADDRESS_MODAL_REFRESH_BUTTON_ACCESSIBLE_NAME));
  button->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, 8)));
  return button;
}

std::unique_ptr<views::View>
PlusAddressCreationDialogDelegate::CreateButtons() {
  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
      .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd)
      .SetBetweenChildSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_RELATED_BUTTON_HORIZONTAL))
      .AddChildren(
          views::Builder<views::MdTextButton>()
              .SetCallback(base::BindRepeating(
                  &PlusAddressCreationDialogDelegate::HandleButtonPress,
                  // Safe because this delegate outlives the Widget (and
                  // this view).
                  base::Unretained(this), PlusAddressViewButtonType::kCancel))
              .SetText(
                  l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_CANCEL_TEXT))
              .SetTooltipText(
                  l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_CANCEL_TEXT))
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
                  base::Unretained(this), PlusAddressViewButtonType::kConfirm))
              .SetText(
                  l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_OK_TEXT))
              .SetTooltipText(
                  l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_OK_TEXT))
              .SetStyle(ui::ButtonStyle::kProminent)
              .SetEnabled(false)
              .SetProperty(views::kElementIdentifierKey,
                           kPlusAddressConfirmButtonElementId)
              .SetAccessibleName(
                  l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_OK_TEXT)))
      .Build();
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
