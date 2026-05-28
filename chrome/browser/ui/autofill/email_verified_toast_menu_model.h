// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIED_TOAST_MENU_MODEL_H_
#define CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIED_TOAST_MENU_MODEL_H_

#include "base/memory/raw_ptr.h"
#include "ui/menus/simple_menu_model.h"

class BrowserWindowInterface;

namespace autofill {

class EmailVerifiedToastMenuModel : public ui::SimpleMenuModel,
                                    public ui::SimpleMenuModel::Delegate {
 public:
  explicit EmailVerifiedToastMenuModel(BrowserWindowInterface* window);
  EmailVerifiedToastMenuModel(const EmailVerifiedToastMenuModel&) = delete;
  EmailVerifiedToastMenuModel& operator=(const EmailVerifiedToastMenuModel&) =
      delete;
  ~EmailVerifiedToastMenuModel() override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  raw_ptr<BrowserWindowInterface> window_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIED_TOAST_MENU_MODEL_H_
