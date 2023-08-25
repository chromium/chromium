// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_SERVER_UTIL_H_
#define COMPONENTS_EXO_WAYLAND_SERVER_UTIL_H_

#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "components/exo/surface.h"
#include "ui/base/class_property.h"

struct wl_resource;

namespace exo {

class SecurityDelegate;
class DataOffer;

namespace wayland {

template <class T>
T* GetUserDataAs(wl_resource* resource) {
  return static_cast<T*>(wl_resource_get_user_data(resource));
}

template <class T>
std::unique_ptr<T> TakeUserDataAs(wl_resource* resource) {
  std::unique_ptr<T> user_data = base::WrapUnique(GetUserDataAs<T>(resource));
  wl_resource_set_user_data(resource, nullptr);
  return user_data;
}

template <class T>
void DestroyUserData(wl_resource* resource) {
  TakeUserDataAs<T>(resource);
}

template <class T>
void SetImplementation(wl_resource* resource,
                       const void* implementation,
                       std::unique_ptr<T> user_data) {
  wl_resource_set_implementation(resource, implementation, user_data.release(),
                                 DestroyUserData<T>);
}

void SetImplementation(wl_resource* resource, const void* implementation);

// Convert a timestamp to a time value that can be used when interfacing
// with wayland. Note that we cast a int64_t value to uint32_t which can
// potentially overflow.
uint32_t TimeTicksToMilliseconds(base::TimeTicks ticks);
uint32_t NowInMilliseconds();

wl_resource* GetSurfaceResource(Surface* surface);
void SetSurfaceResource(Surface* surface, wl_resource* resource);

wl_resource* GetDataOfferResource(const DataOffer* data_offer);
void SetDataOfferResource(DataOffer* data_offer,
                          wl_resource* data_offer_resource);

// Associates the given |display| with its |security_delegate|.
void SetSecurityDelegate(wl_display* display,
                         SecurityDelegate* security_delegate);

// Clears the SecurityDelegate association for |display|.
void RemoveSecurityDelegate(wl_display* display);

// Returns the associated security_delegate for this |display|.
SecurityDelegate* GetSecurityDelegate(wl_display* display);

// Returns the associated security_delegate for the display this |client| is
// connected to.
SecurityDelegate* GetSecurityDelegate(wl_client* client);

// Returns whether a client has initiated destruction. After destruction begins
// resources associated with the client start to be freed and there is a risk of
// UAFs in querying for client resources (see crbug.com/1433187).
bool IsClientDestroyed(wl_client* client);

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_SERVER_UTIL_H_
