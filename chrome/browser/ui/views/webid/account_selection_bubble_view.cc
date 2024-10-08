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
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/monogram_utils.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/webid/account_selection_view_base.h"
#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"
#include "chrome/browser/ui/views/webid/webid_utils.h"
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
#include "ui/base/l10n/time_format.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
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
// - If `extra_accessible_text` is passed, appends this to the button's
// accessible name. This is useful when the user logs in via a popup window and
// cannot easily navigate the rest of the text in the dialog to confirm which is
// the account being used to login via FedCM.
class ContinueButton : public views::MdTextButton {
  METADATA_HEADER(ContinueButton, views::MdTextButton)

 public:
  ContinueButton(views::MdTextButton::PressedCallback callback,
                 const std::u16string& text,
                 AccountSelectionBubbleView* bubble_view,
                 const content::IdentityProviderMetadata& idp_metadata,
                 std::optional<std::u16string> extra_accessible_text)
      : views::MdTextButton(std::move(callback), text),
        bubble_view_(bubble_view),
        brand_background_color_(idp_metadata.brand_background_color),
        brand_text_color_(idp_metadata.brand_text_color) {
    SetCornerRadius(kButtonRadius);
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
    SetStyle(ui::ButtonStyle::kProminent);
    if (extra_accessible_text.has_value()) {
      GetViewAccessibility().SetName(text + u", " + *extra_accessible_text);
    }
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
      SetBgColorOverrideDeprecated(std::nullopt);
      SetEnabledTextColors(std::nullopt);
      return;
    }

    SetBgColorOverrideDeprecated(*brand_background_color_);
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
    const std::u16string& rp_for_display,
    const std::u16string& idp_for_display) {
  std::string code = error ? error->code : "";
  GURL url = error ? error->url : GURL();

  std::u16string summary;
  std::u16string description;

  if (code == kInvalidRequest) {
    summary = l10n_util::GetStringFUTF16(
        IDS_SIGNIN_INVALID_REQUEST_ERROR_DIALOG_SUMMARY, rp_for_display,
        idp_for_display);
    description = l10n_util::GetStringUTF16(
        IDS_SIGNIN_INVALID_REQUEST_ERROR_DIALOG_DESCRIPTION);
  } else if (code == kUnauthorizedClient) {
    summary = l10n_util::GetStringFUTF16(
        IDS_SIGNIN_UNAUTHORIZED_CLIENT_ERROR_DIALOG_SUMMARY, rp_for_display,
        idp_for_display);
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
        IDS_SIGNIN_SERVER_ERROR_DIALOG_DESCRIPTION, rp_for_display);
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
                   rp_for_display);
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
    const std::vector<std::u16string>& mismatch_idps,
    const std::vector<std::u16string>& non_mismatch_idps) {
  constexpr int kMaxIdpsToShow = 3;
  size_t num_idps = 0;
  std::u16string result;
  auto AddToResult = [&](const std::vector<std::u16string>& idp_vector) {
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
    const std::u16string& rp_for_display,
    const std::optional<std::u16string>& idp_title,
    blink::mojom::RpContext rp_context,
    content::WebContents* web_contents,
    views::View* anchor_view,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    AccountSelectionViewBase::Observer* observer,
    views::WidgetObserver* widget_observer)
    : views::BubbleDialogDelegateView(
          anchor_view,
          // Note that TOP_RIGHT means the bubble's top and right are anchored
          // to the `anchor_view`. The final bubble positioning will be computed
          // in GetBubbleBounds.
          views::BubbleBorder::Arrow::TOP_RIGHT,
          views::BubbleBorder::DIALOG_SHADOW,
          /*autosize=*/true),
      AccountSelectionViewBase(web_contents,
                               observer,
                               widget_observer,
                               std::move(url_loader_factory),
                               rp_for_display),
      rp_context_(rp_context) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_fixed_width(kBubbleWidth);
  set_margins(idp_title.has_value()
                  ? gfx::Insets::VH(kTopBottomPadding + kVerticalSpacing, 0)
                  : gfx::Insets::TLBR(kTopBottomPadding + kVerticalSpacing, 0,
                                      kVerticalSpacing, 0));
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

  title_ = webid::GetTitle(rp_for_display_, idp_title, rp_context);
  SetAccessibleTitle(title_);

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
  occlusion_observation_.Observe(widget);
}

