// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "content/public/browser/storage_partition.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"

namespace {

constexpr int kButtonRadius = 20;
constexpr int kBubbleWidth = 375;
constexpr int kDesiredAvatarSize = 40;
constexpr int kPadding = 5;

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

}  // namespace

AccountSelectionBubbleView::AccountSelectionBubbleView(
    AccountSelectionView::Delegate* delegate,
    const std::string& rp_etld_plus_one,
    const std::string& idp_etld_plus_one,
    base::span<const content::IdentityRequestAccount> accounts,
    const content::IdentityProviderMetadata& idp_metadata,
    const content::ClientIdData& client_data,
    views::View* anchor_view,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TabStripModel* tab_strip_model)
    : views::BubbleDialogDelegateView(anchor_view,
                                      views::BubbleBorder::Arrow::TOP_RIGHT),
      idp_etld_plus_one_(base::ASCIIToUTF16(idp_etld_plus_one)),
      brand_text_color_(idp_metadata.brand_text_color),
      brand_background_color_(idp_metadata.brand_background_color),
      tab_strip_model_(tab_strip_model) {
  image_fetcher_ = std::make_unique<image_fetcher::ImageFetcherImpl>(
      std::make_unique<ImageDecoderImpl>(), url_loader_factory);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_fixed_width(kBubbleWidth);
  set_margins(gfx::Insets(kPadding));
  set_close_on_deactivate(false);
  SetTitle(l10n_util::GetStringFUTF16(
      IDS_ACCOUNT_SELECTION_SHEET_TITLE_EXPLICIT,
      base::ASCIIToUTF16(rp_etld_plus_one), idp_etld_plus_one_));
  auto imageSkia = gfx::ImageSkia::CreateFrom1xBitmap(idp_metadata.brand_icon);
  SetIcon(imageSkia);
  SetShowIcon(true);
  SetShowCloseButton(true);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  AddChildView(std::make_unique<views::Separator>());
  AddChildView(CreateAccountChooser(accounts, client_data));
}

AccountSelectionBubbleView::~AccountSelectionBubbleView() = default;

std::unique_ptr<views::View> AccountSelectionBubbleView::CreateAccountChooser(
    base::span<const content::IdentityRequestAccount> accounts,
    const content::ClientIdData& client_data) {
  DCHECK(!accounts.empty());
  if (accounts.size() == 1u) {
    return CreateSingleAccountChooser(accounts.front(), client_data);
  }
  return CreateMultipleAccountChooser(accounts, client_data);
}

