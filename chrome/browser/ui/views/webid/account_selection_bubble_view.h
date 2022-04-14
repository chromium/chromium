// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_BUBBLE_VIEW_H_

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"

// Bubble dialog that is used in the FedCM flow. It creates a dialog with an
// account chooser for the user, and it changes the content of that dialog as
// user moves through the FedCM flow steps.
class AccountSelectionBubbleView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(AccountSelectionBubbleView);
  AccountSelectionBubbleView(
      AccountSelectionView::Delegate* delegate,
      const std::string& rp_etld_plus_one,
      const std::string& idp_etld_plus_one,
      base::span<const content::IdentityRequestAccount> accounts,
      const content::IdentityProviderMetadata& idp_metadata,
      const content::ClientIdData& client_data,
      views::View* anchor_view,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      TabStripModel* tab_strip_model);
  ~AccountSelectionBubbleView() override;

 private:
  // Returns a View containing the account chooser, i.e. everything that goes
  // below the horizontal separator on the initial FedCM bubble.
  std::unique_ptr<views::View> CreateAccountChooser(
      base::span<const content::IdentityRequestAccount> accounts,
      const content::ClientIdData& client_data);

  // Returns a View for single account chooser. It contains the account
  // information, disclosure text and a button for the user to confirm the
  // selection.
  std::unique_ptr<views::View> CreateSingleAccountChooser(
      const content::IdentityRequestAccount& account,
      const content::ClientIdData& client_data);

  // Returns a View for multiple account chooser. It contains the info for each
  // account in a button, so the user can pick an account.
  std::unique_ptr<views::View> CreateMultipleAccountChooser(
      base::span<const content::IdentityRequestAccount> accounts,
      const content::ClientIdData& client_data);

  // Returns a View containing information about an account: the picture for the
  // account on the left, and information about the account on the right.
  // |should_hover| determines whether the account row is a HoverButton or not.
  std::unique_ptr<views::View> CreateAccountRow(
      const content::IdentityRequestAccount& account,
      bool should_hover);

  // Updates the image for the account in the corresponding ImageView.
  void OnAccountImageFetched(views::ImageView* image_view,
                             const gfx::Image& image,
                             const image_fetcher::RequestMetadata& metadata);

  // Called when the user clicks on the privacy policy or terms of service URL.
  // Opens the URL in a new tab.
  void OnLinkClicked(const GURL& gurl);

  // Called when the user selects an account from the multiple account chooser
  // menu. Modifies the UI to show one of the following:
  // 1. For new users, the single account chooser.
  // 2. For returning users, fetch the ID token automatically while displaying
  // "Signing you in".
  void OnSingleAccountPicked();

  // Called when the user clicks on the button from the single account chooser.
  void OnAccountSelected();

  // The ImageFetcher used to fetch the account pictures for FedCM.
  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher_;

  // Used in various messages so the user is aware of who the IDP is.
  std::u16string idp_etld_plus_one_;

  // Used in single account chooser to determine the look of the consent button.
  absl::optional<SkColor> brand_text_color_;
  absl::optional<SkColor> brand_background_color_;

  // The TabStripModel of the current browser. We need this in order to show the
  // privacy policy and terms of service urls when the user clicks on the links.
  const raw_ptr<TabStripModel> tab_strip_model_;

  // Used to ensure that callbacks are not run if the AccountSelectionBubbleView
  // is destroyed.
  base::WeakPtrFactory<AccountSelectionBubbleView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_BUBBLE_VIEW_H_
