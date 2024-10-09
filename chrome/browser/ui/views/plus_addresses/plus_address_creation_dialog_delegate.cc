// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/plus_addresses/plus_address_creation_dialog_delegate.h"

#include <memory>
#include <optional>

#include "base/check_deref.h"
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
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/vector_icons/vector_icons.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
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

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressCancelButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressConfirmButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressDescriptionTextElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressErrorTextElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressGenerationMessageElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressNoticeElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressProgressBarId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressRefreshButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressReserveErrorId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressCreateErrorId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressSuggestedEmailElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView,
                                      kPlusAddressTitleElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PlusAddressCreationView, kTopViewId);

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

// Returns a progress bar that has the width of the modal, is animated
// continuously and is exempt from layout (and thus displayed at the top of the
// dialog).
std::unique_ptr<views::ProgressBar> CreateProgressBar() {
  static constexpr int kProgressBarHeight = 4;
  auto bar = std::make_unique<views::ProgressBar>();
  bar->SetPreferredCornerRadii(std::nullopt);
  bar->SetValue(-1);
  bar->SetBackgroundColor(SK_ColorTRANSPARENT);
  bar->SetPreferredSize(
      gfx::Size(ChromeLayoutProvider::Get()->GetDistanceMetric(
                    views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH),
                kProgressBarHeight));
  bar->SizeToPreferredSize();
  bar->SetProperty(views::kViewIgnoredByLayoutKey, true);
  bar->SetProperty(views::kElementIdentifierKey,
                   PlusAddressCreationView::kPlusAddressProgressBarId);
  return bar;
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

std::unique_ptr<views::Label> CreateErrorMessageLabel() {
  return views::Builder<views::Label>()
      .SetTextContext(views::style::CONTEXT_LABEL)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetProperty(views::kElementIdentifierKey,
                   PlusAddressCreationView::kPlusAddressCreateErrorId)
      .SetEnabledColorId(ui::kColorSysError)
      .SetProperty(views::kMarginsKey, gfx::Insets::TLBR(8, 0, 16, 0))
      .SetTextStyle(views::style::TextStyle::STYLE_BODY_5)
      .SetVisible(false)
      .Build();
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

// A view for the bubble consisting of either
// - plus address icon, suggested plus address, and refresh button, or
// - generation message and refresh button.
class PlusAddressCreationDialogDelegate::PlusAddressContainerView final
    : public views::TableLayoutView {
  METADATA_HEADER(PlusAddressContainerView, views::TableLayoutView)
 public:
  explicit PlusAddressContainerView(
      std::unique_ptr<views::ImageButton> refresh_button);
  ~PlusAddressContainerView() override = default;

  // Shows the suggested `address`.
  void ShowPlusAddress(const std::u16string& address);

  // Shows `error` instead of an address.
  void ShowError(const std::u16string& error);

  enum class Icon { kError, kPlusAddress };
  // Shows `icon` in the container for the suggested plus address.
  void ShowIcon(Icon icon);

  // Hides the container for the suggested plus address and instead shows
  // a "Generating ..." message.
  void ShowGenerationMessage();

  void ShowRefresh(bool visible);
  void SetEnabledForRefreshButton(bool enabled);

 private:
  using Alignment = views::LayoutAlignment;
  using ColumnSize = views::TableLayout::ColumnSize;

  // Creates a view that contains a plus address icon and the label for the
  // suggested plus address.
  std::unique_ptr<views::View> CreatePlusAddressLabelWithIcon();

  // A container for the plus address icon and the suggested plus address
  raw_ptr<views::View> icon_and_plus_address_container_ = nullptr;
  raw_ptr<views::ImageView> icon_ = nullptr;
  // A label with mono-spaced font for the suggested plus address.
  raw_ptr<views::Label> plus_address_ = nullptr;
  // A label with red font for error messages.
  raw_ptr<views::Label> error_message_ = nullptr;

  // A container for the plus address generation message.
  raw_ptr<views::Label> generation_message_ = nullptr;

  raw_ptr<views::ImageButton> refresh_button_ = nullptr;
};

PlusAddressCreationDialogDelegate::PlusAddressContainerView::
    PlusAddressContainerView(
        std::unique_ptr<views::ImageButton> refresh_button) {
  // The first column contains two elements, only one of which is visible:
  // - The plus address with the icon to the left.
  // - The generation message.
  auto first_column = std::make_unique<views::BoxLayoutView>();
  first_column->AddChildView(CreatePlusAddressLabelWithIcon());
  // The left margin of the generation label should be the same as that of the
  // plus address icon.
  constexpr auto kMargins = gfx::Insets::TLBR(
      0, (kPlusAddressIconColumnWidth - kPlusAddressIconWidth) / 2, 0, 0);
  first_column->AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_PLUS_ADDRESS_MODAL_GENERATION_TEMPORARY_LABEL_CONTENT))
          .SetTextContext(views::style::CONTEXT_LABEL)
          .SetTextStyle(views::style::STYLE_SECONDARY_MONOSPACED)
          .CopyAddressTo(&generation_message_)
          .SetProperty(views::kMarginsKey, kMargins)
          .SetProperty(
              views::kElementIdentifierKey,
              PlusAddressCreationView::kPlusAddressGenerationMessageElementId)
          .Build());
  generation_message_->SetLineHeight(2 * generation_message_->GetLineHeight());

  SetBackground(views::CreateThemedRoundedRectBackground(
      // TODO(b/342330801): Figure out the correct color for the
      // background and move the definition to the mixer.
      ui::kColorSysHeaderContainer,
      kProposedPlusAddressBackgroundCornerRadius));
  SetProperty(views::kMarginsKey,
              gfx::Insets::VH(kPlusAddressLabelVerticalMargin, 0));
  AddColumn(Alignment::kStart, Alignment::kCenter, 1.0f,
            ColumnSize::kUsePreferred, 0, 0);
  AddColumn(Alignment::kStart, Alignment::kStretch,
            views::TableLayout::kFixedSize, ColumnSize::kFixed,
            kPlusAddressRefreshColumnWidth, 0);
  AddRows(1, views::TableLayout::kFixedSize, /*height=*/0);
  AddChildView(std::move(first_column));
  refresh_button_ = AddChildView(std::move(refresh_button));
}

