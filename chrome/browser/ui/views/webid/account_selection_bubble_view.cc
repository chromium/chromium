// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"

#include "base/i18n/case_conversion.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/monogram_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/platform_locale_settings.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "content/public/browser/storage_partition.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "skia/ext/image_operations.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

// The radius used for the corner of the "Continue as" button.
constexpr int kButtonRadius = 16;
// The fixed, total width of the bubble.
constexpr int kBubbleWidth = 375;
// The desired size of the avatars of user accounts.
constexpr int kDesiredAvatarSize = 30;
// The desired size of the icon of the identity provider.
constexpr int kDesiredIconSize = 20;
// The size of the padding used at the top and bottom of the bubble.
constexpr int kTopBottomPadding = 4;
// The size of the horizontal padding between the bubble content and the edge of
// the bubble, as well as the horizontal padding between icons and text.
constexpr int kLeftRightPadding = 12;
// The size of the vertical padding for most elements in the bubble.
constexpr int kVerticalSpacing = 8;
// The height of the progress bar shown when showing "Verifying...".
constexpr int kProgressBarHeight = 2;

constexpr char kImageFetcherUmaClient[] = "FedCMAccountChooser";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("fedcm_account_profile_image_fetcher",
                                        R"(
        semantics {
          sender: "Profile image fetcher for FedCM Account chooser on desktop."
          description:
            "Retrieves profile images for user's accounts in the FedCM login"
            "flow."
          trigger:
            "Triggered when FedCM API is called and account chooser shows up."
            "The accounts shown are ones for which the user has previously"
            "signed into the identity provider."
          data:
            "Account picture URL of user account, provided by the identity"
            "provider."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature in chrome://settings, under"
            "'Privacy and security', then 'Site Settings', and finally"
            "'Third party sign-in'."
          policy_exception_justification:
            "Not implemented. This is a feature that sites use for"
            "Federated Sign-In, for which we do not have an Enterprise policy."
        })");

// A CanvasImageSource that fills a gray circle with a monogram.
class LetterAvatarImageSkiaSource : public gfx::CanvasImageSource {
 public:
  LetterAvatarImageSkiaSource(const std::u16string& letter, int size)
      : gfx::CanvasImageSource(gfx::Size(size, size)), letter_(letter) {}

  void Draw(gfx::Canvas* canvas) override {
    monogram::DrawMonogramInCanvas(canvas, size().width(), size().width(),
                                   letter_, SK_ColorWHITE, SK_ColorGRAY);
  }

 private:
  const std::u16string letter_;
};

void SendAccessibilityEvent(views::Widget* widget) {
  if (!widget)
    return;

  widget->GetRootView()->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                                  true);
}

}  // namespace

AccountSelectionBubbleView::AccountSelectionBubbleView(
    const std::string& rp_etld_plus_one,
    const std::string& idp_etld_plus_one,
    base::span<const content::IdentityRequestAccount> accounts,
    const content::IdentityProviderMetadata& idp_metadata,
    const content::ClientIdData& client_data,
    views::View* anchor_view,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TabStripModel* tab_strip_model,
    base::OnceCallback<void(const content::IdentityRequestAccount&)>
        on_account_selected_callback)
    : views::BubbleDialogDelegateView(anchor_view,
                                      views::BubbleBorder::Arrow::TOP_RIGHT),
      idp_etld_plus_one_(base::UTF8ToUTF16(idp_etld_plus_one)),
      brand_text_color_(idp_metadata.brand_text_color),
      brand_background_color_(idp_metadata.brand_background_color),
      tab_strip_model_(tab_strip_model),
      on_account_selected_callback_(std::move(on_account_selected_callback)),
      client_data_(client_data) {
  image_fetcher_ = std::make_unique<image_fetcher::ImageFetcherImpl>(
      std::make_unique<ImageDecoderImpl>(), url_loader_factory);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_fixed_width(kBubbleWidth);
  set_margins(gfx::Insets::VH(kTopBottomPadding + kVerticalSpacing, 0));
  // TODO(crbug.com/1323298): we are currently using a custom header because the
  // icon, title, and close buttons from a bubble are not customizable enough to
  // satisfy the UI requirements. However, this adds complexity to the code and
  // makes this bubble lose any improvements made to the base bubble, so we
  // should revisit this.
  SetShowTitle(false);
  SetShowCloseButton(false);
  set_close_on_deactivate(false);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kTopBottomPadding));
  std::u16string title = l10n_util::GetStringFUTF16(
      IDS_ACCOUNT_SELECTION_SHEET_TITLE_EXPLICIT,
      base::UTF8ToUTF16(rp_etld_plus_one), idp_etld_plus_one_);
  gfx::ImageSkia icon =
      gfx::ImageSkia::CreateFrom1xBitmap(skia::ImageOperations::Resize(
          idp_metadata.brand_icon, skia::ImageOperations::RESIZE_LANCZOS3,
          kDesiredIconSize, kDesiredIconSize));
  header_view_ = AddChildView(CreateHeaderView(icon, title));
  AddChildView(std::make_unique<views::Separator>());
  AddChildView(CreateAccountChooser(accounts));
}

