// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/server_util.h"

#include "base/time/time.h"
#include "components/exo/data_offer.h"
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

}  // namespace

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

}  // namespace wayland
}  // namespace exo