std::unique_ptr<views::View> PlusAddressCreationDialogDelegate::
    PlusAddressContainerView::CreatePlusAddressLabelWithIcon() {
  auto address_label =
      views::Builder<views::Label>()
          .SetTextContext(views::style::CONTEXT_LABEL)
          .SetTextStyle(views::style::STYLE_SECONDARY_MONOSPACED)
          .SetProperty(
              views::kElementIdentifierKey,
              PlusAddressCreationView::kPlusAddressSuggestedEmailElementId)
          .SetSelectable(true)
          .CopyAddressTo(&plus_address_)
          .Build();
  address_label->SetLineHeight(2 * address_label->GetLineHeight());

  auto error_label =
      views::Builder<views::Label>()
          .SetTextContext(views::style::CONTEXT_LABEL)
          .SetProperty(views::kElementIdentifierKey,
                       PlusAddressCreationView::kPlusAddressReserveErrorId)
          .SetEnabledColorId(ui::kColorSysError)
          .SetSelectable(true)
          .CopyAddressTo(&error_message_)
          .Build();
  error_label->SetLineHeight(2 * error_label->GetLineHeight());

  views::BoxLayoutView* label_container = nullptr;
  auto label_with_icon =
      views::Builder<views::TableLayoutView>()
          .AddColumn(Alignment::kCenter, Alignment::kCenter,
                     views::TableLayout::kFixedSize, ColumnSize::kFixed,
                     kPlusAddressIconColumnWidth, 0)
          .AddColumn(Alignment::kStart, Alignment::kCenter, 1.0f,
                     ColumnSize::kUsePreferred, 0, 0)
          .AddRows(1, views::TableLayout::kFixedSize, /*height=*/0)
          .AddChild(views::Builder<views::ImageView>().CopyAddressTo(&icon_))
          .AddChild(views::Builder<views::BoxLayoutView>().CopyAddressTo(
              &label_container))
          .CopyAddressTo(&icon_and_plus_address_container_)
          .SetVisible(false)
          .Build();
  label_container->AddChildView(std::move(address_label));
  label_container->AddChildView(std::move(error_label));
  return label_with_icon;
}

