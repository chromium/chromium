// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_LOGIN_PERMISSION_REQUEST_H_
#define CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_LOGIN_PERMISSION_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "components/permissions/permission_request.h"
#include "url/gurl.h"

namespace ambient_signin {

// A permission request for displaying ambient login options (e.g. WebAuthn
// passkeys/passwords or FedCM sign-in) via PermissionRequestManager.
class AmbientLoginPermissionRequest : public permissions::PermissionRequest {
 public:
  AmbientLoginPermissionRequest(
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      PermissionDecidedCallback permission_decided_callback,
      const std::u16string& username,
      const std::u16string& provider_name);
  ~AmbientLoginPermissionRequest() override;

  const std::u16string& username() const { return username_; }
  const std::u16string& provider_name() const { return provider_name_; }

  // permissions::PermissionRequest:
  std::u16string GetMessageTextFragment() const override;

 private:
  std::u16string username_;
  std::u16string provider_name_;
};

}  // namespace ambient_signin

#endif  // CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_LOGIN_PERMISSION_REQUEST_H_
