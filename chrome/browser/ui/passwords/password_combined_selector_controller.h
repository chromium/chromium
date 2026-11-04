// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_COMBINED_SELECTOR_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_COMBINED_SELECTOR_CONTROLLER_H_

#include <memory>
#include <optional>
#include <vector>

#include "chrome/browser/ui/passwords/password_base_dialog_controller.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/remote_actor/remote_actor_credential_sharing_policy.h"
#include "ui/gfx/range/range.h"
#include "url/origin.h"

class GURL;

namespace password_manager {
struct PasswordForm;
}

// Base controller interface for PasswordCombinedSelectorView.
class PasswordCombinedSelectorController : public PasswordBaseDialogController {
 public:
  enum class DisplayType {
    kCredentialManager,
    // Specialized dialog for allowing a remote actor (e.g. an AI agent) to
    // sign in.
    kRemoteActor,
  };

  using FormsVector =
      std::vector<std::unique_ptr<password_manager::PasswordForm>>;

  PasswordCombinedSelectorController() = default;
  ~PasswordCombinedSelectorController() override = default;

  virtual DisplayType GetDisplayType() const = 0;
  virtual bool ShouldShowTopIllustration() const = 0;
  // Returns the remote actor type for selecting the default header icon, or
  // `std::nullopt` if this dialog is not for a remote actor.
  virtual std::optional<password_manager::RemoteActorType> GetActorType()
      const = 0;
  // Returns the custom icon URL for the remote actor header illustration, or
  // an empty GURL to use the default icon.
  virtual GURL GetActorIconUrl() const = 0;

  virtual std::u16string GetTitle() const = 0;
  virtual std::u16string GetSubtitle() const = 0;
  virtual gfx::Range GetSubtitleLinkRange() const = 0;
  virtual void OnSubtitleLinkClicked() = 0;
  virtual std::u16string GetOkButtonLabel() const = 0;

  virtual const FormsVector& GetLocalForms() const = 0;
  virtual void OnChooseCredentials(
      const password_manager::PasswordForm& password_form,
      password_manager::CredentialType credential_type) = 0;
  virtual void OnCloseDialog() = 0;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_COMBINED_SELECTOR_CONTROLLER_H_