void PlusAddressCreationDialogDelegate::PlusAddressContainerView::
    ShowGenerationMessage() {
  generation_message_->SetVisible(true);
  icon_and_plus_address_container_->SetVisible(false);
}

void PlusAddressCreationDialogDelegate::PlusAddressContainerView::
    ShowPlusAddress(const std::u16string& address) {
  generation_message_->SetVisible(false);
  ShowIcon(Icon::kPlusAddress);
  icon_and_plus_address_container_->SetVisible(true);
  error_message_->SetVisible(false);
  plus_address_->SetVisible(true);
  plus_address_->SetText(address);
}

void PlusAddressCreationDialogDelegate::PlusAddressContainerView::ShowError(
    const std::u16string& error) {
  generation_message_->SetVisible(false);
  ShowIcon(Icon::kError);
  icon_and_plus_address_container_->SetVisible(true);
  plus_address_->SetVisible(false);
  error_message_->SetVisible(true);
  error_message_->SetText(error);
}

void PlusAddressCreationDialogDelegate::PlusAddressContainerView::ShowIcon(
    Icon icon) {
  switch (icon) {
    case Icon::kPlusAddress:
      icon_->SetImage(ui::ImageModel::FromVectorIcon(
          kLogoLargeIcon, ui::kColorIcon, kPlusAddressIconWidth));
      return;
    case Icon::kError:
      icon_->SetImage(ui::ImageModel::FromVectorIcon(
          vector_icons::kErrorIcon, ui::kColorSysError, kPlusAddressIconWidth));
      return;
  }
  NOTREACHED();
}

void PlusAddressCreationDialogDelegate::PlusAddressContainerView::ShowRefresh(
    bool visible) {
  refresh_button_->SetVisible(visible);
}

void PlusAddressCreationDialogDelegate::PlusAddressContainerView::
    SetEnabledForRefreshButton(bool enabled) {
  refresh_button_->SetEnabled(enabled);
}

BEGIN_METADATA(PlusAddressCreationDialogDelegate, PlusAddressContainerView)
END_METADATA

PlusAddressCreationDialogDelegate::PlusAddressCreationDialogDelegate(
    base::WeakPtr<PlusAddressCreationController> controller,
    content::WebContents* web_contents,
    const std::string& primary_email_address,
    bool show_notice)
    : views::BubbleDialogDelegate(/*anchor_view=*/nullptr,
                                  views::BubbleBorder::Arrow::NONE,
                                  views::BubbleBorder::DIALOG_SHADOW,
                                  /*autosize=*/true),
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

  // This view will act as the contents view for the bubble. It is used to
  // be able to set empty margins on `this` and, instead, set them on
  // `primary_view`. This allows adding an element at the top of the
  // dialog that has no margins.
  std::unique_ptr<views::View> wrapper_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .Build();
  progress_bar_ = wrapper_view->AddChildView(CreateProgressBar());
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
  // message) and the refresh button.
  plus_address_container_ = primary_view->AddChildView(
      std::make_unique<PlusAddressContainerView>(CreateRefreshButton()));

  // The error report label is hidden by default.
  create_error_message_label_ =
      primary_view->AddChildView(CreateErrorMessageLabel());
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

PlusAddressCreationDialogDelegate::~PlusAddressCreationDialogDelegate() =
    default;

