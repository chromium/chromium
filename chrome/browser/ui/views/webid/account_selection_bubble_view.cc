// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/ui/monogram_utils.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"
#include "chrome/grit/generated_resources.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/common/content_features.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

// Error codes.
constexpr char kInvalidRequest[] = "invalid_request";
constexpr char kUnauthorizedClient[] = "unauthorized_client";
constexpr char kAccessDenied[] = "access_denied";
constexpr char kTemporarilyUnavailable[] = "temporarily_unavailable";
constexpr char kServerError[] = "server_error";

// views::MdTextButton which:
// - Uses the passed-in `brand_background_color` based on whether the button
//   background contrasts sufficiently with dialog background.
// - If `brand_text_color` is not provided, computes the text color such that it
//   contrasts sufficiently with `brand_background_color`.
class ContinueButton : public views::MdTextButton {
  METADATA_HEADER(ContinueButton, views::MdTextButton)

 public:
  ContinueButton(views::MdTextButton::PressedCallback callback,
                 const std::u16string& text,
                 AccountSelectionBubbleView* bubble_view,
                 const content::IdentityProviderMetadata& idp_metadata)
      : views::MdTextButton(std::move(callback), text),
        bubble_view_(bubble_view),
        brand_background_color_(idp_metadata.brand_background_color),
        brand_text_color_(idp_metadata.brand_text_color) {
    SetCornerRadius(kButtonRadius);
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
    SetStyle(ui::ButtonStyle::kProminent);
  }

  ContinueButton(const ContinueButton&) = delete;
  ContinueButton& operator=(const ContinueButton&) = delete;
  ~ContinueButton() override = default;

  void OnThemeChanged() override {
    views::MdTextButton::OnThemeChanged();
    if (!brand_background_color_)
      return;

    const SkColor dialog_background_color = bubble_view_->GetBackgroundColor();
    if (color_utils::GetContrastRatio(dialog_background_color,
                                      *brand_background_color_) <
        color_utils::kMinimumVisibleContrastRatio) {
      SetBgColorOverride(std::nullopt);
      SetEnabledTextColors(std::nullopt);
      return;
    }

    SetBgColorOverride(*brand_background_color_);
    SkColor text_color;
    if (brand_text_color_) {
      // IdpNetworkRequestManager ensures that `brand_text_color_` is only set
      // if it sufficiently contrasts with `brand_background_color_`.
      text_color = *brand_text_color_;
    } else {
      text_color = color_utils::BlendForMinContrast(GetCurrentTextColor(),
                                                    *brand_background_color_)
                       .color;
    }
    SetEnabledTextColors(text_color);
  }

 private:
  raw_ptr<AccountSelectionBubbleView> bubble_view_;
  std::optional<SkColor> brand_background_color_;
  std::optional<SkColor> brand_text_color_;
};

BEGIN_METADATA(ContinueButton)
END_METADATA