AccountSelectionBubbleView::~AccountSelectionBubbleView() = default;

std::unique_ptr<views::View> AccountSelectionBubbleView::CreateHeaderView(
    gfx::ImageSkia icon,
    const std::u16string& title) {
  auto header = std::make_unique<views::View>();
  // Do not use a top margin as it has already been set in the bubble.
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetInteriorMargin(gfx::Insets::TLBR(
          0, kLeftRightPadding, kVerticalSpacing, kLeftRightPadding));

  // Add the icon.
  auto image_view = std::make_unique<views::ImageView>();
  image_view->SetImage(icon);
  image_view->SetProperty(views::kMarginsKey,
                          gfx::Insets().set_right(kLeftRightPadding));
  header->AddChildView(image_view.release());

  // Add the title.
  title_label_ = header->AddChildView(std::make_unique<views::Label>(
      title, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY));
  title_label_->SetMultiLine(true);
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetAllowCharacterBreak(true);
  title_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true));

  // Add the close button.
  std::unique_ptr<views::Button> close_button =
      views::BubbleFrameView::CreateCloseButton(
          base::BindRepeating(&AccountSelectionBubbleView::CloseBubble,
                              weak_ptr_factory_.GetWeakPtr()));
  close_button->SetVisible(true);
  header->AddChildView(close_button.release());
  return header;
}

