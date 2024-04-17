// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_MODAL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_MODAL_VIEW_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/webid/account_selection_view_base.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "content/public/browser/identity_request_account.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/window/dialog_delegate.h"

class AccountSelectionModalView : public views::DialogDelegateView,
                                  public AccountSelectionViewBase {
  METADATA_HEADER(AccountSelectionModalView, views::DialogDelegateView)

 public:
  AccountSelectionModalView(
      const std::u16string& top_frame_for_display,
      const std::optional<std::u16string>& idp_title,
      blink::mojom::RpContext rp_context,
      content::WebContents* web_contents,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AccountSelectionViewBase::Observer* observer,
      views::WidgetObserver* widget_observer);
  ~AccountSelectionModalView() override;
  AccountSelectionModalView(const AccountSelectionModalView&) = delete;
  AccountSelectionModalView& operator=(const AccountSelectionModalView&) =
      delete;

  // AccountSelectionViewBase:
  void InitDialogWidget() override;

  void ShowMultiAccountPicker(
      const std::vector<IdentityProviderDisplayData>& idp_display_data_list,
      bool show_back_button) override;

  void ShowVerifyingSheet(const content::IdentityRequestAccount& account,
                          const IdentityProviderDisplayData& idp_display_data,
                          const std::u16string& title) override;

  void ShowSingleAccountConfirmDialog(
      const std::u16string& top_frame_for_display,
      const std::optional<std::u16string>& iframe_for_display,
      const content::IdentityRequestAccount& account,
      const IdentityProviderDisplayData& idp_display_data,
      bool show_back_button) override;

  void ShowFailureDialog(
      const std::u16string& top_frame_for_display,
      const std::optional<std::u16string>& iframe_for_display,
      const std::u16string& idp_for_display,
      const content::IdentityProviderMetadata& idp_metadata) override;

  void ShowErrorDialog(
      const std::u16string& top_frame_for_display,
      const std::optional<std::u16string>& iframe_for_display,
      const std::u16string& idp_for_display,
      const content::IdentityProviderMetadata& idp_metadata,
      const std::optional<content::IdentityCredentialTokenError>& error)
      override;

  void ShowRequestPermissionDialog(
      const std::u16string& top_frame_for_display,
      const content::IdentityRequestAccount& account,
      const IdentityProviderDisplayData& idp_display_data) override;

  void ShowSingleReturningAccountDialog(
      const std::vector<IdentityProviderDisplayData>& idp_data_list) override;

  void ShowLoadingDialog() override;

  void CloseDialog() override;

  std::string GetDialogTitle() const override;
  std::optional<std::string> GetDialogSubtitle() const override;

 private:
  // Returns a View for header of an account chooser. It contains text to prompt
  // the user to sign in to an RP with an account from an IDP.
  std::unique_ptr<views::View> CreateAccountChooserHeader(
      const content::IdentityProviderMetadata& idp_metadata);

  // Returns a View for single account chooser. It contains a row of account
  // information. `The size of the `idp_display_data.accounts` vector must be 1.
  // `should_hover` determines whether the row is clickable.
  // `show_disclosure_label` determines whether disclosure text is shown.
  std::unique_ptr<views::View> CreateSingleAccountChooser(
      const IdentityProviderDisplayData& idp_display_data,
      const content::IdentityRequestAccount& account,
      bool should_hover,
      bool show_disclosure_label,
      bool show_separator,
      int additional_row_vertical_padding);

  // Returns a View for multiple account chooser. It contains the info for each
  // account in a button, so the user can pick an account.
  std::unique_ptr<views::View> CreateMultipleAccountChooser(
      const std::vector<IdentityProviderDisplayData>& idp_display_data_list);

  // Returns a View for an account row that acts as a placeholder.
  std::unique_ptr<views::View> CreatePlaceholderAccountRow();

  // Returns a View for a row of custom buttons. A cancel button is always
  // shown. A continue button, use other account button and/or back button is
  // shown if `continue_callback`, `use_other_account_callback` and/or
  // `back_callback` is specified respectively. If `use_other_account_callback`
  // is specified, `back_callback` should NOT be specified and vice versa.
  std::unique_ptr<views::View> CreateButtonRow(
      std::optional<views::Button::PressedCallback> continue_callback,
      std::optional<views::Button::PressedCallback> use_other_account_callback,
      std::optional<views::Button::PressedCallback> back_callback);

  // Returns a View containing the image of the icon fetched from
  // `brand_icon_url`. If the image cannot be fetched, a globe icon is returned
  // instead.
  std::unique_ptr<views::View> CreateBrandIconImageView(
      const GURL& brand_icon_url);

  // Adds a progress bar at the top of the modal dialog.
  void AddProgressBar();

  // Resizes the modal dialog to the size of its contents.
  void UpdateModalPositionAndTitle();

  // Removes all child views and dangling pointers.
  void RemoveNonHeaderChildViews();

  // Opens the use other account pop-up and disables the use other account
  // button.
  void OnUseOtherAccount(const GURL& idp_config_url,
                         const GURL& idp_login_url,
                         const ui::Event& event);

  // View containing the header.
  raw_ptr<views::View> header_view_ = nullptr;

  // View containing the use other account button.
  raw_ptr<views::View> use_other_account_button_ = nullptr;

  // View containing the back button.
  raw_ptr<views::View> back_button_ = nullptr;

  // View containing the continue button.
  raw_ptr<views::View> continue_button_ = nullptr;

  // View containing the account chooser.
  raw_ptr<views::View> account_chooser_ = nullptr;

  // View containing the title.
  raw_ptr<views::Label> title_label_ = nullptr;

  // View containing the body.
  raw_ptr<views::Label> body_label_ = nullptr;

  // View containing the brand icon image.
  raw_ptr<BrandIconImageView> brand_icon_ = nullptr;

  // Whether a progress bar is present.
  bool has_progress_bar_{false};

  // The title for the modal dialog.
  std::u16string title_;

  // Used to ensure that callbacks are not run if the AccountSelectionModalView
  // is destroyed.
  base::WeakPtrFactory<AccountSelectionModalView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_MODAL_VIEW_H_
