// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/server_util.h"

#include <wayland-server-core.h>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "components/exo/data_offer.h"
#include "components/exo/wayland/server.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(wl_resource*)

namespace exo {
namespace wayland {

namespace {

// A property key containing the surface resource that is associated with
// window. If unset, no surface resource is associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(wl_resource*, kSurfaceResourceKey, nullptr)

// A property key containing the data offer resource that is associated with
// data offer object.
DEFINE_UI_CLASS_PROPERTY_KEY(wl_resource*, kDataOfferResourceKey, nullptr)

// Wayland provides no convenient way to annotate a wl_display with arbitrary
// data. Therefore, to map displays to their security_delegate, we maintain this
// mapping.
base::flat_map<wl_display*, SecurityDelegate*> g_display_security_map;

}  // namespace

void SetImplementation(wl_resource* resource, const void* implementation) {
  wl_resource_set_implementation(resource, implementation, nullptr, nullptr);
}

uint32_t TimeTicksToMilliseconds(base::TimeTicks ticks) {
  return (ticks - base::TimeTicks()).InMilliseconds();
}

uint32_t NowInMilliseconds() {
  return TimeTicksToMilliseconds(base::TimeTicks::Now());
}

wl_resource* GetSurfaceResource(Surface* surface) {
  return surface->GetProperty(kSurfaceResourceKey);
}

void SetSurfaceResource(Surface* surface, wl_resource* resource) {
  surface->SetProperty(kSurfaceResourceKey, resource);
}

wl_resource* GetDataOfferResource(const DataOffer* data_offer) {
  return data_offer->GetProperty(kDataOfferResourceKey);
}

void SetDataOfferResource(DataOffer* data_offer,
                          wl_resource* data_offer_resource) {
  data_offer->SetProperty(kDataOfferResourceKey, data_offer_resource);
}

void SetSecurityDelegate(wl_display* display,
                         SecurityDelegate* security_delegate) {
  auto emplace_result =
      g_display_security_map.emplace(display, security_delegate);
  DCHECK(emplace_result.second);
}

void RemoveSecurityDelegate(wl_display* display) {
  DCHECK(g_display_security_map.contains(display));
  g_display_security_map.erase(display);
}

SecurityDelegate* GetSecurityDelegate(wl_display* display) {
  DCHECK(g_display_security_map.contains(display));
  return g_display_security_map[display];
}

SecurityDelegate* GetSecurityDelegate(wl_client* client) {
  return GetSecurityDelegate(wl_client_get_display(client));
}

bool IsClientDestroyed(wl_client* client) {
  CHECK(client);

  // There should always be a display associated with a client and display will
  // always outlive the wl_client object.
  wl_display* display = wl_client_get_display(client);
  CHECK(display);

  Server* server = Server::GetServerForDisplay(display);
  return server ? server->IsClientDestroyed(client) : true;
}

}  // namespace wayland
}  // namespace exo