std::pair<std::u16string, std::u16string> GetErrorDialogText(
    const std::optional<TokenError>& error,
    const std::u16string& top_frame_for_display,
    const std::u16string& idp_for_display) {
  std::string code = error ? error->code : "";
  GURL url = error ? error->url : GURL();

  std::u16string summary;
  std::u16string description;

  if (code == kInvalidRequest) {
    summary = l10n_util::GetStringFUTF16(
        IDS_SIGNIN_INVALID_REQUEST_ERROR_DIALOG_SUMMARY, top_frame_for_display,
        idp_for_display);
    description = l10n_util::GetStringUTF16(
        IDS_SIGNIN_INVALID_REQUEST_ERROR_DIALOG_DESCRIPTION);
  } else if (code == kUnauthorizedClient) {
    summary = l10n_util::GetStringFUTF16(
        IDS_SIGNIN_UNAUTHORIZED_CLIENT_ERROR_DIALOG_SUMMARY,
        top_frame_for_display, idp_for_display);
    description = l10n_util::GetStringUTF16(
        IDS_SIGNIN_UNAUTHORIZED_CLIENT_ERROR_DIALOG_DESCRIPTION);
  } else if (code == kAccessDenied) {
    summary = l10n_util::GetStringUTF16(
        IDS_SIGNIN_ACCESS_DENIED_ERROR_DIALOG_SUMMARY);
    description = l10n_util::GetStringUTF16(
        IDS_SIGNIN_ACCESS_DENIED_ERROR_DIALOG_DESCRIPTION);
  } else if (code == kTemporarilyUnavailable) {
    summary = l10n_util::GetStringUTF16(
        IDS_SIGNIN_TEMPORARILY_UNAVAILABLE_ERROR_DIALOG_SUMMARY);
    description = l10n_util::GetStringFUTF16(
        IDS_SIGNIN_TEMPORARILY_UNAVAILABLE_ERROR_DIALOG_DESCRIPTION,
        idp_for_display);
  } else if (code == kServerError) {
    summary = l10n_util::GetStringUTF16(IDS_SIGNIN_SERVER_ERROR_DIALOG_SUMMARY);
    description = l10n_util::GetStringFUTF16(
        IDS_SIGNIN_SERVER_ERROR_DIALOG_DESCRIPTION, top_frame_for_display);
    // Extra description is not needed for kServerError.
    return {summary, description};
  } else {
    summary = l10n_util::GetStringFUTF16(
        IDS_SIGNIN_GENERIC_ERROR_DIALOG_SUMMARY, idp_for_display);
    description =
        l10n_util::GetStringUTF16(IDS_SIGNIN_GENERIC_ERROR_DIALOG_DESCRIPTION);
    // Extra description is not needed for the generic error dialog.
    return {summary, description};
  }

  if (url.is_empty()) {
    description +=
        u" " + l10n_util::GetStringFUTF16(
                   code == kTemporarilyUnavailable
                       ? IDS_SIGNIN_ERROR_DIALOG_TRY_OTHER_WAYS_RETRY_PROMPT
                       : IDS_SIGNIN_ERROR_DIALOG_TRY_OTHER_WAYS_PROMPT,
                   top_frame_for_display);
    return {summary, description};
  }

  description +=
      u" " + l10n_util::GetStringFUTF16(
                 code == kTemporarilyUnavailable
                     ? IDS_SIGNIN_ERROR_DIALOG_MORE_DETAILS_RETRY_PROMPT
                     : IDS_SIGNIN_ERROR_DIALOG_MORE_DETAILS_PROMPT,
                 idp_for_display);
  return {summary, description};
}

std::u16string BuildStringFromIDPs(
    const std::vector<std::u16string> mismatch_idps,
    const std::vector<std::u16string> non_mismatch_idps) {
  constexpr int kMaxIdpsToShow = 3;
  size_t num_idps = 0;
  std::u16string result;
  auto AddToResult = [&](const auto& idp_vector) {
    if (num_idps == kMaxIdpsToShow) {
      return;
    }
    for (const auto& idp : idp_vector) {
      if (num_idps > 0) {
        result += u", ";
      }
      result += idp;
      if (++num_idps == kMaxIdpsToShow) {
        break;
      }
    }
  };
  AddToResult(mismatch_idps);
  AddToResult(non_mismatch_idps);
  return result;
}

}  // namespace

AccountSelectionBubbleView::AccountSelectionBubbleView(
    const std::u16string& top_frame_for_display,
    const std::optional<std::u16string>& iframe_for_display,
    const std::optional<std::u16string>& idp_title,
    blink::mojom::RpContext rp_context,
    content::WebContents* web_contents,
    views::View* anchor_view,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    AccountSelectionViewBase::Observer* observer,
    views::WidgetObserver* widget_observer)
    : views::BubbleDialogDelegateView(
          anchor_view,
          // Note that BOTTOM_RIGHT means the bubble's bottom and right are
          // anchored to the `anchor_view`, which effectively means the bubble
          // will be on top of the `anchor_view`, aligned on its right side.
          views::BubbleBorder::Arrow::BOTTOM_RIGHT),
      AccountSelectionViewBase(web_contents,
                               observer,
                               widget_observer,
                               std::move(url_loader_factory)) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_fixed_width(kBubbleWidth);
  set_margins(gfx::Insets::VH(kTopBottomPadding + kVerticalSpacing, 0));
  // TODO(crbug.com/40224637): we are currently using a custom header because
  // the icon, title, and close buttons from a bubble are not customizable
  // enough to satisfy the UI requirements. However, this adds complexity to the
  // code and makes this bubble lose any improvements made to the base bubble,
  // so we should revisit this.
  SetShowTitle(false);
  SetShowCloseButton(false);
  set_close_on_deactivate(false);

  // If `idp_title` is std::nullopt, we are going to show multi-IDP UI. DCHECK
  // that we do not get to this when the flag is disabled.
  DCHECK(
      idp_title.has_value() ||
      base::FeatureList::IsEnabled(features::kFedCmMultipleIdentityProviders));

  rp_context_ = rp_context;
  title_ = GetTitle(top_frame_for_display, iframe_for_display, idp_title,
                    rp_context);
  accessible_title_ = GetAccessibleTitle(
      top_frame_for_display, iframe_for_display, idp_title, rp_context);
  SetAccessibleTitle(accessible_title_);

  if (iframe_for_display.has_value()) {
    subtitle_ = AccountSelectionViewBase::GetSubtitle(top_frame_for_display);
  }

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kTopBottomPadding));
  header_view_ =
      AddChildView(CreateHeaderView(/*has_idp_icon=*/idp_title.has_value()));
}

