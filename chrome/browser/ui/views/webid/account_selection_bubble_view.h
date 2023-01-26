// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_BUBBLE_VIEW_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/webid/account_selection_bubble_view_interface.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "content/public/browser/identity_request_account.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class ImageView;
class Label;
class MdTextButton;
}  // namespace views

namespace {
class IdpImageView;
}  // namespace

// Bubble dialog that is used in the FedCM flow. It creates a dialog with an
// account chooser for the user, and it changes the content of that dialog as
// user moves through the FedCM flow steps.
class AccountSelectionBubbleView : public views::BubbleDialogDelegateView,
                                   public AccountSelectionBubbleViewInterface {
 public:
  // Used to observe changes to the account selection bubble.
  class Observer {
   public:
    enum class LinkType { PRIVACY_POLICY, TERMS_OF_SERVICE };

    // Called when:
    // 1. A user either selects the account from the multi-account chooser or
    // clicks the "continue" button. (auto_signin == false)
    // 2. Auto sign-in is triggered. (auto_signin == true)
    // Takes `account` as well as `idp_display_data` since passing `account_id`
    // is insufficient in the multiple IDP case. The caller should pass a cref,
    // as these objects are owned by the observer.
    virtual void OnAccountSelected(
        const content::IdentityRequestAccount& account,
        const IdentityProviderDisplayData& idp_display_data,
        bool auto_signin,
        const ui::Event& event) = 0;

    // Called when the user clicks "privacy policy" or "terms of service" link.
    virtual void OnLinkClicked(LinkType link_type,
                               const GURL& url,
                               const ui::Event& event) = 0;

    // Called when the user clicks "back" button.
    virtual void OnBackButtonClicked() = 0;

    // Called when the user clicks "close" button.
    virtual void OnCloseButtonClicked(const ui::Event& event) = 0;
  };

  METADATA_HEADER(AccountSelectionBubbleView);
  AccountSelectionBubbleView(
      const std::u16string& rp_for_display,
      const absl::optional<std::u16string>& idp_title,
      blink::mojom::RpContext rp_context,
      views::View* anchor_view,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Observer* observer);
  ~AccountSelectionBubbleView() override;

  // AccountSelectionBubbleViewInterface:
  void ShowAccountPicker(
      const std::vector<IdentityProviderDisplayData>& idp_display_data_list,
      bool show_back_button) override;
  void ShowVerifyingSheet(const content::IdentityRequestAccount& account,
                          const IdentityProviderDisplayData& idp_display_data,
                          const std::u16string& title) override;

  void ShowSingleAccountConfirmDialog(
      const std::u16string& rp_for_display,
      const content::IdentityRequestAccount& account,
      const IdentityProviderDisplayData& idp_display_data) override;

  void ShowFailureDialog(const std::u16string& rp_for_display,
                         const std::u16string& idp_for_display) override;

  // Populates `idp_images` when an IDP image has been fetched.
  void AddIdpImage(const GURL& image_url, gfx::ImageSkia idp_image);

 private:
  gfx::Rect GetBubbleBounds() override;

  // Returns a View containing the logo of the identity provider and the title
  // of the bubble, properly formatted. Creates the `header_icon_view_` if
  // `has_idp_icon` is true.
  std::unique_ptr<views::View> CreateHeaderView(const std::u16string& title,
                                                bool has_idp_icon);

  // Returns a View containing the account chooser, i.e. everything that goes
  // below the horizontal separator on the initial FedCM bubble.
  std::unique_ptr<views::View> CreateAccountChooser(
      const std::vector<IdentityProviderDisplayData>& idp_display_data_list);

  // Returns a View for single account chooser. It contains the account
  // information, disclosure text and a button for the user to confirm the
  // selection. The size of the `idp_display_data.accounts` vector must be 1.
  std::unique_ptr<views::View> CreateSingleAccountChooser(
      const IdentityProviderDisplayData& idp_display_data,
      const content::IdentityRequestAccount& account);

  // Returns a View for multiple account chooser. It contains the info for each
  // account in a button, so the user can pick an account.
  std::unique_ptr<views::View> CreateMultipleAccountChooser(
      const std::vector<IdentityProviderDisplayData>& idp_display_data_list);

  // Creates a row containing the IDP icon as well as the IDP ETLD+1. Used in
  // the multi IDP scenario, when the user is selecting from multiple accounts.
  std::unique_ptr<views::View> CreateIdpHeaderRowForMultiIdp(
      const std::u16string& idp_for_display,
      const content::IdentityProviderMetadata& idp_metadata);

  // Returns a View containing information about an account: the picture for
  // the account on the left, and information about the account on the right.
  // |should_hover| determines whether the account row is a HoverButton or
  // not.
  std::unique_ptr<views::View> CreateAccountRow(
      const content::IdentityRequestAccount& account,
      const IdentityProviderDisplayData& idp_display_data,
      bool should_hover);

  // Updates the header title, the header icon visibility and the header back
  // button visibiltiy. `idp_metadata` is not null when we need to set a header
  // image based on the IDP.
  void UpdateHeader(const content::IdentityProviderMetadata& idp_metadata,
                    const std::u16string subpage_title,
                    bool show_back_button);

  // Sets the brand views::ImageView visibility and image. Initiates the
  // download of the brand icon if necessary.
  void ConfigureIdpBrandImageView(
      IdpImageView* image_view,
      const content::IdentityProviderMetadata& idp_metadata);

  // Removes all children except for `header_view_`.
  void RemoveNonHeaderChildViews();

  // The ImageFetcher used to fetch the account pictures for FedCM.
  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher_;

  // The accessible title.
  std::u16string accessible_title_;

  blink::mojom::RpContext rp_context_;

  // The images for the IDP icons. Stored so that they can be reused upon
  // pressing the back button after choosing an account on the multi IDP
  // chooser.
  base::flat_map<GURL, gfx::ImageSkia> idp_images_;

  // Whether the dialog has been populated via either ShowAccountPicker() or
  // ShowVerifyingSheet().
  bool has_sheet_{false};

  // View containing the logo of the identity provider and the title.
  raw_ptr<views::View> header_view_ = nullptr;

  // View containing the header IDP icon, if one needs to be used.
  raw_ptr<IdpImageView> header_icon_view_ = nullptr;

  // View containing the back button.
  raw_ptr<views::ImageButton> back_button_ = nullptr;

  // View containing the bubble title.
  raw_ptr<views::Label> title_label_ = nullptr;

  // View containing the continue button.
  raw_ptr<views::MdTextButton> continue_button_ = nullptr;

  // Observes events on AccountSelectionBubbleView.
  raw_ptr<Observer> observer_{nullptr};

  // Used to ensure that callbacks are not run if the AccountSelectionBubbleView
  // is destroyed.
  base::WeakPtrFactory<AccountSelectionBubbleView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_BUBBLE_VIEW_H_
