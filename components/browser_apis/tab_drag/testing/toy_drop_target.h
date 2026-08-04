// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_DROP_TARGET_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_DROP_TARGET_H_

#include <vector>

#include "components/browser_apis/tab_drag/tab_drag_api.mojom.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "ui/gfx/geometry/point.h"

namespace tabs_api {

class ToyDropTarget : public mojom::DropTarget {
 public:
  struct ReceivedEvent {
    enum class Type {
      kEntered,
      kDrag,
      kLeave,
      kDrop,
      kCancelled,
    };
    Type type;
    std::vector<tabs_api::NodeId> tab_ids;
    gfx::Point local_point;
  };

  ToyDropTarget();
  ToyDropTarget(const ToyDropTarget&) = delete;
  ToyDropTarget& operator=(const ToyDropTarget&) = delete;
  ~ToyDropTarget() override;

  // mojom::DropTarget:

  void OnDragEntered(const std::vector<tabs_api::NodeId>& source_tab_ids,
                     const gfx::Point& local_point,
                     int32_t tab_original_offset_x) override;
  void OnDrag(const gfx::Point& local_point) override;
  void OnDragLeave() override;
  void OnDrop(const std::vector<tabs_api::NodeId>& source_tab_ids,
              const gfx::Point& local_point) override;
  void OnDragCancelled() override;

  const std::vector<ReceivedEvent>& events() const { return events_; }
  void ClearEvents();

 private:
  std::vector<ReceivedEvent> events_;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_DROP_TARGET_H_