AccountSelectionBubbleView::~AccountSelectionBubbleView() = default;

void AccountSelectionBubbleView::InitDialogWidget() {
  if (!web_contents_) {
    return;
  }

  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);

  if (!widget) {
    return;
  }

  // Add the widget observer, if available. It is null in tests.
  if (widget_observer_) {
    widget->AddObserver(widget_observer_);
  }

  dialog_widget_ = widget->GetWeakPtr();
}

void AccountSelectionBubbleView::ShowMultiAccountPicker(
    const std::vector<IdentityProviderDisplayData>& idp_display_data_list,
    bool show_back_button) {
  // If there are multiple IDPs, then the content::IdentityProviderMetadata
  // passed will be unused since there will be no `header_icon_view_`.
  // Therefore, it is fine to pass the first one into UpdateHeader().
  DCHECK(idp_display_data_list.size() == 1u || !header_icon_view_);
  UpdateHeader(idp_display_data_list[0].idp_metadata, title_, subtitle_,
               show_back_button);

  RemoveNonHeaderChildViews();
  AddChildView(std::make_unique<views::Separator>());
  AddChildView(CreateMultipleAccountChooser(idp_display_data_list));

  if (!has_sheet_) {
    has_sheet_ = true;
    InitDialogWidget();
    return;
  }

  SizeToContents();
  PreferredSizeChanged();
}

void AccountSelectionBubbleView::ShowVerifyingSheet(
    const content::IdentityRequestAccount& account,
    const IdentityProviderDisplayData& idp_display_data,
    const std::u16string& title) {
  UpdateHeader(idp_display_data.idp_metadata, title,
               /*subpage_subtitle=*/u"", /*show_back_button=*/false);

  RemoveNonHeaderChildViews();
  views::ProgressBar* const progress_bar =
      AddChildView(std::make_unique<views::ProgressBar>());
  progress_bar->SetPreferredHeight(kProgressBarHeight);
  // Use an infinite animation: SetValue(-1).
  progress_bar->SetValue(-1);
  progress_bar->SetBackgroundColor(SK_ColorLTGRAY);
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kTopBottomPadding, kLeftRightPadding)));
  row->AddChildView(CreateAccountRow(account, idp_display_data,
                                     /*should_hover=*/false,
                                     /*should_include_idp=*/false));
  AddChildView(std::move(row));

  if (!has_sheet_) {
    has_sheet_ = true;
    InitDialogWidget();
    SendAccessibilityEvent(GetWidget(), title);
    return;
  }

  SizeToContents();
  PreferredSizeChanged();

  SendAccessibilityEvent(GetWidget(), title);
}

void AccountSelectionBubbleView::ShowSingleAccountConfirmDialog(
    const std::u16string& top_frame_for_display,
    const std::optional<std::u16string>& iframe_for_display,
    const content::IdentityRequestAccount& account,
    const IdentityProviderDisplayData& idp_display_data,
    bool show_back_button) {
  std::u16string title =
      GetTitle(top_frame_for_display, iframe_for_display,
               idp_display_data.idp_etld_plus_one, rp_context_);
  UpdateHeader(idp_display_data.idp_metadata, title, subtitle_,
               show_back_button);

  RemoveNonHeaderChildViews();
  AddChildView(std::make_unique<views::Separator>());
  AddChildView(CreateSingleAccountChooser(idp_display_data, account));

  if (!has_sheet_) {
    has_sheet_ = true;
    InitDialogWidget();
    return;
  }

  SizeToContents();
  PreferredSizeChanged();
}

