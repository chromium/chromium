// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/chrome_javascript_app_modal_dialog_view_factory.h"

#include <memory>

#include "chrome/browser/ui/javascript_dialogs/chrome_app_modal_dialog_manager_delegate.h"
#include "components/javascript_dialogs/app_modal_dialog_manager.h"

void SetChromeAppModalDialogManagerDelegate() {
  javascript_dialogs::AppModalDialogManager::GetInstance()->SetDelegate(
      std::make_unique<ChromeAppModalDialogManagerDelegate>());
}
