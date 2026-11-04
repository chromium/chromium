// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_REMOTE_ACTOR_SELECTION_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_REMOTE_ACTOR_SELECTION_DIALOG_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/passwords/password_combined_selector_controller.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/remote_actor/remote_actor_credential_sharing_policy.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

class AccountChooserPrompt;

namespace password_manager {

// Controller for the PasswordCombinedSelectorView when used in the Remote Actor
// credential sharing flow.
class RemoteActorSelectionDialogController
    : public PasswordCombinedSelectorController {
 public:
  using OnResultCallback = base::OnceCallback<void(
      std::optional<password_manager::PasswordForm> selected_form)>;

  RemoteActorSelectionDialogController(content::WebContents* web_contents,
                                       FormsVector local_credentials,
                                       const std::string& credential_domain,
                                       OnResultCallback callback);

  RemoteActorSelectionDialogController(
      const RemoteActorSelectionDialogController&) = delete;
  RemoteActorSelectionDialogController& operator=(
      const RemoteActorSelectionDialogController&) = delete;

  ~RemoteActorSelectionDialogController() override;

  // Show the dialog.
  virtual void Show();

  // Setters below must be called before `Show()`.
  void SetActorType(RemoteActorType actor_type);
  void SetActorName(std::u16string actor_name);
  void SetLearnMoreUrl(GURL learn_more_url);
  void SetActorIconUrl(GURL actor_icon_url);

  // Returns the configured learn-more URL, or the default help URL if unset.
  GURL GetLearnMoreUrl() const;

  // PasswordCombinedSelectorController:
  DisplayType GetDisplayType() const override;
  bool ShouldShowTopIllustration() const override;
  std::optional<RemoteActorType> GetActorType() const override;
  GURL GetActorIconUrl() const override;
  std::u16string GetTitle() const override;
  std::u16string GetSubtitle() const override;
  gfx::Range GetSubtitleLinkRange() const override;
  void OnSubtitleLinkClicked() override;
  std::u16string GetOkButtonLabel() const override;
  const FormsVector& GetLocalForms() const override;
  void OnChooseCredentials(
      const password_manager::PasswordForm& password_form,
      password_manager::CredentialType credential_type) override;
  void OnCloseDialog() override;
  bool IsShowingAccountChooser() const override;

 private:
  void DestroyDialog();

  // The WebContents of the tab showing the dialog.
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  // The credentials available for the user to select.
  FormsVector local_credentials_;
  // The domain of the site for which credentials are being requested.
  std::string credential_domain_;
  // Cached range of the link inside the subtitle text.
  mutable gfx::Range subtitle_link_range_;
  // Callback to run when the user selects a credential or closes the dialog.
  OnResultCallback callback_;
  // The remote actor type for which credentials are being requested.
  RemoteActorType actor_type_ = RemoteActorType::kGemini;
  // Optional display name override for the remote actor in the dialog.
  std::u16string actor_name_;
  // Optional "Learn more" URL override for the remote actor in the dialog.
  GURL learn_more_url_;
  // Optional URL for a custom remote actor icon in the dialog header.
  GURL actor_icon_url_;

  // The controller owns the view, which in turn owns the widget.
  std::unique_ptr<AccountChooserPrompt> view_;
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_UI_PASSWORDS_REMOTE_ACTOR_SELECTION_DIALOG_CONTROLLER_H_
