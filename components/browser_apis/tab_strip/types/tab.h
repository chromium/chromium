// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_TAB_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_TAB_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "components/tabs/public/tab_alert.h"
#include "components/tabs/public/tab_handle_factory.h"
#include "components/tabs/public/tab_network_state.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace tabs_api {

struct Tab {
  Tab();
  Tab(const Tab&);
  Tab(Tab&&) noexcept;
  Tab& operator=(const Tab&);
  Tab& operator=(Tab&&) noexcept;
  ~Tab();

  // Returns the underlying tab handle if `id` is a valid `kContent` id, or
  // `Handle::Null()` otherwise.
  tabs::SupportsTabHandles::Handle ToTabHandle() const;

  NodeId id;
  std::u16string title;
  GURL url;
  gfx::ImageSkia favicon;
  std::vector<tabs::TabAlert> alert_states;
  tabs::TabNetworkState network_state = tabs::TabNetworkState::kNone;
  bool is_active = false;
  bool is_selected = false;
  bool is_blocked = false;
  base::TimeTicks last_active;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_TAB_H_
