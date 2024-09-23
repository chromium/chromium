// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_RENDER_WIDGET_HOST_VIEW_INPUT_OBSERVER_H_
#define COMPONENTS_INPUT_RENDER_WIDGET_HOST_VIEW_INPUT_OBSERVER_H_

#include "base/component_export.h"

namespace input {

class RenderWidgetHostViewInput;

class COMPONENT_EXPORT(INPUT) RenderWidgetHostViewInputObserver {
 public:
  RenderWidgetHostViewInputObserver(const RenderWidgetHostViewInputObserver&) =
      delete;
  RenderWidgetHostViewInputObserver& operator=(
      const RenderWidgetHostViewInputObserver&) = delete;

  // All derived classes must de-register as observers when receiving this
  // notification.
  virtual void OnRenderWidgetHostViewInputDestroyed(
      RenderWidgetHostViewInput* view);

 protected:
  RenderWidgetHostViewInputObserver() = default;
  virtual ~RenderWidgetHostViewInputObserver();
};

}  // namespace input

#endif  // COMPONENTS_INPUT_RENDER_WIDGET_HOST_VIEW_INPUT_OBSERVER_H_
