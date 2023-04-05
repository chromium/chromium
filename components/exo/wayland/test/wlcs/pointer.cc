// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/test/wlcs/pointer.h"

#include <linux/input-event-codes.h>
#include <wayland-util.h>

#include "base/logging.h"
#include "components/exo/wayland/fuzzer/server_environment.h"
#include "components/exo/wayland/test/wlcs/display_server.h"
#include "components/exo/wayland/test/wlcs/wlcs_helpers.h"
#include "ui/events/test/event_generator.h"

namespace exo::wlcs {

Pointer::Pointer(DisplayServer* server) : server_(server) {
  WlcsPointer::version = 1;
  WlcsPointer::move_absolute = [](WlcsPointer* pointer, wl_fixed_t x,
                                  wl_fixed_t y) {
    static_cast<Pointer*>(pointer)->MoveAbsolute(x, y);
  };
  WlcsPointer::move_relative = [](WlcsPointer* pointer, wl_fixed_t x,
                                  wl_fixed_t y) {
    static_cast<Pointer*>(pointer)->MoveRelative(x, y);
  };
  WlcsPointer::button_up = [](WlcsPointer* pointer, int button) {
    static_cast<Pointer*>(pointer)->ButtonUp(button);
  };
  WlcsPointer::button_down = [](WlcsPointer* pointer, int button) {
    static_cast<Pointer*>(pointer)->ButtonDown(button);
  };
  WlcsPointer::destroy = [](WlcsPointer* pointer) {
    delete static_cast<Pointer*>(pointer);
  };
}

Pointer::~Pointer() = default;

void Pointer::MoveAbsolute(wl_fixed_t x, wl_fixed_t y) {
  server_->server()->GenerateEvent(base::BindOnce(
      [](wl_fixed_t x, wl_fixed_t y, ui::test::EventGenerator& evg) {
        evg.MoveMouseTo({wl_fixed_to_int(x), wl_fixed_to_int(y)});
      },
      x, y));
}

void Pointer::MoveRelative(wl_fixed_t x, wl_fixed_t y) {
  server_->server()->GenerateEvent(base::BindOnce(
      [](wl_fixed_t x, wl_fixed_t y, ui::test::EventGenerator& evg) {
        evg.MoveMouseBy(wl_fixed_to_int(x), wl_fixed_to_int(y));
      },
      x, y));
}

void Pointer::ButtonUp(int button) {
  server_->server()->GenerateEvent(base::BindOnce(
      [](int button, ui::test::EventGenerator& evg) {
        if (button == BTN_LEFT) {
          evg.ReleaseLeftButton();
        } else if (button == BTN_RIGHT) {
          evg.ReleaseRightButton();
        } else {
          LOG(FATAL) << "Unknown Button " << button;
        }
      },
      button));
}

void Pointer::ButtonDown(int button) {
  server_->server()->GenerateEvent(base::BindOnce(
      [](int button, ui::test::EventGenerator& evg) {
        if (button == BTN_LEFT) {
          evg.PressLeftButton();
        } else if (button == BTN_RIGHT) {
          evg.PressRightButton();
        } else {
          LOG(FATAL) << "Unknown Button " << button;
        }
      },
      button));
}

}  // namespace exo::wlcs