void PlusAddressCreationDialogDelegate::OnWidgetInitialized() {
  if (views::BubbleFrameView* frame = GetBubbleFrameView(); frame) {
    frame->close_button()->SetCallback(
        views::Button::PressedCallback(base::BindRepeating(
            &PlusAddressCreationDialogDelegate::HandleButtonPress,
            // Safe because this outlives the BubbleFrameView.
            base::Unretained(this), PlusAddressViewButtonType::kClose)));
  }
}

void PlusAddressCreationDialogDelegate::ShowReserveResult(
    const PlusProfileOrError& maybe_plus_profile,
    bool offer_refresh) {

  SetProgressBarVisibility(false);
  plus_address_container_->ShowRefresh(offer_refresh);
  plus_address_container_->SetEnabledForRefreshButton(true);
  if (maybe_plus_profile.has_value()) {
    plus_address_container_->ShowPlusAddress(
        base::UTF8ToUTF16(*maybe_plus_profile->plus_address));
    confirm_button_->SetEnabled(true);
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kPlusAddressUpdatedErrorStatesInOnboardingModal)) {
    // Show error in plus address container.
    plus_address_container_->ShowError(
        l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_RESERVE_ERROR));
  } else {
    ShowErrorStateUI();
  }
}

void PlusAddressCreationDialogDelegate::ShowConfirmResult(
    const PlusProfileOrError& maybe_plus_profile) {
  CHECK(GetWidget());
  CHECK(GetBubbleFrameView());

  // Stop indicating loading now that we have the server response.
  SetProgressBarVisibility(false);

  if (maybe_plus_profile.has_value()) {
    GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kAcceptButtonClicked);
    return;
  }
  if (base::FeatureList::IsEnabled(
          features::kPlusAddressUpdatedErrorStatesInOnboardingModal)) {
    ShowCreateErrorMessage(maybe_plus_profile.error().IsTimeoutError());
    confirm_button_->SetText(
        l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_CREATE_ERROR_BUTTON));
    confirm_button_->SetEnabled(true);
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
      confirm_button_->SetEnabled(false);
      HideCreateErrorMessage();
      SetProgressBarVisibility(true);
      controller_->OnConfirmed();
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
  button->SetVisible(false);
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

void PlusAddressCreationDialogDelegate::SetProgressBarVisibility(
    bool is_visible) {
  progress_bar_->SetVisible(is_visible);
}

void PlusAddressCreationDialogDelegate::ShowErrorStateUI() {
  CHECK(GetWidget() && web_contents_);
  plus_address_container_->SetVisible(false);
  // Show the error report instructions.
  error_report_label_->SetVisible(true);
  // Update the size of modal.
  constrained_window::UpdateWebContentsModalDialogPosition(
      GetWidget(),
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_)
          ->delegate()
          ->GetWebContentsModalDialogHost());
}

void PlusAddressCreationDialogDelegate::ShowCreateErrorMessage(
    bool is_timeout) {
  plus_address_container_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kPlusAddressLabelVerticalMargin, 0, 0, 0));
  plus_address_container_->ShowIcon(PlusAddressContainerView::Icon::kError);
  create_error_message_label_->SetText(l10n_util::GetStringUTF16(
      is_timeout ? IDS_PLUS_ADDRESS_MODAL_CREATE_TIMEOUT_ERROR
                 : IDS_PLUS_ADDRESS_MODAL_CREATE_ERROR));
  create_error_message_label_->SetVisible(true);
}

void PlusAddressCreationDialogDelegate::HideCreateErrorMessage() {
  plus_address_container_->SetProperty(
      views::kMarginsKey, gfx::Insets::VH(kPlusAddressLabelVerticalMargin, 0));
  plus_address_container_->ShowIcon(
      PlusAddressContainerView::Icon::kPlusAddress);
  create_error_message_label_->SetVisible(false);
}

void PlusAddressCreationDialogDelegate::OnRefreshClicked() {
  plus_address_container_->ShowGenerationMessage();
  plus_address_container_->SetEnabledForRefreshButton(false);
  confirm_button_->SetEnabled(false);
  SetProgressBarVisibility(true);
  HideCreateErrorMessage();
  controller_->OnRefreshClicked();
}

}  // namespace plus_addresses