void AccountSelectionBubbleView::ShowFailureDialog(
    const std::u16string& top_frame_for_display,
    const std::optional<std::u16string>& iframe_for_display,
    const std::u16string& idp_for_display,
    const content::IdentityProviderMetadata& idp_metadata) {
  std::u16string title = GetTitle(top_frame_for_display, iframe_for_display,
                                  idp_for_display, rp_context_);
  UpdateHeader(idp_metadata, title, subtitle_,
               /*show_back_button=*/false);

  RemoveNonHeaderChildViews();
  AddChildView(std::make_unique<views::Separator>());
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(0, kLeftRightPadding)));

  // Add body.
  views::Label* const body = row->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(IDS_IDP_SIGNIN_STATUS_MISMATCH_DIALOG_BODY,
                                 idp_for_display),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY));
  body->SetMultiLine(true);
  body->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  constexpr int kBodyLineHeight = 20;
  body->SetLineHeight(kBodyLineHeight);

  // Add space between the body and the separator and the body and the continue
  // button.
  constexpr int kBottomSpacing = 16;
  body->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kVerticalSpacing, 0, kBottomSpacing, 0)));

  // Add continue button.
  auto button = std::make_unique<ContinueButton>(
      base::BindRepeating(&AccountSelectionViewBase::Observer::OnLoginToIdP,
                          base::Unretained(observer_), idp_metadata.config_url,
                          idp_metadata.idp_login_url),
      l10n_util::GetStringUTF16(IDS_SIGNIN_CONTINUE), this, idp_metadata);
  row->AddChildView(std::move(button));
  AddChildView(std::move(row));

  if (!has_sheet_) {
    has_sheet_ = true;
    InitDialogWidget();
    return;
  }

  SizeToContents();
  PreferredSizeChanged();
}

void AccountSelectionBubbleView::ShowErrorDialog(
    const std::u16string& top_frame_for_display,
    const std::optional<std::u16string>& iframe_for_display,
    const std::u16string& idp_for_display,
    const content::IdentityProviderMetadata& idp_metadata,
    const std::optional<TokenError>& error) {
  std::u16string title = GetTitle(top_frame_for_display, iframe_for_display,
                                  idp_for_display, rp_context_);
  UpdateHeader(idp_metadata, title, subtitle_,
               /*show_back_button=*/false);

  RemoveNonHeaderChildViews();
  AddChildView(std::make_unique<views::Separator>());
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kTopBottomPadding, kLeftRightPadding)));

  std::u16string summary_text;
  std::u16string description_text;
  std::tie(summary_text, description_text) =
      GetErrorDialogText(error, top_frame_for_display, idp_for_display);

  // Add error summary.
  views::Label* const summary =
      row->AddChildView(std::make_unique<views::Label>(
          summary_text, views::style::CONTEXT_DIALOG_TITLE,
          views::style::STYLE_PRIMARY));
  summary->SetMultiLine(true);
  summary->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  constexpr int kSummaryLineHeight = 22;
  summary->SetLineHeight(kSummaryLineHeight);

  // Add error description.
  views::Label* const description =
      row->AddChildView(std::make_unique<views::Label>(
          description_text, views::style::CONTEXT_DIALOG_BODY_TEXT,
          views::style::STYLE_SECONDARY));
  description->SetMultiLine(true);
  description->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  constexpr int kDescriptionLineHeight = 20;
  description->SetLineHeight(kDescriptionLineHeight);

  AddChildView(std::move(row));

  // Add row for buttons.
  auto button_row = std::make_unique<views::BoxLayoutView>();
  button_row->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd);
  constexpr int kButtonRowTopPadding = 6;
  button_row->SetInsideBorderInsets(
      gfx::Insets::TLBR(kButtonRowTopPadding, 0, 0, kLeftRightPadding));
  constexpr int kButtonRowChildSpacing = 8;
  button_row->SetBetweenChildSpacing(kButtonRowChildSpacing);

  // Add more details button.
  if (error && !error->url.is_empty()) {
    auto more_details_button = std::make_unique<views::MdTextButton>(
        base::BindRepeating(&AccountSelectionViewBase::Observer::OnMoreDetails,
                            base::Unretained(observer_)),
        l10n_util::GetStringUTF16(IDS_SIGNIN_ERROR_DIALOG_MORE_DETAILS_BUTTON));
    button_row->AddChildView(std::move(more_details_button));
  }

  // Add got it button.
  auto got_it_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&AccountSelectionViewBase::Observer::OnGotIt,
                          base::Unretained(observer_)),
      l10n_util::GetStringUTF16(IDS_SIGNIN_ERROR_DIALOG_GOT_IT_BUTTON));
  button_row->AddChildView(std::move(got_it_button));

  AddChildView(std::move(button_row));

  if (!has_sheet_) {
    has_sheet_ = true;
    InitDialogWidget();
    return;
  }

  SizeToContents();
  PreferredSizeChanged();
}

