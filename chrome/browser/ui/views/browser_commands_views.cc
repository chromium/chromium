// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/wm/public/activation_client.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/platform_util.h"
#endif

namespace {
views::View* GetActiveWindowRootView(const Browser* browser) {
#if defined(USE_AURA)
  wm::ActivationClient* client = wm::GetActivationClient(
      browser->window()->GetNativeWindow()->GetRootWindow());
  if (!client)
    return nullptr;
  gfx::NativeWindow active_window = client->GetActiveWindow();
#elif BUILDFLAG(IS_MAC)
  NSWindow* active_window = platform_util::GetActiveWindow();
  if (!active_window)
    return nullptr;
#endif

  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(active_window);
  return widget ? widget->GetRootView() : nullptr;
}
}  // namespace

namespace chrome {

std::optional<int> GetKeyboardFocusedTabIndex(const Browser* browser) {
  BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser);
  if (view && view->tabstrip())
    return view->tabstrip()->GetFocusedTabIndex();
  return std::nullopt;
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
      if (views::View* view = GetActiveWindowRootView(browser))
        PrintViewHierarchy(view);
      break;
    case IDC_DEBUG_PRINT_VIEW_TREE_DETAILS:
      if (views::View* view = GetActiveWindowRootView(browser))
        PrintViewHierarchy(view, /* verbose= */ true);
      break;
    default:
      NOTREACHED() << "Unimplemented UI Debug command: " << id;
  }
}

}  // namespace chrome