void AccountSelectionBubbleView::ShowMultiAccountPicker(
    const std::vector<IdentityRequestAccountPtr>& accounts,
    const std::vector<IdentityProviderDataPtr>& idp_list,
    bool show_back_button,
    bool is_choose_an_account) {
  // If there are multiple IDPs, then the content::IdentityProviderMetadata
  // passed will be unused since there will be no `header_icon_view_`.
  // Therefore, it is fine to pass the first one into UpdateHeader().
  DCHECK(idp_list.size() == 1u || !header_icon_view_);
  DCHECK(!is_choose_an_account || show_back_button);
  std::u16string title =
      is_choose_an_account
          ? l10n_util::GetStringFUTF16(IDS_MULTI_IDP_CHOOSE_AN_ACCOUNT_TITLE,
                                       rp_for_display_)
          : webid::GetTitle(
                rp_for_display_,
                idp_list.size() > 1u
                    ? std::nullopt
                    : std::make_optional<std::u16string>(
                          base::UTF8ToUTF16(idp_list[0]->idp_for_display)),
                rp_context_);
  UpdateHeader(idp_list[0]->idp_metadata, title, show_back_button);

  RemoveNonHeaderChildViews();
  AddSeparatorAndMultipleAccountChooser(accounts, idp_list);

  if (!has_sheet_) {
    has_sheet_ = true;
    InitDialogWidget();
    return;
  }

  PreferredSizeChanged();
}

void AccountSelectionBubbleView::ShowVerifyingSheet(
    const content::IdentityRequestAccount& account,
    const std::u16string& title) {
  UpdateHeader(account.identity_provider->idp_metadata, title,
               /*show_back_button=*/false);

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
  row->AddChildView(CreateAccountRow(account,
                                     /*clickable_position=*/std::nullopt,
                                     /*should_include_idp=*/false));
  AddChildView(std::move(row));

  if (!has_sheet_) {
    has_sheet_ = true;
    InitDialogWidget();
    return;
  }

  PreferredSizeChanged();
}

void AccountSelectionBubbleView::ShowSingleAccountConfirmDialog(
    const content::IdentityRequestAccount& account,
    bool show_back_button) {
  std::u16string title = webid::GetTitle(
      rp_for_display_,
      base::UTF8ToUTF16(account.identity_provider->idp_for_display),
      rp_context_);
  UpdateHeader(account.identity_provider->idp_metadata, title,
               show_back_button);

  RemoveNonHeaderChildViews();
  AddChildView(std::make_unique<views::Separator>());
  AddChildView(CreateSingleAccountChooser(account));

  if (!has_sheet_) {
    has_sheet_ = true;
    InitDialogWidget();
    return;
  }

  PreferredSizeChanged();
}

void AccountSelectionBubbleView::ShowFailureDialog(
    const std::u16string& idp_for_display,
    const content::IdentityProviderMetadata& idp_metadata) {
  UpdateHeader(idp_metadata,
               webid::GetTitle(rp_for_display_, idp_for_display, rp_context_),
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
      l10n_util::GetStringUTF16(IDS_SIGNIN_CONTINUE), this, idp_metadata,
      /*extra_accessible_text=*/std::nullopt);
  row->AddChildView(std::move(button));
  AddChildView(std::move(row));

  if (!has_sheet_) {
    has_sheet_ = true;
    InitDialogWidget();
    return;
  }

  PreferredSizeChanged();
}