std::unique_ptr<views::View>
AccountSelectionBubbleView::CreateSingleAccountChooser(
    const content::IdentityRequestAccount& account,
    const content::ClientIdData& client_data) {
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  row->AddChildView(CreateAccountRow(account, /*should_hover=*/false));

  // Prefer using the given name if it is provided, otherwise fallback to name.
  std::string display_name =
      account.given_name.empty() ? account.name : account.given_name;
  auto button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&AccountSelectionBubbleView::OnAccountSelected,
                          weak_ptr_factory_.GetWeakPtr()),
      l10n_util::GetStringFUTF16(IDS_ACCOUNT_SELECTION_CONTINUE,
                                 base::ASCIIToUTF16(display_name)));
  button->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
  if (brand_background_color_) {
    button->SetBackground(views::CreateRoundedRectBackground(
        *brand_background_color_, kButtonRadius));
  }
  if (brand_text_color_) {
    button->SetTextColor(views::Button::ButtonState::STATE_NORMAL,
                         *brand_text_color_);
    button->SetTextColor(views::Button::ButtonState::STATE_HOVERED,
                         *brand_text_color_);
    button->SetTextColor(views::Button::ButtonState::STATE_PRESSED,
                         *brand_text_color_);
  }
  button->SetProminent(true);
  row->AddChildView(std::move(button));

  // Add consent text. It requires a StyledLabel so that we can add the links to
  // the privacy policy and terms of service URLs.
  views::StyledLabel* consent_label =
      row->AddChildView(std::make_unique<views::StyledLabel>());
  consent_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  std::vector<size_t> offsets;
  if (client_data.terms_of_service_url.is_empty()) {
    // Case for when we only need to add a link for privacy policy URL, but not
    // terms of service. We use two placeholders for the start and end of
    // 'privacy policy' in order to style that text as a link.
    std::u16string consent_text = l10n_util::GetStringFUTF16(
        IDS_ACCOUNT_SELECTION_DATA_SHARING_CONSENT_NO_TOS,
        {idp_etld_plus_one_, std::u16string(), std::u16string()}, &offsets);
    consent_label->SetText(consent_text);
    // Add link styling for privacy policy url.
    consent_label->AddStyleRange(
        gfx::Range(offsets[1], offsets[2]),
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &AccountSelectionBubbleView::OnLinkClicked,
            weak_ptr_factory_.GetWeakPtr(), client_data.privacy_policy_url)));
    return row;
  }

  // Case for when we add a link for privacy policy URL as well as
  // terms of service URL. We use four placeholders at start/end of both
  // 'privacy policy' and 'terms of service' in order to style both of them as
  // links.
  std::vector<std::u16string> replacements = {
      idp_etld_plus_one_, std::u16string(), std::u16string(), std::u16string(),
      std::u16string()};
  std::u16string consent_text = l10n_util::GetStringFUTF16(
      IDS_ACCOUNT_SELECTION_DATA_SHARING_CONSENT, replacements, &offsets);
  consent_label->SetText(consent_text);
  // Add link styling for privacy policy url.
  consent_label->AddStyleRange(
      gfx::Range(offsets[1], offsets[2]),
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &AccountSelectionBubbleView::OnLinkClicked,
          weak_ptr_factory_.GetWeakPtr(), client_data.privacy_policy_url)));
  // Add link styling for terms of service url.
  consent_label->AddStyleRange(
      gfx::Range(offsets[3], offsets[4]),
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &AccountSelectionBubbleView::OnLinkClicked,
          weak_ptr_factory_.GetWeakPtr(), client_data.terms_of_service_url)));
  return row;
}

std::unique_ptr<views::View>
AccountSelectionBubbleView::CreateMultipleAccountChooser(
    base::span<const content::IdentityRequestAccount> accounts,
    const content::ClientIdData& client_data) {
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
  image_fetcher_->FetchImage(
      account.picture,
      base::BindOnce(&AccountSelectionBubbleView::OnAccountImageFetched,
                     weak_ptr_factory_.GetWeakPtr(), image_view.get()),
      std::move(params));
  if (should_hover) {
    auto row = std::make_unique<HoverButton>(
        base::BindRepeating(&AccountSelectionBubbleView::OnSingleAccountPicked,
                            weak_ptr_factory_.GetWeakPtr()),
        std::move(image_view), base::ASCIIToUTF16(account.name),
        base::ASCIIToUTF16(account.email));
    row->SetImageModel(views::Button::STATE_NORMAL, ui::ImageModel());
    return row;
  }
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>());
  row->AddChildView(std::move(image_view));
  views::View* text_column = row->AddChildView(std::make_unique<views::View>());
  text_column->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  text_column
      ->AddChildView(
          std::make_unique<views::Label>(base::ASCIIToUTF16(account.name)))
      ->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  text_column
      ->AddChildView(
          std::make_unique<views::Label>(base::ASCIIToUTF16(account.email)))
      ->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  const views::LayoutProvider* provider = views::LayoutProvider::Get();
  text_column->SetBorder(
      views::CreateEmptyBorder(provider->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kText)));

  return row;
}

void AccountSelectionBubbleView::OnAccountImageFetched(
    views::ImageView* image_view,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  // TODO(npm): transform the image into a circle.
  image_view->SetImage(ui::ImageModel::FromImage(image));
}

void AccountSelectionBubbleView::OnLinkClicked(const GURL& gurl) {
  DCHECK(tab_strip_model_);
  // Add a tab for the URL at the end of the tab strip, in the foreground.
  tab_strip_model_->delegate()->AddTabAt(gurl, -1, true);
}

void AccountSelectionBubbleView::OnSingleAccountPicked() {
  // TODO(npm): transform the bubble once an account is selected.
}

void AccountSelectionBubbleView::OnAccountSelected() {
  // TODO(npm): call the FedCM API here.
}

BEGIN_METADATA(AccountSelectionBubbleView, views::BubbleDialogDelegateView)
END_METADATA
