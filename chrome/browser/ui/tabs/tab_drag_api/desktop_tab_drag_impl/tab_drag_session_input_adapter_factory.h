// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_INPUT_ADAPTER_FACTORY_H_
#define CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_INPUT_ADAPTER_FACTORY_H_

#include <memory>

namespace tabs_api {

class TabDragSessionInputAdapter;

class TabDragSessionInputAdapterFactory {
 public:
  TabDragSessionInputAdapterFactory() = delete;
  ~TabDragSessionInputAdapterFactory() = delete;

  static std::unique_ptr<TabDragSessionInputAdapter> Create();
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_INPUT_ADAPTER_FACTORY_H_
