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

  void ShowMultiAccountPicker(const std::vector<IdentityProviderDisplayData>&
                                  idp_display_data_list) override;

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

  void CloseDialog() override;

  std::string GetDialogTitle() const override;
  std::optional<std::string> GetDialogSubtitle() const override;

 private:
  // Returns a View for header of an account chooser. It contains text to prompt
  // the user to sign in to an RP with an account from an IDP.
  std::unique_ptr<views::View> CreateAccountChooserHeader(
      const content::IdentityProviderMetadata& idp_metadata);

  // Returns a View for single account chooser. It contains clickable account
  // information, and a button for the user to close the modal dialog. The size
  // of the `idp_display_data.accounts` vector must be 1.
  std::unique_ptr<views::View> CreateSingleAccountChooser(
      const IdentityProviderDisplayData& idp_display_data,
      const content::IdentityRequestAccount& account);

  // Returns a View for multiple account chooser. It contains the info for each
  // account in a button, so the user can pick an account.
  std::unique_ptr<views::View> CreateMultipleAccountChooser(
      const std::vector<IdentityProviderDisplayData>& idp_display_data_list);

  // View containing the modal dialog title.
  raw_ptr<views::Label> title_label_ = nullptr;

  // The title for the modal dialog.
  std::u16string title_;

  // Used to ensure that callbacks are not run if the AccountSelectionModalView
  // is destroyed.
  base::WeakPtrFactory<AccountSelectionModalView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_ACCOUNT_SELECTION_MODAL_VIEW_H_
