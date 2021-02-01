// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_OTHER_TRANSPORTS_MENU_MODEL_H_
#define CHROME_BROWSER_UI_WEBAUTHN_OTHER_TRANSPORTS_MENU_MODEL_H_

#include "build/build_config.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "ui/base/models/simple_menu_model.h"

// The model of the pop-up menu shown when `Choose another option` is clicked.
//
// This pop-up menu is available on several sheets instructing the user to
// activate their security key over a given transport, and allows the user to
// instead use a different transport protocol.
class OtherTransportsMenuModel
    : public ui::SimpleMenuModel,
      public ui::SimpleMenuModel::Delegate,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  OtherTransportsMenuModel(AuthenticatorRequestDialogModel* dialog_model,
                           AuthenticatorTransport current_transport);
  ~OtherTransportsMenuModel() override;

 protected:
  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // AuthenticatorRequestDialogModel::Observer:
  void OnModelDestroyed() override;

 private:
  // Appends all available transports except the |current_transport|
  void PopulateWithTransportsExceptFor(
      AuthenticatorTransport current_transport);

#if defined(OS_WIN)
  void AppendItemForNativeWinApi();
#endif

  AuthenticatorRequestDialogModel* dialog_model_;

  DISALLOW_COPY_AND_ASSIGN(OtherTransportsMenuModel);
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_OTHER_TRANSPORTS_MENU_MODEL_H_
