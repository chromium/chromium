// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/test/server_util.h"

#include <wayland-util.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"

namespace exo::wayland::test::server_util {

wl_resource* LookUpResource(Server* server, const ResourceKey& key) {
  struct IteratorData {
    raw_ptr<wl_resource> result = nullptr;
    const raw_ref<const ResourceKey> key;
  };

  IteratorData iterator_data{.key = ToRawRef(key)};

  wl_client* client = nullptr;
  wl_list* all_clients =
      wl_display_get_client_list(server->GetWaylandDisplay());

  auto find_closure = [](struct wl_resource* resource, void* data) {
    IteratorData* iterator_data = static_cast<IteratorData*>(data);
    if (strcmp(wl_resource_get_class(resource),
               iterator_data->key->class_name.c_str()) == 0 &&
        wl_resource_get_id(resource) == iterator_data->key->id) {
      iterator_data->result = resource;
      return WL_ITERATOR_STOP;
    }
    return WL_ITERATOR_CONTINUE;
  };
  wl_client_for_each(client, all_clients) {
    wl_client_for_each_resource(client, find_closure, &iterator_data);
  }
  return iterator_data.result;
}

}  // namespace exo::wayland::test::server_util