void AccountSelectionBubbleView::CloseBubble() {
  if (!GetWidget())
    return;
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

std::unique_ptr<views::View> AccountSelectionBubbleView::CreateAccountChooser(
    base::span<const content::IdentityRequestAccount> accounts) {
  DCHECK(!accounts.empty());
  if (accounts.size() == 1u) {
    return CreateSingleAccountChooser(accounts.front());
  }
  return CreateMultipleAccountChooser(accounts);
}

std::unique_ptr<views::View>
AccountSelectionBubbleView::CreateSingleAccountChooser(
    const content::IdentityRequestAccount& account) {
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(0, kLeftRightPadding), kVerticalSpacing));
  row->AddChildView(CreateAccountRow(account, /*should_hover=*/false));

  // Prefer using the given name if it is provided, otherwise fallback to name.
  std::string display_name =
      account.given_name.empty() ? account.name : account.given_name;
  auto button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&AccountSelectionBubbleView::OnAccountSelected,
                          weak_ptr_factory_.GetWeakPtr(), account),
      l10n_util::GetStringFUTF16(IDS_ACCOUNT_SELECTION_CONTINUE,
                                 base::UTF8ToUTF16(display_name)));
  button->SetCornerRadius(kButtonRadius);
  button->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
  if (brand_background_color_)
    button->SetBgColorOverride(*brand_background_color_);
  if (brand_text_color_)
    button->SetEnabledTextColors(brand_text_color_);
  button->SetProminent(true);
  continue_button_ = row->AddChildView(std::move(button));

  // Do not add disclosure text if this is a sign in.
  if (account.login_state == Account::LoginState::kSignIn)
    return row;

  // Add disclosure text. It requires a StyledLabel so that we can add the links
  // to the privacy policy and terms of service URLs.
  views::StyledLabel* disclosure_label =
      row->AddChildView(std::make_unique<views::StyledLabel>());
  // TODO(crbug.com/1324689): remove the IsRTL() check and instead replace with
  // just gfx::HorizontalAlignment::ALIGN_LEFT when
  // StyledLabel::SetHorizontalAlignment() does mirror in RTL.
  disclosure_label->SetHorizontalAlignment(
      base::i18n::IsRTL() ? gfx::HorizontalAlignment::ALIGN_RIGHT
                          : gfx::HorizontalAlignment::ALIGN_LEFT);

  // Set custom top margin for `disclosure_label` in order to take
  // (line_height - font_height) into account.
  disclosure_label->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(5, 0, 0, 0)));
  disclosure_label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);

  std::vector<size_t> offsets;

  if (client_data_.privacy_policy_url.is_empty() &&
      client_data_.terms_of_service_url.is_empty()) {
    // Case for both the privacy policy and terms of service URLs are missing.
    std::u16string disclosure_text = l10n_util::GetStringFUTF16(
        IDS_ACCOUNT_SELECTION_DATA_SHARING_CONSENT_NO_PP_OR_TOS,
        {idp_etld_plus_one_});
    disclosure_label->SetText(disclosure_text);
    return row;
  }

  if (client_data_.privacy_policy_url.is_empty()) {
    // Case for when we only need to add a link for terms of service URL, but
    // not privacy policy. We use two placeholders for the start and end of
    // 'terms of service' in order to style that text as a link.
    std::u16string disclosure_text = l10n_util::GetStringFUTF16(
        IDS_ACCOUNT_SELECTION_DATA_SHARING_CONSENT_NO_PP,
        {idp_etld_plus_one_, std::u16string(), std::u16string()}, &offsets);
    disclosure_label->SetText(disclosure_text);
    // Add link styling for terms of service url.
    disclosure_label->AddStyleRange(
        gfx::Range(offsets[1], offsets[2]),
        views::StyledLabel::RangeStyleInfo::CreateForLink(
            base::BindRepeating(&AccountSelectionBubbleView::OnLinkClicked,
                                weak_ptr_factory_.GetWeakPtr(),
                                client_data_.terms_of_service_url)));
    return row;
  }

  if (client_data_.terms_of_service_url.is_empty()) {
    // Case for when we only need to add a link for privacy policy URL, but not
    // terms of service. We use two placeholders for the start and end of
    // 'privacy policy' in order to style that text as a link.
    std::u16string disclosure_text = l10n_util::GetStringFUTF16(
        IDS_ACCOUNT_SELECTION_DATA_SHARING_CONSENT_NO_TOS,
        {idp_etld_plus_one_, std::u16string(), std::u16string()}, &offsets);
    disclosure_label->SetText(disclosure_text);
    // Add link styling for privacy policy url.
    disclosure_label->AddStyleRange(
        gfx::Range(offsets[1], offsets[2]),
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &AccountSelectionBubbleView::OnLinkClicked,
            weak_ptr_factory_.GetWeakPtr(), client_data_.privacy_policy_url)));
    return row;
  }

  // Case for when we add a link for privacy policy URL as well as
  // terms of service URL. We use four placeholders at start/end of both
  // 'privacy policy' and 'terms of service' in order to style both of them as
  // links.
  std::vector<std::u16string> replacements = {
      idp_etld_plus_one_, std::u16string(), std::u16string(), std::u16string(),
      std::u16string()};
  std::u16string disclosure_text = l10n_util::GetStringFUTF16(
      IDS_ACCOUNT_SELECTION_DATA_SHARING_CONSENT, replacements, &offsets);
  disclosure_label->SetText(disclosure_text);
  // Add link styling for privacy policy url.
  disclosure_label->AddStyleRange(
      gfx::Range(offsets[1], offsets[2]),
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &AccountSelectionBubbleView::OnLinkClicked,
          weak_ptr_factory_.GetWeakPtr(), client_data_.privacy_policy_url)));
  // Add link styling for terms of service url.
  disclosure_label->AddStyleRange(
      gfx::Range(offsets[3], offsets[4]),
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &AccountSelectionBubbleView::OnLinkClicked,
          weak_ptr_factory_.GetWeakPtr(), client_data_.terms_of_service_url)));
  return row;
}

std::unique_ptr<views::View>
AccountSelectionBubbleView::CreateMultipleAccountChooser(
    base::span<const content::IdentityRequestAccount> accounts) {
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  for (const auto& account : accounts) {
    row->AddChildView(CreateAccountRow(account, /*should_hover=*/true));
  }
  return row;
}