void AccountSelectionBubbleView::ShowLoadingDialog() {
  NOTREACHED()
      << "ShowLoadingDialog is only implemented for AccountSelectionModalView";
}

void AccountSelectionBubbleView::ShowRequestPermissionDialog(
    const std::u16string& top_frame_for_display,
    const content::IdentityRequestAccount& account,
    const IdentityProviderDisplayData& idp_display_data) {
  NOTREACHED() << "ShowRequestPermissionDialog is only implemented for "
                  "AccountSelectionModalView";
}

void AccountSelectionBubbleView::ShowSingleReturningAccountDialog(
    const std::vector<IdentityProviderDisplayData>& idp_data_list) {
  // We currently only invoke this method in the multi IDP case.
  DCHECK(idp_data_list.size() > 1u);
  // Since there are multiple IDPs, then the content::IdentityProviderMetadata
  // passed will be unused since there will be no `header_icon_view_`.
  UpdateHeader(content::IdentityProviderMetadata(), title_, subtitle_,
               /*show_back_button=*/false);

  RemoveNonHeaderChildViews();
  AddChildView(std::make_unique<views::Separator>());
  AddChildView(CreateSingleReturningAccountChooser(idp_data_list));

  if (!has_sheet_) {
    has_sheet_ = true;
    InitDialogWidget();
    return;
  }

  SizeToContents();
  PreferredSizeChanged();
}

void AccountSelectionBubbleView::CloseDialog() {
  if (!dialog_widget_) {
    return;
  }

  CancelDialog();
  // Remove the widget observer, if available. It is null in tests.
  if (widget_observer_) {
    dialog_widget_->RemoveObserver(widget_observer_);
  }
  dialog_widget_.reset();
}

std::string AccountSelectionBubbleView::GetDialogTitle() const {
  // We cannot just return title_ because it is not always set
  // (e.g. by ShowFailureDialog).
  return base::UTF16ToUTF8(title_label_->GetText());
}

std::optional<std::string> AccountSelectionBubbleView::GetDialogSubtitle()
    const {
  if (!subtitle_label_) {
    return std::nullopt;
  }

  return base::UTF16ToUTF8(subtitle_label_->GetText());
}

gfx::Rect AccountSelectionBubbleView::GetBubbleBounds() {
  // The bubble initially looks like this relative to the contents_web_view:
  //                        |--------|
  //                        |        |
  //                        | bubble |
  //                        |        |
  //       |-------------------------|
  //       |                         |
  //       | contents_web_view       |
  //       |          ...            |
  //       |-------------------------|
  // Thus, we need to move the bubble to the left by kRightMargin and down by
  // the size of the bubble plus kTopMargin in order to achieve what we want:
  //       |-------------------------|
  //       |               kTopMargin|
  //       |         |--------|      |
  //       |         |        |kRight|
  //       |         | bubble |Margin|
  //       |         |--------|      |
  //       |                         |
  //       | contents_web_view       |
  //       |          ...            |
  //       |-------------------------|
  // In the RTL case, the bubble is aligned towards the left side of the screen
  // and hence the x-axis offset needs to be in the opposite direction.
  return views::BubbleDialogDelegateView::GetBubbleBounds() +
         gfx::Vector2d(base::i18n::IsRTL() ? kRightMargin : -kRightMargin,
                       GetWidget()->client_view()->GetPreferredSize().height() +
                           kTopMargin);
}

