// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_BUBBLE_VIEW_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/webid/account_selection_view_base.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "content/public/browser/identity_request_account.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"

using IdentityProviderDataPtr = scoped_refptr<content::IdentityProviderData>;
using IdentityRequestAccountPtr =
    scoped_refptr<content::IdentityRequestAccount>;
using TokenError = content::IdentityCredentialTokenError;

namespace views {
class Checkbox;
class ImageButton;
class Label;
class MdTextButton;
}  // namespace views

// Bubble dialog that is used in the FedCM flow. It creates a dialog with an
// account chooser for the user, and it changes the content of that dialog as
// user moves through the FedCM flow steps.
class AccountSelectionBubbleView : public views::BubbleDialogDelegateView,
                                   public AccountSelectionViewBase {
  METADATA_HEADER(AccountSelectionBubbleView, views::BubbleDialogDelegateView)

 public:
  AccountSelectionBubbleView(
      const std::u16string& rp_for_display,
      const std::optional<std::u16string>& idp_title,
      blink::mojom::RpContext rp_context,
      content::WebContents* web_contents,
      views::View* anchor_view,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AccountSelectionViewBase::Observer* observer,
      views::WidgetObserver* widget_observer);
  ~AccountSelectionBubbleView() override;

  // AccountSelectionViewBase:
  void InitDialogWidget() override;

  void ShowMultiAccountPicker(
      const std::vector<IdentityRequestAccountPtr>& accounts,
      const std::vector<IdentityProviderDataPtr>& idp_list,
      bool show_back_button,
      bool is_choose_an_account) override;
  void ShowVerifyingSheet(const content::IdentityRequestAccount& account,
                          const std::u16string& title) override;

  void ShowSingleAccountConfirmDialog(
      const content::IdentityRequestAccount& account,
      bool show_back_button) override;

  void ShowFailureDialog(
      const std::u16string& idp_for_display,
      const content::IdentityProviderMetadata& idp_metadata) override;

  void ShowErrorDialog(const std::u16string& idp_for_display,
                       const content::IdentityProviderMetadata& idp_metadata,
                       const std::optional<TokenError>& error) override;

  void ShowRequestPermissionDialog(
      const content::IdentityRequestAccount& account,
      const content::IdentityProviderData& idp_data) override;

  void ShowSingleReturningAccountDialog(
      const std::vector<IdentityRequestAccountPtr>& accounts,
      const std::vector<IdentityProviderDataPtr>& idp_list) override;

  void ShowLoadingDialog() override;

  void CloseDialog() override;

  void UpdateDialogPosition() override;

  void OnAnchorBoundsChanged() override;

  std::string GetDialogTitle() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AccountSelectionBubbleViewTest,
                           WebContentsLargeEnoughToFitDialog);

  // views::BubbleDialogDelegateView:
  gfx::Rect GetBubbleBounds() override;

  // Returns a View containing the logo of the identity provider. Creates the
  // `header_icon_view_` if `has_idp_icon` is true.
  std::unique_ptr<views::View> CreateHeaderView(bool has_idp_icon);

  // Returns a View for single account chooser. It contains the account
  // information, disclosure text and a button for the user to confirm the
  // selection.
  std::unique_ptr<views::View> CreateSingleAccountChooser(
      const content::IdentityRequestAccount& account);

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

  // Returns a View containing a single returning account as well as a button to
  // 'choose an account' which will show all accounts and IDPs that are
  // available.
  std::unique_ptr<views::View> CreateSingleReturningAccountChooser(
      const std::vector<IdentityRequestAccountPtr>& accounts,
      const std::vector<IdentityProviderDataPtr>& idp_list);

  // Returns a view containing a button for the user to login to an IDP for
  // which there was a login status mismatch, to be used in the multiple account
  // chooser case.
  std::unique_ptr<views::View> CreateIdpLoginRow(
      const std::u16string& idp_for_display,
      const content::IdentityProviderMetadata& idp_metadata);

  // Creates the "Use other account" button.
  std::unique_ptr<views::View> CreateUseOtherAccountButton(
      const content::IdentityProviderMetadata& idp_metadata);

  // Updates the header title, the header icon visibility and the header back
  // button visibiltiy. `idp_metadata` is not null when we need to set a header
  // image based on the IDP.
  void UpdateHeader(const content::IdentityProviderMetadata& idp_metadata,
                    const std::u16string& title,
                    bool show_back_button);

  // Removes all children except for `header_view_`.
  void RemoveNonHeaderChildViews();

  // Creates the "Choose an account" button, showing some IDP domains as well.
  // Prioritizes showing any IDPs for which there was a login status mismatch.
  std::unique_ptr<views::View> CreateChooseAnAccountButton(
      const std::vector<std::u16string>& mismatch_idps,
      const std::vector<std::u16string>& non_mismatch_idps);

  // The current title for the dialog.
  std::u16string title_;

  // The relying party context to show in the title.
  blink::mojom::RpContext rp_context_;

  // Whether the dialog has been populated via either ShowMultiAccountPicker()
  // or ShowVerifyingSheet().
  bool has_sheet_{false};

  // View containing the logo of the identity provider and the title.
  raw_ptr<views::View> header_view_ = nullptr;

  // View containing the header IDP icon, if one needs to be used.
  raw_ptr<BrandIconImageView> header_icon_view_ = nullptr;

  // View containing the back button.
  raw_ptr<views::ImageButton> back_button_ = nullptr;

  // View containing the bubble title.
  raw_ptr<views::Label> title_label_ = nullptr;

  // View containing the continue button.
  raw_ptr<views::MdTextButton> continue_button_ = nullptr;

  // Auto re-authn opt-out checkbox.
  raw_ptr<views::Checkbox> auto_reauthn_checkbox_ = nullptr;

  // Whether to show the auto re-authn opt-out checkbox;
  bool show_auto_reauthn_checkbox_{false};

  // Used to ensure that callbacks are not run if the AccountSelectionBubbleView
  // is destroyed.
  base::WeakPtrFactory<AccountSelectionBubbleView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_BUBBLE_VIEW_H_
