// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_dismiss_menu_model.h"

#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/toasts/toast_metrics.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ToastDismissMenuModel,
                                      kToastDismissMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ToastDismissMenuModel,
                                      kToastDontShowAgainMenuItem);

ToastDismissMenuModel::ToastDismissMenuModel(ToastId toast_id)
    : ui::SimpleMenuModel(/*delegate=*/this), toast_id_(toast_id) {
  AddItem(static_cast<int>(toasts::ToastDismissMenuEntries::kDismiss),
          l10n_util::GetStringUTF16(IDS_TOAST_MENU_ITEM_DISMISS));
  SetElementIdentifierAt(0, kToastDismissMenuItem);
  AddItem(static_cast<int>(toasts::ToastDismissMenuEntries::kDontShowAgain),
          l10n_util::GetStringUTF16(IDS_TOAST_MENU_ITEM_DONT_SHOW_AGAIN));
  SetElementIdentifierAt(1, kToastDontShowAgainMenuItem);
}

ToastDismissMenuModel::~ToastDismissMenuModel() = default;

void ToastDismissMenuModel::ExecuteCommand(int command_id, int event_flags) {
  const toasts::ToastDismissMenuEntries command =
      static_cast<toasts::ToastDismissMenuEntries>(command_id);
  RecordToastDismissMenuClicked(toast_id_, command);

  switch (command) {
    case toasts::ToastDismissMenuEntries::kDismiss:
      // Toast will dismiss automatically when a menu item is clicked.
      return;
    case toasts::ToastDismissMenuEntries::kDontShowAgain:
      g_browser_process->local_state()->SetInteger(
          prefs::kToastAlertLevel,
          static_cast<int>(toasts::ToastAlertLevel::kActionable));
      return;
  }
  NOTREACHED();
}
