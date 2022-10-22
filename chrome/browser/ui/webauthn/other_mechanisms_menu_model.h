// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_OTHER_MECHANISMS_MENU_MODEL_H_
#define CHROME_BROWSER_UI_WEBAUTHN_OTHER_MECHANISMS_MENU_MODEL_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/models/simple_menu_model.h"

class AuthenticatorRequestDialogModel;

// The model of the pop-up menu shown when `Choose another option` is clicked.
//
// This pop-up menu is available on several sheets instructing the user to
// activate their security key over a given transport, and allows the user to
// instead use a different transport protocol.
class OtherMechanismsMenuModel : public ui::SimpleMenuModel,
                                 public ui::SimpleMenuModel::Delegate {
 public:
  explicit OtherMechanismsMenuModel(
      AuthenticatorRequestDialogModel* dialog_model);

  OtherMechanismsMenuModel(const OtherMechanismsMenuModel&) = delete;
  OtherMechanismsMenuModel& operator=(const OtherMechanismsMenuModel&) = delete;

  ~OtherMechanismsMenuModel() override;

 protected:
  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  const raw_ptr<AuthenticatorRequestDialogModel, DanglingUntriaged>
      dialog_model_;
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_OTHER_MECHANISMS_MENU_MODEL_H_
