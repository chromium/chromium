// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_SIGNIN_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_SIGNIN_CONTROLLER_H_

#include "chrome/browser/ash/net/network_portal_web_dialog.h"

namespace ash {

class NetworkPortalSigninController : public NetworkPortalWebDialog::Delegate {
 public:
  NetworkPortalSigninController();
  NetworkPortalSigninController(const NetworkPortalSigninController&) = delete;
  NetworkPortalSigninController& operator=(
      const NetworkPortalSigninController&) = delete;
  ~NetworkPortalSigninController() override;

  // Returns a weak ptr to pass to the notification delegate.
  virtual base::WeakPtr<NetworkPortalSigninController> GetWeakPtr();

  // Shows the signin UI.
  virtual void ShowSignin();

  // Closes the signin UI if appropriate.
  virtual void CloseSignin();

  // Returns whether the sigin UI is show.
  virtual bool DialogIsShown();

  // NetworkPortalWebDialog::Delegate
  void OnDialogDestroyed(const NetworkPortalWebDialog* dialog) override;

 private:
  void ShowDialog();

  NetworkPortalWebDialog* dialog_ = nullptr;
  base::WeakPtrFactory<NetworkPortalWebDialog::Delegate>
      web_dialog_weak_factory_{this};
  base::WeakPtrFactory<NetworkPortalSigninController> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_SIGNIN_CONTROLLER_H_
