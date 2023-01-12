// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZCR_UI_CONTROLS_H_
#define COMPONENTS_EXO_WAYLAND_ZCR_UI_CONTROLS_H_

#include <memory>

#include "base/component_export.h"

namespace exo::wayland {
class Server;

class COMPONENT_EXPORT(UI_CONTROLS_PROTOCOL) UiControls {
 public:
  explicit UiControls(Server* server);
  UiControls(const UiControls&) = delete;
  UiControls& operator=(const UiControls&) = delete;
  ~UiControls();

  // Tracks button and mouse states as well as pending requests testing.
  struct UiControlsState;

 private:
  std::unique_ptr<UiControlsState> state_;
};

}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_ZCR_UI_CONTROLS_H_