void AccountSelectionBubbleView::ShowErrorDialog(
    const std::u16string& idp_for_display,
    const content::IdentityProviderMetadata& idp_metadata,
    const std::optional<TokenError>& error) {
  std::u16string title =
      webid::GetTitle(rp_for_display_, idp_for_display, rp_context_);
  UpdateHeader(idp_metadata, title,
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
      GetErrorDialogText(error, rp_for_display_, idp_for_display);

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

  PreferredSizeChanged();
}

void AccountSelectionBubbleView::ShowLoadingDialog() {
  NOTREACHED_IN_MIGRATION()
      << "ShowLoadingDialog is only implemented for AccountSelectionModalView";
}

void AccountSelectionBubbleView::ShowRequestPermissionDialog(
    const content::IdentityRequestAccount& account,
    const content::IdentityProviderData& idp_data) {
  NOTREACHED_IN_MIGRATION()
      << "ShowRequestPermissionDialog is only implemented for "
         "AccountSelectionModalView";
}

void AccountSelectionBubbleView::ShowSingleReturningAccountDialog(
    const std::vector<IdentityRequestAccountPtr>& accounts,
    const std::vector<IdentityProviderDataPtr>& idp_list) {
  // We currently only invoke this method in the multi IDP case.
  DCHECK_GT(idp_list.size(), 1u);
  // Since there are multiple IDPs, then the content::IdentityProviderMetadata
  // passed will be unused since there will be no `header_icon_view_`.
  UpdateHeader(content::IdentityProviderMetadata(),
               webid::GetTitle(rp_for_display_, std::nullopt, rp_context_),
               /*show_back_button=*/false);

  RemoveNonHeaderChildViews();
  AddChildView(std::make_unique<views::Separator>());
  AddChildView(CreateSingleReturningAccountChooser(accounts, idp_list));

  if (!has_sheet_) {
    has_sheet_ = true;
    InitDialogWidget();
    return;
  }

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
  return base::UTF16ToUTF8(title_);
}

void AccountSelectionBubbleView::UpdateDialogPosition() {
  dialog_widget_->SetBounds(GetBubbleBounds());
}

void AccountSelectionBubbleView::OnAnchorBoundsChanged() {
  // TODO(crbug.com/342216390): It is unclear why there are callers where some
  // of these checks fail.
  if (!web_contents_) {
    return;
  }

  Browser* browser = chrome::FindBrowserWithTab(web_contents_.get());
  if (!browser || !browser->tab_strip_model()) {
    return;
  }

  // This method is called only if we didn't early return because there is a
  // crash (crbug.com/341240034) that is caused by calling this method and
  // subsequently, calling GetBubbleBounds() when the web contents is invalid.
  views::BubbleDialogDelegateView::OnAnchorBoundsChanged();
}

gfx::Rect AccountSelectionBubbleView::GetBubbleBounds() {
  // Since the top right corner of the bubble is set as the arrow in the ctor,
  // the top right corner of the bubble will be anchored to the origin, which we
  // set to be the top right corner of the web contents container.
  //       |-------------------------|
  //       |                |        |
  //       |                | bubble |
  //       |                |        |
  //       |                |--------|
  //       |                         |
  //       |   contents_web_view     |
  //       |          ...            |
  //       |-------------------------|
  // We also need to inset the web contents bounds by kTopMargin at the top and
  // kRightMargin either at the left or right, depending on whether RTL is
  // enabled, in order to leave some space between the bubble and the edges of
  // the web contents.
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
  // and the horizontal inset would apply to the left of the bubble.
  CHECK(web_contents_);

  gfx::Rect bubble_bounds = views::BubbleDialogDelegateView::GetBubbleBounds();
  gfx::Rect web_contents_bounds = web_contents_->GetViewBounds();
  if (base::i18n::IsRTL()) {
    web_contents_bounds.Inset(gfx::Insets::TLBR(
        /*top=*/kTopMargin, /*left=*/kRightMargin, /*bottom=*/0, /*right=*/0));
    bubble_bounds.set_origin(web_contents_->GetViewBounds().origin());
  } else {
    web_contents_bounds.Inset(gfx::Insets::TLBR(
        /*top=*/kTopMargin, /*left=*/0, /*bottom=*/0, /*right=*/kRightMargin));
    bubble_bounds.set_origin(web_contents_->GetViewBounds().top_right());
  }
  bubble_bounds.AdjustToFit(web_contents_bounds);

  return bubble_bounds;
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
        kBubbleIdpIconSize, /*should_circle_crop=*/true);
    image_view->SetImageSize(gfx::Size(kBubbleIdpIconSize, kBubbleIdpIconSize));
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
  return header;
}

std::unique_ptr<views::View>
AccountSelectionBubbleView::CreateSingleAccountChooser(
    const content::IdentityRequestAccount& account) {
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(0, kLeftRightPadding), kVerticalSpacing));
  row->AddChildView(CreateAccountRow(account,
                                     /*clickable_position=*/std::nullopt,
                                     /*should_include_idp=*/false));

  // Prefer using the given name if it is provided, otherwise fallback to name.
  const std::string display_name =
      account.given_name.empty() ? account.name : account.given_name;
  const content::IdentityProviderData& idp_data = *account.identity_provider;
  const content::IdentityProviderMetadata& idp_metadata = idp_data.idp_metadata;
  // We can pass crefs to OnAccountSelected because the `observer_` owns the
  // data.
  auto button = std::make_unique<ContinueButton>(
      base::BindRepeating(
          &AccountSelectionViewBase::Observer::OnAccountSelected,
          base::Unretained(observer_), std::cref(account), std::cref(idp_data)),
      l10n_util::GetStringFUTF16(IDS_ACCOUNT_SELECTION_CONTINUE,
                                 base::UTF8ToUTF16(display_name)),
      this, idp_metadata, base::UTF8ToUTF16(account.email));
  continue_button_ = row->AddChildView(std::move(button));

  // Do not add disclosure text if this is a sign in or if we were requested
  // to skip it.
  if (account.login_state == Account::LoginState::kSignIn ||
      idp_data.disclosure_fields.empty()) {
    return row;
  }

  // Add disclosure text.
  row->AddChildView(CreateDisclosureLabel(idp_data));
  return row;
}

