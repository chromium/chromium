// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_DRAG_API_TAB_DRAG_SERVICE_FEATURE_H_
#define CHROME_BROWSER_UI_TABS_TAB_DRAG_API_TAB_DRAG_SERVICE_FEATURE_H_

#include <memory>

#include "components/browser_apis/tab_drag/tab_drag_api.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace tabs_api {
class TabDragServiceImpl;
class TabDragWindowAdapter;
}  // namespace tabs_api

// Public interface for retrieving the tab drag service, either through mojo
// or the native interface.
class TabDragServiceFeature {
 public:
  explicit TabDragServiceFeature(
      std::unique_ptr<tabs_api::TabDragWindowAdapter> window_adapter);
  ~TabDragServiceFeature();

  TabDragServiceFeature(const TabDragServiceFeature&) = delete;
  TabDragServiceFeature& operator=(const TabDragServiceFeature&) = delete;

  void AcceptDragService(
      mojo::PendingReceiver<tabs_api::mojom::TabDragService> client);

 private:
  std::unique_ptr<tabs_api::TabDragServiceImpl> tab_drag_service_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_DRAG_API_TAB_DRAG_SERVICE_FEATURE_H_
