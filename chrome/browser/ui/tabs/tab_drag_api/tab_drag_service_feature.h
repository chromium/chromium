// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_DRAG_API_TAB_DRAG_SERVICE_FEATURE_H_
#define CHROME_BROWSER_UI_TABS_TAB_DRAG_API_TAB_DRAG_SERVICE_FEATURE_H_

#include <memory>

#include "components/browser_apis/tab_drag/tab_drag_api.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/gfx/native_ui_types.h"

class BrowserWindowInterface;

namespace tabs_api {
class TabDragServiceImpl;
class TabDragWindowAdapter;
}  // namespace tabs_api

// Public interface for retrieving the tab drag service, either through mojo
// or the native interface.
class TabDragServiceFeature {
 public:
  DECLARE_USER_DATA(TabDragServiceFeature);

  TabDragServiceFeature(
      std::unique_ptr<tabs_api::TabDragWindowAdapter> window_adapter,
      ui::UnownedUserDataHost& host);
  ~TabDragServiceFeature();

  // Returns the feature for `browser_window`, or null if it does not have
  // one.
  static TabDragServiceFeature* From(BrowserWindowInterface* browser_window);

  TabDragServiceFeature(const TabDragServiceFeature&) = delete;
  TabDragServiceFeature& operator=(const TabDragServiceFeature&) = delete;

  void AcceptDragService(
      mojo::PendingReceiver<tabs_api::mojom::TabDragService> client,
      gfx::NativeView context_view);

 private:
  std::unique_ptr<tabs_api::TabDragServiceImpl> tab_drag_service_;

  ui::ScopedUnownedUserData<TabDragServiceFeature> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_DRAG_API_TAB_DRAG_SERVICE_FEATURE_H_