void AccountSelectionBubbleView::AddSeparatorAndMultipleAccountChooser(
    const std::vector<IdentityRequestAccountPtr>& accounts,
    const std::vector<IdentityProviderDataPtr>& idp_list) {
  // We use a separate scroller for accounts vs for mismatches. This allows us
  // to show accounts at the top while still always showing mismatches in the UI
  // before any scrolling occurs.
  auto account_scroll_view = std::make_unique<views::ScrollView>();
  account_scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  views::View* const accounts_content =
      account_scroll_view->SetContents(std::make_unique<views::View>());
  accounts_content->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  bool is_multi_idp = idp_list.size() > 1u;
  AddAccounts(accounts, accounts_content, is_multi_idp);
  size_t num_account_rows = accounts.size();
  for (const auto& idp_data : idp_list) {
    const content::IdentityProviderMetadata& idp_metadata =
        idp_data->idp_metadata;
    if (idp_metadata.supports_add_account) {
      accounts_content->AddChildView(std::make_unique<views::Separator>());
      accounts_content->AddChildView(CreateUseOtherAccountButton(idp_metadata));
    }
  }

  bool starts_with_scroller = false;
  // The maximum height that the multi-account-picker can have. This value was
  // chosen so that if there are more than two accounts, the picker will show up
  // as a scrollbar showing 2 accounts plus half of the third one. Note that
  // this is an estimate if there are multiple IDPs, as IDP rows are not the
  // same height. That said, calling GetPreferredSize() is expensive so we are
  // ok with this estimate. And in this case, we prefer to use 3.5 as there will
  // be at least one IDP row at the beginning. Note that `num_account_rows` can
  // be 0 if everything is an IDP mismatch.
  if (num_account_rows > 0) {
    float num_visible_rows = is_multi_idp ? 3.5f : 2.5f;
    const int per_account_size =
        accounts_content->GetPreferredSize().height() / num_account_rows;
    account_scroll_view->ClipHeightTo(
        0, static_cast<int>(per_account_size * num_visible_rows));
    if (num_account_rows > num_visible_rows) {
      starts_with_scroller = true;
    } else {
      // We will have some spacing between the scroller and the separator at the
      // top but we need some additional spacing to match the bottom margin,
      // which is slightly larger in single IDP case.
      account_scroll_view->SetBorder(views::CreateEmptyBorder(
          gfx::Insets::TLBR(is_multi_idp ? kVerticalSpacing - kTopBottomPadding
                                         : kVerticalSpacing,
                            0, 0, 0)));
    }
  }

  auto mismatch_scroll_view = std::make_unique<views::ScrollView>();
  mismatch_scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  views::View* const mismatch_content =
      mismatch_scroll_view->SetContents(std::make_unique<views::View>());
  mismatch_content->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Add mismatch rows.
  size_t num_mismatch_rows = 0;
  for (const auto& idp_data : idp_list) {
    if (idp_data->has_login_status_mismatch) {
      mismatch_content->AddChildView(
          CreateIdpLoginRow(base::UTF8ToUTF16(idp_data->idp_for_display),
                            idp_data->idp_metadata));
      num_mismatch_rows += 1;
    }
  }
  if (num_mismatch_rows > 0) {
    // Similar to accounts scroller, clip the height of the mismatch dialog to
    // show at most 2.
    const int per_mismatch_size =
        mismatch_content->GetPreferredSize().height() / num_mismatch_rows;
    mismatch_scroll_view->ClipHeightTo(
        0, static_cast<int>(per_mismatch_size * 2.5f));
    if (num_account_rows == 0) {
      if (num_mismatch_rows > 2) {
        starts_with_scroller = true;
      } else {
        // We will have some spacing between the scroller and the separator at
        // the top but we need some additional spacing to match the bottom
        // margin. Note that this can only happen when there are multiple IDPs.
        mismatch_scroll_view->SetBorder(views::CreateEmptyBorder(
            gfx::Insets::TLBR(kVerticalSpacing - kTopBottomPadding, 0, 0, 0)));
      }
    }
  }

  // We use a container for most of the contents here. If there is a scroller at
  // the start, we include the separator so that there is no spacing between the
  // separator and the scroller. And we also always include the accounts
  // followed by the IDP mismatches.
  auto container = std::make_unique<views::View>();
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  auto separator = std::make_unique<views::Separator>();
  if (!starts_with_scroller) {
    separator->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(0, 0, kTopBottomPadding, 0)));
  }
  container->AddChildView(std::move(separator));
  container->AddChildView(std::move(account_scroll_view));
  // If there are both accounts and mismatches, add a separator.
  if (num_account_rows > 0 && num_mismatch_rows > 0) {
    container->AddChildView(std::make_unique<views::Separator>());
  }
  container->AddChildView(std::move(mismatch_scroll_view));
  AddChildView(std::move(container));
}

