// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_SESSION_INPUT_ADAPTER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_SESSION_INPUT_ADAPTER_H_

#include <vector>

#include "base/types/expected.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "mojo/public/mojom/base/error.mojom.h"

namespace tabs_api {

class ToyTabDragSessionInputAdapter : public TabDragSessionInputAdapter {
 public:
  ToyTabDragSessionInputAdapter();
  ~ToyTabDragSessionInputAdapter() override;

  // TabDragSessionInputAdapter overrides:
  base::expected<void, mojo_base::mojom::ErrorPtr> StartInputCapture(
      const std::vector<tabs_api::NodeId>& source_tab_ids,
      EventCallback callback) override;

  void ReleaseInputCapture() override;

  void SendToyEvent(TabDragInputEvent::Type type,
                    const gfx::Point& screen_point = {});

  bool capture_started() const { return capture_started_; }
  bool capture_released() const { return capture_released_; }

 private:
  bool capture_started_ = false;
  bool capture_released_ = false;
  EventCallback callback_;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_SESSION_INPUT_ADAPTER_H_
