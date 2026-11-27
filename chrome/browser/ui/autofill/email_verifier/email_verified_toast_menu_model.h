// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFIED_TOAST_MENU_MODEL_H_
#define CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFIED_TOAST_MENU_MODEL_H_

#include "base/functional/callback.h"
#include "ui/menus/simple_menu_model.h"

namespace autofill {

class EmailVerifiedToastMenuModel : public ui::SimpleMenuModel,
                                    public ui::SimpleMenuModel::Delegate {
 public:
  enum CommandId {
    kManage = 0,
  };

  explicit EmailVerifiedToastMenuModel(
      base::RepeatingClosure open_settings_callback);
  EmailVerifiedToastMenuModel(const EmailVerifiedToastMenuModel&) = delete;
  EmailVerifiedToastMenuModel& operator=(const EmailVerifiedToastMenuModel&) =
      delete;
  ~EmailVerifiedToastMenuModel() override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  const base::RepeatingClosure open_settings_callback_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFIED_TOAST_MENU_MODEL_H_
