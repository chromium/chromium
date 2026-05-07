// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTRO_SIGN_IN_CELEBRATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INTRO_SIGN_IN_CELEBRATION_HANDLER_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/intro/sign_in_celebration.mojom.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class SignInCelebrationHandler : public intro::mojom::PageHandler,
                                 public signin::IdentityManager::Observer {
 public:
  SignInCelebrationHandler(
      signin::IdentityManager* identity_manager,
      mojo::PendingRemote<intro::mojom::Page> page,
      mojo::PendingReceiver<intro::mojom::PageHandler> receiver);

  SignInCelebrationHandler(const SignInCelebrationHandler&) = delete;
  SignInCelebrationHandler& operator=(
      const SignInCelebrationHandler&) = delete;

  ~SignInCelebrationHandler() override;

  // intro::mojom::PageHandler:
  void GetSignInCelebrationUserInfo(
      GetSignInCelebrationUserInfoCallback callback) override;

  // signin::IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

 private:
  // Sends an updated user info to the WebUI.
  void UpdateUserInfo();

  // Computes the user info to be sent to the WebUI.
  intro::mojom::SignInCelebrationUserInfoPtr GetUserInformationMojo();

  const raw_ref<signin::IdentityManager> identity_manager_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  mojo::Receiver<intro::mojom::PageHandler> receiver_;
  mojo::Remote<intro::mojom::Page> page_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTRO_SIGN_IN_CELEBRATION_HANDLER_H_
