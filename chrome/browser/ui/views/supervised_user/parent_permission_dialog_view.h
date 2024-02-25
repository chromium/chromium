// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SUPERVISED_USER_PARENT_PERMISSION_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SUPERVISED_USER_PARENT_PERMISSION_DIALOG_VIEW_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/install_prompt_permissions.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

class GaiaAuthFetcher;

namespace extensions {
class Extension;
}  // namespace extensions

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
class IdentityManager;
}  // namespace signin

namespace views {
class Label;
}

class ParentPermissionInputSection;

// Modal dialog that shows a dialog that prompts a parent for permission by
// asking them to enter their google account credentials.  This is created only
// when the dialog is ready to be shown (after the state has been
// asynchronously fetched).
class ParentPermissionDialogView : public views::DialogDelegateView,
                                   public GaiaAuthConsumer {
  METADATA_HEADER(ParentPermissionDialogView, views::DialogDelegateView)

 public:
  class Observer {
   public:
    // Tells observers that their references to the view are becoming invalid.
    virtual void OnParentPermissionDialogViewDestroyed() = 0;
  };
  struct Params;

  ParentPermissionDialogView(std::unique_ptr<Params> params,
                             Observer* observer);
  ParentPermissionDialogView(const ParentPermissionDialogView&) = delete;
  ParentPermissionDialogView& operator=(const ParentPermissionDialogView&) =
      delete;
  ~ParentPermissionDialogView() override;

  // Closes the dialog.
  void CloseDialog();

  // Shows the parent permission dialog.
  void ShowDialog();

  // Removes the observer reference.
  void RemoveObserver();

  void SetSelectedParentPermissionEmail(const std::u16string& email_address);
  std::u16string GetSelectedParentPermissionEmail() const;

  void SetParentPermissionCredential(const std::u16string& credential);
  std::u16string GetParentPermissionCredential() const;

  bool GetInvalidCredentialReceived() const;

  void SetIdentityManagerForTesting(signin::IdentityManager* identity_manager);

  void SetRepromptAfterIncorrectCredential(bool reprompt);
  bool GetRepromptAfterIncorrectCredential() const;

 private:
  // views::View:
  void AddedToWidget() override;
  void OnThemeChanged() override;

  // views::DialogDelegate:
  bool Cancel() override;
  bool Accept() override;

  // views::WidgetDelegate:
  std::u16string GetAccessibleWindowTitle() const override;

  // Changes the widget size to accommodate the contents' preferred size.
  void ResizeWidget();

  // Creates the contents area that contains permissions and other extension
  // info.
  void CreateContents();

  void ShowDialogInternal();

  void AddInvalidCredentialLabel();
  void LoadParentEmailAddresses();
  void CloseWithReason(views::Widget::ClosedReason reason);

  // Given an email address of the child's parent, return the parents'
  // obfuscated gaia id.
  std::string GetParentObfuscatedGaiaID(
      const std::u16string& parent_email) const;

  // Starts the Reauth-scoped OAuth access token fetch process.
  void StartReauthAccessTokenFetch(const std::string& parent_obfuscated_gaia_id,
                                   const std::string& parent_credential);

  // Handles the result of the access token
  void OnAccessTokenFetchComplete(const std::string& parent_obfuscated_gaia_id,
                                  const std::string& parent_credential,
                                  GoogleServiceAuthError error,
                                  signin::AccessTokenInfo access_token_info);

  // Starts the Parent Reauth proof token fetch process.
  void StartParentReauthProofTokenFetch(
      const std::string& child_access_token,
      const std::string& parent_obfuscated_gaia_id,
      const std::string& credential);

  // GaiaAuthConsumer
  void OnReAuthProofTokenSuccess(
      const std::string& reauth_proof_token) override;
  void OnReAuthProofTokenFailure(
      const GaiaAuthConsumer::ReAuthProofTokenStatus error) override;

  // The first time it is called, logs the result to UMA and passes it to the
  // callback. No effect if called subsequent times.
  void SendResultOnce(ParentPermissionDialog::Result result);

  // Sets the |extension| to be optionally displayed in the dialog.  This
  // causes the view to show several extension properties including the
  // permissions and the extension name.
  void InitializeExtensionData(
      scoped_refptr<const extensions::Extension> extension);

  // Permissions ot be displayed in the prompt. Only populated
  // if an extension has been set.
  extensions::InstallPromptPermissions prompt_permissions_;

  // The email address of the parents to display in the dialog.
  std::vector<std::u16string> parent_permission_email_addresses_;

  bool reprompt_after_incorrect_credential_ = true;

  // Contains the parent-permission-input related views widgets.
  std::unique_ptr<ParentPermissionInputSection>
      parent_permission_input_section_;

  raw_ptr<views::Label> invalid_credential_label_ = nullptr;

  bool invalid_credential_received_ = false;

  // The currently selected parent email.
  std::u16string selected_parent_permission_email_;

  // The currently entered parent credential.
  std::u16string parent_permission_credential_;

  // Parameters for the dialog.
  std::unique_ptr<Params> params_;

  // Used to ensure we don't try to show same dialog twice.
  bool is_showing_ = false;

  // Used to fetch the Reauth token.
  std::unique_ptr<GaiaAuthFetcher> reauth_token_fetcher_;

  // Used to fetch OAuth2 access tokens.
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  std::unique_ptr<signin::AccessTokenFetcher> oauth2_access_token_fetcher_;

  raw_ptr<Observer> observer_;

  SupervisedUserExtensionsMetricsRecorder supervised_user_metrics_recorder_;

  base::WeakPtrFactory<ParentPermissionDialogView> weak_factory_{this};
};

// Allows tests to observe the create of the testing instance of
// ParentPermissionDialogView
class TestParentPermissionDialogViewObserver {
 public:
  // Implementers should pass "this" as constructor argument.
  TestParentPermissionDialogViewObserver(
      TestParentPermissionDialogViewObserver* observer);
  ~TestParentPermissionDialogViewObserver();
  virtual void OnTestParentPermissionDialogViewCreated(
      ParentPermissionDialogView* view) = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SUPERVISED_USER_PARENT_PERMISSION_DIALOG_VIEW_H_