void AccountSelectionBubbleView::AddAccounts(
    const std::vector<IdentityRequestAccountPtr>& accounts,
    views::View* accounts_content,
    bool is_multi_idp) {
  int out_position = 0;
  if (!is_multi_idp) {
    for (const auto& account : accounts) {
      accounts_content->AddChildView(
          CreateAccountRow(*account, /*clickable_position=*/out_position++,
                           /*should_include_idp=*/false));
    }
    return;
  }
  std::optional<base::Time> now;
  for (const auto& account : accounts) {
    std::optional<std::u16string> last_used_string;
    if (account->last_used_timestamp) {
      // For the most recently used account, we want to show "last used on this
      // site" while for all other accounts we want to show timing regarding
      // when it was last used ("last used 1 month ago"). |now| is set when the
      // first account is seen, so !|now| is only true for the most recently
      // used account.
      if (!now) {
        last_used_string = l10n_util::GetStringUTF16(
            IDS_MULTI_IDP_ACCOUNT_LAST_USED_ON_THIS_SITE);
        now = base::Time::Now();
      } else {
        // ui::TimeFormat::SimpleWithMonthAndYear does not support negative
        // values, so if the value is negative, make it 0.
        base::TimeDelta delta =
            std::max(*now - *account->last_used_timestamp, base::TimeDelta());
        last_used_string = l10n_util::GetStringFUTF16(
            IDS_MULTI_IDP_ACCOUNT_USED_TIME_AGO,
            ui::TimeFormat::SimpleWithMonthAndYear(
                ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_LONG,
                delta, true));
      }
    }
    accounts_content->AddChildView(
        CreateAccountRow(*account, /*clickable_position=*/out_position++,
                         /*should_include_idp=*/true, /*is_modal_dialog=*/false,
                         /*additional_vertical_padding=*/0, last_used_string));
  }
}

