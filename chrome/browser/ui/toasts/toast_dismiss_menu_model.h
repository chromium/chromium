// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOASTS_TOAST_DISMISS_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOASTS_TOAST_DISMISS_MENU_MODEL_H_

#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/menus/simple_menu_model.h"

class ToastDismissMenuModel : public ui::SimpleMenuModel,
                              public ui::SimpleMenuModel::Delegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kToastDismissMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kToastDontShowAgainMenuItem);

  explicit ToastDismissMenuModel(ToastId toast_id);
  ~ToastDismissMenuModel() override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  const ToastId toast_id_;
};

#endif  // CHROME_BROWSER_UI_TOASTS_TOAST_DISMISS_MENU_MODEL_H_
