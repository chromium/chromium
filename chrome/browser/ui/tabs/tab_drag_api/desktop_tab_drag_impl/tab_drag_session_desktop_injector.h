// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_DESKTOP_INJECTOR_H_
#define CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_DESKTOP_INJECTOR_H_

#include <memory>

#include "components/browser_apis/tab_drag/adapters/tab_drag_platform_provider.h"

namespace tabs_api {

class TabDragSessionInputAdapter;

class TabDragSessionDesktopInjector : public TabDragPlatformProvider {
 public:
  TabDragSessionDesktopInjector();
  TabDragSessionDesktopInjector(const TabDragSessionDesktopInjector&&) = delete;
  TabDragSessionDesktopInjector operator=(
      const TabDragSessionDesktopInjector&) = delete;
  ~TabDragSessionDesktopInjector() override;

  // TabDragPlatformProvider:
  TabDragSessionInputAdapter& tab_drag_session_input_adapter() override;

 private:
  std::unique_ptr<TabDragSessionInputAdapter> adapter_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_DESKTOP_INJECTOR_H_