std::unique_ptr<views::View>
AccountSelectionBubbleView::CreateSingleReturningAccountChooser(
    const std::vector<IdentityRequestAccountPtr>& accounts,
    const std::vector<IdentityProviderDataPtr>& idp_list) {
  std::vector<std::u16string> mismatch_idps;
  std::vector<std::u16string> non_mismatch_idps;
  for (const auto& idp : idp_list) {
    if (idp->has_login_status_mismatch) {
      mismatch_idps.push_back(base::UTF8ToUTF16(idp->idp_for_display));
    } else {
      non_mismatch_idps.push_back(base::UTF8ToUTF16(idp->idp_for_display));
    }
  }
  CHECK(!accounts.empty() &&
        accounts[0]->login_state == Account::LoginState::kSignIn);
  auto content = std::make_unique<views::View>();
  // Add spacing at the top to make the total spacing between it and the
  // separator be kVertical spacing, and also add kVertical spacing between
  // children since there is a separator between them.1
  content->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(kVerticalSpacing - kTopBottomPadding, 0, 0, 0),
      kVerticalSpacing));
  std::optional<std::u16string> last_used_string =
      accounts[0]->last_used_timestamp
          ? std::make_optional<std::u16string>(l10n_util::GetStringUTF16(
                IDS_MULTI_IDP_ACCOUNT_LAST_USED_ON_THIS_SITE))
          : std::nullopt;
  content->AddChildView(CreateAccountRow(*accounts[0],
                                         /*clickable_position=*/0,
                                         /*should_include_idp=*/true,
                                         /*is_modal_dialog=*/false,
                                         /*additional_vertical_padding=*/0,
                                         last_used_string));
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
      kMultiIdpIconSize, /*should_circle_crop=*/true);
  image_view->SetImageSize(gfx::Size(kMultiIdpIconSize, kMultiIdpIconSize));
  image_view->SetVisible(idp_metadata.brand_icon_url.is_valid());
  ConfigureBrandImageView(image_view.get(), idp_metadata.brand_icon_url);

  auto button = std::make_unique<HoverButton>(
      base::BindRepeating(&AccountSelectionViewBase::Observer::OnLoginToIdP,
                          base::Unretained(observer_), idp_metadata.config_url,
                          idp_metadata.idp_login_url),
      std::move(image_view),
      l10n_util::GetStringFUTF16(IDS_IDP_SIGNIN_STATUS_MISMATCH_BUTTON_TEXT,
                                 idp_for_display),
      /*subtitle=*/std::u16string(),
      /*secondary_view=*/
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          kOpenInNewIcon, ui::kColorMenuIcon, kBubbleIdpIconSize)));
  button->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
      /*vertical=*/kMultiIdpVerticalSpacing,
      /*horizontal=*/kLeftRightPadding)));
  button->SetIconHorizontalMargins(kMultiIdpIconLeftMargin,
                                   kMultiIdpIconRightMargin);
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
                                     kIdpLoginIconSize),
      l10n_util::GetStringUTF16(IDS_ACCOUNT_SELECTION_USE_OTHER_ACCOUNT));
  button->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      /*top=*/2 * kVerticalSpacing, /*left=*/kLeftRightPadding, /*bottom=*/0,
      /*right=*/kLeftRightPadding)));
  return button;
}

void AccountSelectionBubbleView::UpdateHeader(
    const content::IdentityProviderMetadata& idp_metadata,
    const std::u16string& title,
    bool show_back_button) {
  back_button_->SetVisible(show_back_button);
  if (header_icon_view_) {
    // The back button takes the place of the brand icon, if it is shown. By
    // default, we show placeholder brand icon prior to brand icon being fetched
    // so that header text wrapping does not change when brand icon is fetched.
    // Therefore, we need to hide the brand icon if the URL is invalid.
    if (show_back_button || !idp_metadata.brand_icon_url.is_valid()) {
      header_icon_view_->SetVisible(false);
    } else {
      ConfigureBrandImageView(header_icon_view_, idp_metadata.brand_icon_url);
      header_icon_view_->SetVisible(true);
    }
  }
  if (title.compare(title_) != 0) {
    title_ = title;
    title_label_->SetText(title_);
    SetAccessibleTitle(title_);
    // The title label is not destroyed, so announce it manually.
    webid::SendAccessibilityEvent(GetWidget(), title_);
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
    const std::vector<std::u16string>& mismatch_idps,
    const std::vector<std::u16string>& non_mismatch_idps) {
  // TODO(crbug.com/325503352): `icon_view` should probably be smaller while
  // still taking the same amount of space.
  auto icon_view =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          kPersonIcon, ui::kColorMenuIcon, kMultiIdpIconSize));
  auto button = std::make_unique<HoverButton>(
      base::BindOnce(
          &AccountSelectionViewBase::Observer::OnChooseAnAccountClicked,
          base::Unretained(observer_)),
      std::move(icon_view),
      /*title=*/
      l10n_util::GetStringUTF16(IDS_ACCOUNT_SELECTION_CHOOSE_AN_ACCOUNT_BUTTON),
      /*subtitle=*/BuildStringFromIDPs(mismatch_idps, non_mismatch_idps),
      /*secondary_view=*/
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          kKeyboardArrowRightIcon, ui::kColorMenuIcon, kMultiIdpIconSize)));
  button->SetSubtitleTextStyle(views::style::CONTEXT_LABEL,
                               views::style::STYLE_SECONDARY);
  button->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
      /*vertical=*/kMultiIdpVerticalSpacing,
      /*horizontal=*/kLeftRightPadding)));
  button->SetIconHorizontalMargins(kMultiIdpIconLeftMargin,
                                   kMultiIdpIconRightMargin);
  return button;
}

BEGIN_METADATA(AccountSelectionBubbleView)
END_METADATA
