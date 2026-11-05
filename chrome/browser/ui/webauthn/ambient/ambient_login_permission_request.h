// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_LOGIN_PERMISSION_REQUEST_H_
#define CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_LOGIN_PERMISSION_REQUEST_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "components/permissions/permission_request.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ambient_signin {

enum class CredentialType {
  kPasskey,
  kPassword,
};

// Represents a WebAuthn passkey or saved password credential option.
struct PasskeyOrPasswordCredential {
  std::u16string username;
  std::u16string provider_name;
  CredentialType type = CredentialType::kPasskey;
  std::u16string display_name;
  // Icon of the provider that stores the credential, e.g. Google Password
  // Manager. The prompt shows it as a badge on the credential's monogram. If
  // null, the prompt uses the generic passkey or password icon instead.
  raw_ptr<const gfx::VectorIcon> provider_icon = nullptr;
};

// The size of account pictures in the prompt, in DIPs. Callers should crop
// pictures to this size so that they aren't scaled.
inline constexpr int kAccountPictureSize = 40;

// The size of the image in a badge on an account picture or a monogram, in
// DIPs.
inline constexpr int kBadgeImageSize = 16;

// Represents a federated sign-in credential option (e.g. from FedCM).
struct FederatedCredential {
  // The identity provider's domain formatted for display, e.g. "idp.example".
  // The prompt elides it at the start, so that the end of the domain stays
  // visible.
  std::u16string idp_name;
  std::u16string account_name;
  std::u16string email;
  GURL idp_origin;
  // Optional account picture. If empty, the prompt uses a default icon.
  //
  // The picture must come from the identity provider and be fetched by the
  // browser (as FedCM does with the picture URL from the IdP's accounts
  // endpoint), never from the requesting site.
  //
  // The caller crops the picture so that it is square and
  // `kAccountPictureSize` wide, e.g. FedCM crops it into a circle like in its
  // own UI.
  ui::ImageModel icon;
  // Optional badge on `icon`, e.g. the identity provider's icon. The prompt
  // doesn't show it without `icon`.
  //
  // Like `icon`, the badge must come from the identity provider and be fetched
  // by the browser, never from the requesting site.
  //
  // The caller crops the badge so that it is square and `kBadgeImageSize`
  // wide, e.g. FedCM crops the IdP's icon into a circle like in its own UI.
  ui::ImageModel badge;
  // Whether choosing this credential signs the user in without further UI,
  // e.g. without a dialog to review what is shared with the site. If so, the
  // chip shows that the user is signing in.
  bool signs_in_directly = false;
};

// A permission request for displaying ambient login options (e.g. WebAuthn
// passkeys/passwords or FedCM sign-in) via PermissionRequestManager.
class AmbientLoginPermissionRequest : public permissions::PermissionRequest {
 public:
  using PasskeyOrPasswordSelectedCallback =
      base::OnceCallback<void(size_t index)>;
  using FederatedCredentialSelectedCallback =
      base::OnceCallback<void(size_t index)>;

  // A set of optional callbacks and values configured at creation time.
  struct Options {
    // When present, if a user selected a credential then this is called after
    // the credential selection callback.
    PermissionDecidedCallback permission_decided_callback = base::DoNothing();
    // Whether the bubble opens automatically along with the chip. Otherwise,
    // only the chip is shown, and the bubble opens when the user clicks it.
    bool bubble_starts_open = true;
  };

  // Constructor supporting both WebAuthn/password credentials and FedCM
  // federated credentials.
  AmbientLoginPermissionRequest(
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      std::vector<PasskeyOrPasswordCredential> credentials,
      PasskeyOrPasswordSelectedCallback credential_selected_callback,
      std::vector<FederatedCredential> federated_credentials,
      FederatedCredentialSelectedCallback federated_selected_callback,
      Options options);

  // Convenience constructor for passkey/password-only requests.
  AmbientLoginPermissionRequest(
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      std::vector<PasskeyOrPasswordCredential> credentials,
      PasskeyOrPasswordSelectedCallback credential_selected_callback,
      Options options);

  ~AmbientLoginPermissionRequest() override;

  const std::vector<PasskeyOrPasswordCredential>& credentials() const {
    return credentials_;
  }
  const std::vector<FederatedCredential>& federated_credentials() const {
    return federated_credentials_;
  }

  // Records the chosen credential index to be passed to
  // `credential_selected_callback_` when the permission request is accepted.
  void SelectCredential(size_t index);

  // Records the chosen federated credential index to be passed to
  // `federated_selected_callback_` when the permission request is accepted.
  void SelectFederatedCredential(size_t index);

  // Returns the vector icon representing a credential of the given `type`.
  // Used both for credential rows in the bubble and for the omnibox chip.
  static const gfx::VectorIcon& GetCredentialIcon(CredentialType type);

  // permissions::PermissionRequest:
  permissions::IconId GetIconForChip() const override;
  permissions::IconId GetBlockedIconForChip() const override;
  bool ShouldKeepChipOnBubbleClose() const override;
  std::optional<std::u16string> GetRequestChipText(
      ChipTextType type) const override;
  std::u16string GetMessageTextFragment() const override;
  bool ShouldBubbleStartOpen() const override;

 private:
  void OnPermissionDecided(
      const permissions::PermissionPromptDecision& decision,
      const permissions::PermissionRequestData& request_data);

  std::vector<PasskeyOrPasswordCredential> credentials_;
  PasskeyOrPasswordSelectedCallback credential_selected_callback_;
  std::vector<FederatedCredential> federated_credentials_;
  FederatedCredentialSelectedCallback federated_selected_callback_;
  PermissionDecidedCallback caller_permission_decided_callback_;
  std::optional<size_t> selected_credential_index_;
  std::optional<size_t> selected_federated_index_;
  const bool bubble_starts_open_;
};

}  // namespace ambient_signin

#endif  // CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_LOGIN_PERMISSION_REQUEST_H_
