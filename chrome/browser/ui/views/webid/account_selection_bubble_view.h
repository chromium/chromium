// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_BUBBLE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/webid/account_selection_view_base.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "content/public/browser/webid/identity_request_account.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class Label;
class Widget;
}  // namespace views

namespace webid {

class AccountSelectionBubbleView;

class AccountSelectionBubbleDelegate : public views::BubbleDialogDelegate {
 public:
  AccountSelectionBubbleDelegate(
      std::unique_ptr<AccountSelectionBubbleView> account_selection_bubble_view,
      views::View* anchor_view);
  ~AccountSelectionBubbleDelegate() override;

  AccountSelectionBubbleDelegate(const AccountSelectionBubbleDelegate&) =
      delete;
  AccountSelectionBubbleDelegate& operator=(
      const AccountSelectionBubbleDelegate&) = delete;

  // views::BubbleDialogDelegate overrides:
  gfx::Rect GetBubbleBounds() override;

  // TODO (kylixrd): Investigate removal of these overrides.
  // views::WidgetDelegate overrides:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

 private:
  AccountSelectionBubbleView* GetAccountSelectionView();
};

// Bubble dialog that is used in the FedCM flow. It creates a dialog with an
// account chooser for the user, and it changes the content of that dialog as
// user moves through the FedCM flow steps.
class AccountSelectionBubbleView : public views::BoxLayoutView,
                                   public AccountSelectionViewBase {
  METADATA_HEADER(AccountSelectionBubbleView, views::BoxLayoutView)

 public:
  AccountSelectionBubbleView(
      const content::RelyingPartyData& rp_data,
      const std::optional<std::u16string>& idp_title,
      blink::mojom::RpContext rp_context,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      FedCmAccountSelectionView* owner);
  ~AccountSelectionBubbleView() override;

  // AccountSelectionViewBase:
  void ShowMultiAccountPicker(
      const std::vector<IdentityRequestAccountPtr>& accounts,
      const std::vector<IdentityProviderDataPtr>& idp_list,
      const gfx::Image& rp_icon,
      bool show_back_button) override;
  void ShowVerifyingSheet(const IdentityRequestAccountPtr& account,
                          const std::u16string& title) override;

  void ShowSingleAccountConfirmDialog(const IdentityRequestAccountPtr& account,
                                      bool show_back_button) override;

  void ShowFailureDialog(
      const std::u16string& idp_for_display,
      const content::IdentityProviderMetadata& idp_metadata) override;

  void ShowErrorDialog(const std::u16string& idp_for_display,
                       const content::IdentityProviderMetadata& idp_metadata,
                       const std::optional<TokenError>& error) override;

  void ShowRequestPermissionDialog(
      const IdentityRequestAccountPtr& account) override;

  std::string GetDialogTitle() const override;
  std::optional<std::string> GetDialogSubtitle() const override;

  std::u16string dialog_title() const { return title_; }

  gfx::Rect GetBubbleBounds(gfx::Rect proposed_bubble_bounds);

 private:
  FRIEND_TEST_ALL_PREFIXES(AccountSelectionBubbleViewTest,
                           WebContentsLargeEnoughToFitDialog);

  // Returns a View containing the logo of the identity provider.
  std::unique_ptr<views::View> CreateHeaderView();

  // Returns a pair <View, Button> where the first element is the View for
  // single account chooser. This View contains the account information,
  // disclosure text and a button for the user to confirm the selection. The
  // second element is the button for the user to confirm the selection.
  std::pair<std::unique_ptr<views::View>, views::MdTextButton*>
  CreateSingleAccountChooser(const IdentityRequestAccountPtr& account);

  // Adds a separator as well as a multiple account chooser. The chooser
  // contains the info for each account in a button, so the user can pick an
  // account. It also contains mismatch login URLs in the multiple IDP case.
  void AddSeparatorAndMultipleAccountChooser(
      const std::vector<IdentityRequestAccountPtr>& accounts,
      const std::vector<IdentityProviderDataPtr>& idp_list);

  // Adds the accounts provided to the given view. This method does not reorder
  // the accounts, and assumes they are provided in the correct order.
  void AddAccounts(const std::vector<IdentityRequestAccountPtr>& accounts,
                   views::View* accounts_content,
                   bool is_multi_idp);

  // Invoked whenever the expandable account chooser is scrolled.
  void OnExpandableAccountsScrolled();

  // Returns a view containing a button for the user to login to an IDP for
  // which there was a login status mismatch, to be used in the multiple account
  // chooser case.
  std::unique_ptr<views::View> CreateMultiIdpLoginRow(
      const std::u16string& idp_for_display,
      const IdentityProviderDataPtr& idp_data);

  // Creates the "Use other account" button when showing a dialog with one IDP.
  std::unique_ptr<views::View> CreateSingleIdpUseOtherAccountButton(
      const content::IdentityProviderMetadata& idp_metadata,
      const std::u16string& title,
      int icon_margin);

  // Updates the header title, the header icon visibility and the header back
  // button visibility. `idp_image` is not empty when we need to set a header
  // image based on the IDP. `should_circle_crop_header_icon` determines whether
  // the icon passed should be cropped or not. Some icons like the RP icon are
  // not meant to be cropped, and some icons like the badged account icon are
  // cropped on the backend, so they should not be cropped here.
  void UpdateHeader(const gfx::Image& idp_image,
                    const std::u16string& title,
                    const std::u16string& subtitle,
                    bool show_back_button,
                    bool should_circle_crop_header_icon);

  // Removes all children except for `header_view_`.
  void RemoveNonHeaderChildViews();

  // Creates the "Choose an account" button, showing some IDP domains as well.
  // Prioritizes showing any IDPs for which there was a login status mismatch.
  std::unique_ptr<views::View> CreateChooseAnAccountButton(
      const std::vector<std::u16string>& mismatch_idps,
      const std::vector<std::u16string>& non_mismatch_idps);

  // The current title for the dialog.
  std::u16string title_;

  // The current subtitle for the dialog.
  std::u16string subtitle_;

  // The relying party context to show in the title.
  blink::mojom::RpContext rp_context_;

  // View containing the logo of the identity provider and the title.
  raw_ptr<views::View> header_view_ = nullptr;

  // View containing the header IDP icon, if one needs to be used.
  raw_ptr<BrandIconImageView> header_icon_view_ = nullptr;

  // View containing the back button.
  raw_ptr<views::ImageButton> back_button_ = nullptr;

  // View containing the bubble title.
  raw_ptr<views::Label> title_label_ = nullptr;

  // View containing the bubble subtitle.
  raw_ptr<views::Label> subtitle_label_ = nullptr;

  // Used to ensure that callbacks are not run if the AccountSelectionBubbleView
  // is destroyed.
  base::WeakPtrFactory<AccountSelectionBubbleView> weak_ptr_factory_{this};
};

}  // namespace webid

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_BUBBLE_VIEW_H_