std::unique_ptr<views::View> AccountSelectionBubbleView::CreateHeaderView(
    bool has_idp_icon) {
  auto header = std::make_unique<views::View>();
  // Do not use a top margin as it has already been set in the bubble.
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetInteriorMargin(gfx::Insets::TLBR(
          0, kLeftRightPadding, kVerticalSpacing, kLeftRightPadding));

  // Add the space for the icon.
  if (has_idp_icon) {
    auto image_view = std::make_unique<BrandIconImageView>(
        base::BindOnce(&AccountSelectionViewBase::AddIdpImage,
                       weak_ptr_factory_.GetWeakPtr()),
        kDesiredIdpIconSize);
    image_view->SetImageSize(
        gfx::Size(kDesiredIdpIconSize, kDesiredIdpIconSize));
    image_view->SetProperty(views::kMarginsKey,
                            gfx::Insets().set_right(kLeftRightPadding));
    header_icon_view_ = header->AddChildView(std::move(image_view));
  }

  back_button_ =
      header->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(
              &AccountSelectionViewBase::Observer::OnBackButtonClicked,
              base::Unretained(observer_)),
          vector_icons::kArrowBackIcon));
  views::InstallCircleHighlightPathGenerator(back_button_.get());
  back_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  back_button_->SetVisible(false);

  int back_button_right_margin = kLeftRightPadding;
  if (header_icon_view_) {
    // Set the right margin of the back button so that the back button and
    // the IDP brand icon have the same width. This ensures that the header
    // title does not shift when the user navigates to the consent screen.
    back_button_right_margin =
        std::max(0, back_button_right_margin +
                        header_icon_view_->GetPreferredSize().width() -
                        back_button_->GetPreferredSize().width());
  }
  back_button_->SetProperty(views::kMarginsKey,
                            gfx::Insets().set_right(back_button_right_margin));

  // Add the title.
  title_label_ = header->AddChildView(std::make_unique<views::Label>(
      title_, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY));
  SetLabelProperties(title_label_);

  // Add the close button.
  std::unique_ptr<views::Button> close_button =
      views::BubbleFrameView::CreateCloseButton(base::BindRepeating(
          &AccountSelectionViewBase::Observer::OnCloseButtonClicked,
          base::Unretained(observer_)));
  close_button->SetVisible(true);
  header->AddChildView(std::move(close_button));

  if (subtitle_.empty()) {
    return header;
  }

  // Add the subtitle.
  auto header_with_subtitle = std::make_unique<views::View>();
  header_with_subtitle->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  header_with_subtitle->AddChildView(std::move(header));
  subtitle_label_ =
      header_with_subtitle->AddChildView(std::make_unique<views::Label>(
          subtitle_, views::style::CONTEXT_DIALOG_BODY_TEXT,
          views::style::STYLE_SECONDARY));
  SetLabelProperties(subtitle_label_);
  int leftPadding = 2 * kLeftRightPadding;
  if (has_idp_icon) {
    leftPadding += kDesiredIdpIconSize;
  }
  subtitle_label_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      -kTopBottomPadding, leftPadding, kTopBottomPadding, kLeftRightPadding)));

  return header_with_subtitle;
}

std::unique_ptr<views::View>
AccountSelectionBubbleView::CreateSingleAccountChooser(
    const IdentityProviderDisplayData& idp_display_data,
    const content::IdentityRequestAccount& account) {
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(0, kLeftRightPadding), kVerticalSpacing));
  row->AddChildView(CreateAccountRow(account, idp_display_data,
                                     /*should_hover=*/false,
                                     /*should_include_idp=*/false));

  // Prefer using the given name if it is provided, otherwise fallback to name.
  const std::string display_name =
      account.given_name.empty() ? account.name : account.given_name;
  const content::IdentityProviderMetadata& idp_metadata =
      idp_display_data.idp_metadata;
  // We can pass crefs to OnAccountSelected because the `observer_` owns the
  // data.
  auto button = std::make_unique<ContinueButton>(
      base::BindRepeating(
          &AccountSelectionViewBase::Observer::OnAccountSelected,
          base::Unretained(observer_), std::cref(account),
          std::cref(idp_display_data)),
      l10n_util::GetStringFUTF16(IDS_ACCOUNT_SELECTION_CONTINUE,
                                 base::UTF8ToUTF16(display_name)),
      this, idp_metadata);
  continue_button_ = row->AddChildView(std::move(button));

  // Do not add disclosure text if this is a sign in or if we were requested
  // to skip it.
  if (account.login_state == Account::LoginState::kSignIn ||
      !idp_display_data.request_permission) {
    return row;
  }

  // Add disclosure text.
  row->AddChildView(CreateDisclosureLabel(idp_display_data));
  return row;
}

