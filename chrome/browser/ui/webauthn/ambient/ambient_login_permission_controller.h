// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_LOGIN_PERMISSION_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_LOGIN_PERMISSION_CONTROLLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_request.h"
#include "content/public/browser/page_user_data.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace ambient_signin {

// Manages ambient login permission requests across WebAuthn and FedCM.
// Lifetime is scoped to content::PageUserData.
class AmbientLoginPermissionController
    : public content::PageUserData<AmbientLoginPermissionController> {
 public:
  enum class State {
    kIdle,
    kRequestAdded,
  };

  ~AmbientLoginPermissionController() override;

  // Requests displaying an ambient sign-in prompt via PermissionRequestManager.
  void RequestPermission(
      content::RenderFrameHost* requesting_frame,
      std::unique_ptr<AmbientLoginPermissionRequest> request);

  State state() const { return state_; }

  void SetFinishedNotificationForTesting(base::OnceClosure finished_closure);

  base::WeakPtr<AmbientLoginPermissionController> GetWeakPtr();

 private:
  explicit AmbientLoginPermissionController(content::Page& page);
  friend class content::PageUserData<AmbientLoginPermissionController>;
  PAGE_USER_DATA_KEY_DECL();

  void OnRequestFinished();

  State state_ = State::kIdle;
  base::OnceClosure finished_closure_;

  base::WeakPtrFactory<AmbientLoginPermissionController> weak_ptr_factory_{
      this};
};

}  // namespace ambient_signin

#endif  // CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_LOGIN_PERMISSION_CONTROLLER_H_
