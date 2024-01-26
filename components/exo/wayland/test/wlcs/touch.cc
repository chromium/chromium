// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/test/wlcs/touch.h"

#include <wayland-util.h>

#include <optional>

#include "base/logging.h"
#include "components/exo/wayland/test/wlcs/display_server.h"
#include "components/exo/wayland/test/wlcs/wlcs_helpers.h"
#include "ui/events/test/event_generator.h"

namespace exo::wlcs {

Touch::Touch(DisplayServer* server) : server_(server) {
  WlcsTouch::version = 1;
  WlcsTouch::touch_down = [](WlcsTouch* touch, wl_fixed_t x, wl_fixed_t y) {
    static_cast<Touch*>(touch)->TouchDown(x, y);
  };
  WlcsTouch::touch_move = [](WlcsTouch* touch, wl_fixed_t x, wl_fixed_t y) {
    static_cast<Touch*>(touch)->TouchMove(x, y);
  };
  WlcsTouch::touch_up = [](WlcsTouch* touch) {
    static_cast<Touch*>(touch)->TouchUp();
  };
  WlcsTouch::destroy = [](WlcsTouch* touch) {
    delete static_cast<Touch*>(touch);
  };
}

Touch::~Touch() = default;

void Touch::TouchDown(wl_fixed_t x, wl_fixed_t y) {
  server_->server()->GenerateEvent(base::BindOnce(
      [](wl_fixed_t x, wl_fixed_t y, ui::test::EventGenerator& evg) {
        evg.PressTouch(std::make_optional<gfx::Point>(wl_fixed_to_int(x),
                                                      wl_fixed_to_int(y)));
      },
      x, y));
}

void Touch::TouchMove(wl_fixed_t x, wl_fixed_t y) {
  server_->server()->GenerateEvent(base::BindOnce(
      [](wl_fixed_t x, wl_fixed_t y, ui::test::EventGenerator& evg) {
        evg.MoveTouch({wl_fixed_to_int(x), wl_fixed_to_int(y)});
      },
      x, y));
}

void Touch::TouchUp() {
  server_->server()->GenerateEvent(base::BindOnce(
      [](ui::test::EventGenerator& evg) { evg.ReleaseTouch(); }));
}

}  // namespace exo::wlcs