std::unique_ptr<views::View>
AccountSelectionBubbleView::CreateMultipleAccountChooser(
    const std::vector<IdentityProviderDisplayData>& idp_display_data_list) {
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  views::View* const content =
      scroll_view->SetContents(std::make_unique<views::View>());
  content->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  bool is_multi_idp = idp_display_data_list.size() > 1u;
  size_t num_rows = 0;
  for (const auto& idp_display_data : idp_display_data_list) {
    if (idp_display_data.has_login_status_mismatch) {
      content->AddChildView(CreateIdpLoginRow(
          idp_display_data.idp_etld_plus_one, idp_display_data.idp_metadata));
      num_rows += 1;
      continue;
    }
    for (const auto& account : idp_display_data.accounts) {
      content->AddChildView(
          CreateAccountRow(account, idp_display_data, /*should_hover=*/true,
                           /*should_include_idp=*/is_multi_idp));
    }
    const content::IdentityProviderMetadata& idp_metadata =
        idp_display_data.idp_metadata;
    if (idp_metadata.supports_add_account) {
      content->AddChildView(std::make_unique<views::Separator>());
      content->AddChildView(CreateUseOtherAccountButton(idp_metadata));
    }
    num_rows += idp_display_data.accounts.size();
  }

  // The maximum height that the multi-account-picker can have. This value was
  // chosen so that if there are more than two accounts, the picker will show up
  // as a scrollbar showing 2 accounts plus half of the third one. Note that
  // this is an estimate if there are multiple IDPs, as IDP rows are not the
  // same height. That said, calling GetPreferredSize() is expensive so we are
  // ok with this estimate. And in this case, we prefer to use 3.5 as there will
  // be at least one IDP row at the beginning.
  float num_visible_rows = is_multi_idp ? 3.5f : 2.5f;
  const int per_account_size = content->GetPreferredSize().height() / num_rows;
  scroll_view->ClipHeightTo(
      0, static_cast<int>(per_account_size * num_visible_rows));
  return scroll_view;
}

std::unique_ptr<views::View>
AccountSelectionBubbleView::CreateSingleReturningAccountChooser(
    const std::vector<IdentityProviderDisplayData>& idp_display_data_list) {
  std::vector<std::u16string> mismatch_idps;
  std::vector<std::u16string> non_mismatch_idps;
  const content::IdentityRequestAccount* returning_account = nullptr;
  const IdentityProviderDisplayData* returning_idp = nullptr;
  for (const auto& idp : idp_display_data_list) {
    if (idp.has_login_status_mismatch) {
      mismatch_idps.push_back(idp.idp_etld_plus_one);
    } else {
      non_mismatch_idps.push_back(idp.idp_etld_plus_one);
    }
    if (returning_account) {
      continue;
    }
    for (const auto& acc : idp.accounts) {
      if (acc.login_state == Account::LoginState::kSignIn) {
        returning_account = &acc;
        returning_idp = &idp;
        break;
      }
    }
  }
  CHECK(returning_account && returning_idp);
  auto content = std::make_unique<views::View>();
  content->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  content->AddChildView(CreateAccountRow(*returning_account, *returning_idp,
                                         /*should_hover=*/true,
                                         /*should_include_idp=*/true));
  content->AddChildView(std::make_unique<views::Separator>());
  content->AddChildView(
      CreateChooseAnAccountButton(mismatch_idps, non_mismatch_idps));
  return content;
}