std::unique_ptr<views::View> AccountSelectionBubbleView::CreateAccountRow(
    const content::IdentityRequestAccount& account,
    bool should_hover) {
  auto image_view = std::make_unique<views::ImageView>();
  image_view->SetImageSize({kDesiredAvatarSize, kDesiredAvatarSize});
  image_fetcher::ImageFetcherParams params(kTrafficAnnotation,
                                           kImageFetcherUmaClient);
  std::u16string account_name16 = base::UTF8ToUTF16(account.name);
  image_fetcher_->FetchImage(
      account.picture,
      base::BindOnce(&AccountSelectionBubbleView::OnAccountImageFetched,
                     weak_ptr_factory_.GetWeakPtr(), image_view.get(),
                     account_name16),
      std::move(params));
  if (should_hover) {
    auto row = std::make_unique<HoverButton>(
        base::BindRepeating(&AccountSelectionBubbleView::OnSingleAccountPicked,
                            weak_ptr_factory_.GetWeakPtr(), account),
        std::move(image_view), account_name16,
        base::UTF8ToUTF16(account.email));
    row->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::VH(/*vertical=*/0, /*horizontal=*/kLeftRightPadding)));
    row->SetSubtitleTextStyle(views::style::CONTEXT_LABEL,
                              views::style::STYLE_SECONDARY);
    return row;
  }
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(/*vertical=*/kVerticalSpacing, /*horizontal=*/0),
      kLeftRightPadding));
  row->AddChildView(std::move(image_view));
  views::View* text_column = row->AddChildView(std::make_unique<views::View>());
  text_column->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Add account name.
  views::Label* account_name =
      text_column->AddChildView(std::make_unique<views::Label>(
          base::UTF8ToUTF16(account.name),
          views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));
  account_name->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  // Add account email.
  views::Label* account_email = text_column->AddChildView(
      std::make_unique<views::Label>(base::UTF8ToUTF16(account.email),
                                     views::style::CONTEXT_DIALOG_BODY_TEXT,
                                     views::style::STYLE_SECONDARY));
  account_email->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  return row;
}

void AccountSelectionBubbleView::OnAccountImageFetched(
    views::ImageView* image_view,
    const std::u16string& account_name,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  ui::ImageModel avatar;
  if (image.IsEmpty()) {
    std::u16string letter = account_name;
    if (letter.length() > 0)
      letter = base::i18n::ToUpper(account_name.substr(0, 1));
    avatar = ui::ImageModel::FromImageSkia(
        gfx::CanvasImageSource::MakeImageSkia<LetterAvatarImageSkiaSource>(
            letter, kDesiredAvatarSize));
  } else {
    avatar = ui::ImageModel::FromImage(profiles::GetSizedAvatarIcon(
        image, /*is_rectangle=*/true, kDesiredAvatarSize, kDesiredAvatarSize,
        profiles::SHAPE_CIRCLE));
  }
  image_view->SetImage(avatar);
}

void AccountSelectionBubbleView::OnLinkClicked(const GURL& gurl) {
  DCHECK(tab_strip_model_);
  // Add a tab for the URL at the end of the tab strip, in the foreground.
  tab_strip_model_->delegate()->AddTabAt(gurl, -1, true);
}

void AccountSelectionBubbleView::OnSingleAccountPicked(
    const content::IdentityRequestAccount& account) {
  if (account.login_state == Account::LoginState::kSignIn) {
    OnAccountSelected(account);
    return;
  }
  RemoveNonHeaderChildViews();
  AddChildView(std::make_unique<views::Separator>());
  std::vector<content::IdentityRequestAccount> accounts = {account};
  AddChildView(CreateAccountChooser(accounts));
  SizeToContents();
  PreferredSizeChanged();

  // Focusing `continue_button_` without screen reader on makes the UI look
  // awkward, so we only want to do so when screen reader is enabled.
  if (accessibility_state_utils::IsScreenReaderEnabled())
    continue_button_->RequestFocus();
  SendAccessibilityEvent(GetWidget());
}

void AccountSelectionBubbleView::OnAccountSelected(
    const content::IdentityRequestAccount& account) {
  ShowVerifySheet(account);
  std::move(on_account_selected_callback_).Run(account);
}

void AccountSelectionBubbleView::ShowVerifySheet(
    const content::IdentityRequestAccount& account) {
  RemoveNonHeaderChildViews();
  title_label_->SetText(l10n_util::GetStringUTF16(IDS_VERIFY_SHEET_TITLE));
  views::ProgressBar* progress_bar =
      AddChildView(std::make_unique<views::ProgressBar>(kProgressBarHeight));
  // Use an infinite animation: SetValue(-1).
  progress_bar->SetValue(-1);
  progress_bar->SetBackgroundColor(SK_ColorLTGRAY);
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kTopBottomPadding, kLeftRightPadding)));
  row->AddChildView(CreateAccountRow(account, /*should_hover=*/false));
  AddChildView(row.release());
  SizeToContents();
  PreferredSizeChanged();

  SendAccessibilityEvent(GetWidget());
}

void AccountSelectionBubbleView::RemoveNonHeaderChildViews() {
  std::vector<views::View*> child_views = children();
  for (views::View* child_view : child_views) {
    if (child_view != header_view_) {
      RemoveChildView(child_view);
      delete child_view;
    }
  }
}

BEGIN_METADATA(AccountSelectionBubbleView, views::BubbleDialogDelegateView)
END_METADATA
