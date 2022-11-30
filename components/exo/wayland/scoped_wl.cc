// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/scoped_wl.h"

#include <wayland-server-core.h>

namespace exo {
namespace wayland {

void WlDisplayDeleter::operator()(wl_display* display) const {
  // Destroy all clients in the display manually, because wl_display_destroy()
  // doesn't do it.
  // TODO(penghuang): Remove below manual client tear down code if the
  // wl_display_destroy() is fixed upstream.
  wl_list* client_list = wl_display_get_client_list(display);
  while (!wl_list_empty(client_list)) {
    wl_client* client = wl_client_from_link(client_list->next);
    wl_client_destroy(client);
  }

  wl_display_destroy(display);
}

}  // namespace wayland
}  // namespace exo