std::unique_ptr<views::View> AccountSelectionBubbleView::CreateIdpLoginRow(
    const std::u16string& idp_for_display,
    const content::IdentityProviderMetadata& idp_metadata) {
  auto image_view = std::make_unique<BrandIconImageView>(
      base::BindOnce(&AccountSelectionViewBase::AddIdpImage,
                     weak_ptr_factory_.GetWeakPtr()),
      kDesiredIdpIconSize);
  image_view->SetImageSize(gfx::Size(kDesiredIdpIconSize, kDesiredIdpIconSize));
  image_view->SetProperty(views::kMarginsKey,
                          gfx::Insets().set_right(kLeftRightPadding));
  ConfigureBrandImageView(image_view.get(), idp_metadata.brand_icon_url);

  auto button = std::make_unique<HoverButton>(
      base::BindRepeating(&AccountSelectionViewBase::Observer::OnLoginToIdP,
                          base::Unretained(observer_), idp_metadata.config_url,
                          idp_metadata.idp_login_url),
      std::move(image_view),
      l10n_util::GetStringFUTF16(IDS_IDP_SIGNIN_STATUS_MISMATCH_BUTTON_TEXT,
                                 idp_for_display));
  button->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
      /*vertical=*/kVerticalSpacing, /*horizontal=*/kLeftRightPadding)));
  return button;
}

std::unique_ptr<views::View>
AccountSelectionBubbleView::CreateUseOtherAccountButton(
    const content::IdentityProviderMetadata& idp_metadata) {
  auto button = std::make_unique<HoverButton>(
      base::BindRepeating(&AccountSelectionViewBase::Observer::OnLoginToIdP,
                          base::Unretained(observer_), idp_metadata.config_url,
                          idp_metadata.idp_login_url),
      ui::ImageModel::FromVectorIcon(kOpenInNewIcon, ui::kColorMenuIcon,
                                     kDesiredUseOtherAccountIconSize),
      l10n_util::GetStringUTF16(IDS_ACCOUNT_SELECTION_USE_OTHER_ACCOUNT));
  button->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      /*top=*/2 * kVerticalSpacing, /*left=*/kLeftRightPadding, /*bottom=*/0,
      /*right=*/kLeftRightPadding)));
  return button;
}

void AccountSelectionBubbleView::UpdateHeader(
    const content::IdentityProviderMetadata& idp_metadata,
    const std::u16string subpage_title,
    const std::u16string subpage_subtitle,
    bool show_back_button) {
  back_button_->SetVisible(show_back_button);
  if (header_icon_view_) {
    if (show_back_button)
      header_icon_view_->SetVisible(false);
    else
      ConfigureBrandImageView(header_icon_view_, idp_metadata.brand_icon_url);
  }
  title_label_->SetText(subpage_title);

  if (subtitle_label_) {
    if (subpage_subtitle.empty()) {
      delete subtitle_label_;
      subtitle_label_ = nullptr;
      return;
    }
    subtitle_label_->SetText(subpage_subtitle);
  }
}

void AccountSelectionBubbleView::RemoveNonHeaderChildViews() {
  // Make sure not to keep dangling pointers around first.
  continue_button_ = nullptr;
  auto_reauthn_checkbox_ = nullptr;

  const std::vector<raw_ptr<views::View, VectorExperimental>> child_views =
      children();
  for (views::View* child_view : child_views) {
    if (child_view != header_view_) {
      RemoveChildView(child_view);
      delete child_view;
    }
  }
}

std::unique_ptr<views::View>
AccountSelectionBubbleView::CreateChooseAnAccountButton(
    const std::vector<std::u16string> mismatch_idps,
    const std::vector<std::u16string> non_mismatch_idps) {
  auto button = std::make_unique<HoverButton>(
      base::BindOnce(&AccountSelectionViewBase::Observer::OnChooseAnAccount,
                     base::Unretained(observer_)),
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          kPersonFilledPaddedSmallIcon, ui::kColorMenuIcon,
          kDesiredChooseAnAccountIconSize)),
      l10n_util::GetStringUTF16(IDS_ACCOUNT_SELECTION_CHOOSE_AN_ACCOUNT_BUTTON),
      BuildStringFromIDPs(mismatch_idps, non_mismatch_idps));
  button->SetSubtitleTextStyle(views::style::CONTEXT_LABEL,
                               views::style::STYLE_SECONDARY);
  return button;
}

BEGIN_METADATA(AccountSelectionBubbleView)
END_METADATA
