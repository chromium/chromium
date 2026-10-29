// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_BOOKMARK_BAR_UI_CONTROLLER_OBSERVER_H_
#define COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_BOOKMARK_BAR_UI_CONTROLLER_OBSERVER_H_

#include "base/observer_list_types.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/bookmark_bar_api.mojom.h"

namespace bookmark_bar_api {

// The in-process equivalent of mojom::BookmarkBarObserver, for synchronous
// clients. Notifications are delivered before the triggering call returns.
class BookmarkBarUIControllerObserver : public base::CheckedObserver {
 public:
  ~BookmarkBarUIControllerObserver() override = default;

  virtual void OnDisplayOptionsChanged(const mojom::DisplayOptions& options) {}
  virtual void OnContextMenuClosed() {}
};

}  // namespace bookmark_bar_api

#endif  // COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_BOOKMARK_BAR_UI_CONTROLLER_OBSERVER_H_
