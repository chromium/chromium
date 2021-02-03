// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/debug_utils.h"

namespace chrome {

base::Optional<int> GetKeyboardFocusedTabIndex(const Browser* browser) {
  BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser);
  if (view && view->tabstrip())
    return view->tabstrip()->GetFocusedTabIndex();
  return base::nullopt;
}

void ExecuteUIDebugCommand(int id, const Browser* browser) {
  if (!base::FeatureList::IsEnabled(features::kUIDebugTools))
    return;

  switch (id) {
    case IDC_DEBUG_TOGGLE_TABLET_MODE: {
      auto* controller = ui::TouchUiController::Get();
      // Chrome always uses touch_ui_state auto mode except in tests, so whether
      // it is touch ui relies on whether it is in tablet mode.
      controller->OnTabletModeToggled(!controller->touch_ui());
      break;
    }
    case IDC_DEBUG_PRINT_VIEW_TREE:
      // TODO(weili): replace with a new tree dumping utility which can show
      // detailed property info.
      PrintViewHierarchy(BrowserView::GetBrowserViewForBrowser(browser));
      break;

    default:
      NOTREACHED() << "Unimplemented UI Debug command: " << id;
      break;
  }
}

}  // namespace chrome
