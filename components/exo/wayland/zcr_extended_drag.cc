// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_extended_drag.h"

#include <extended-drag-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "components/exo/data_offer.h"
#include "components/exo/data_source.h"
#include "components/exo/display.h"
#include "components/exo/extended_drag_offer.h"
#include "components/exo/extended_drag_source.h"
#include "components/exo/surface.h"
#include "components/exo/wayland/server_util.h"
#include "ui/gfx/geometry/vector2d.h"

namespace exo {
namespace wayland {

namespace {

////////////////////////////////////////////////////////////////////////////////
// zcr_extended_drag_source interface:

class ZcrExtendedDragSourceDelegate : public ExtendedDragSource::Delegate {
 public:
  ZcrExtendedDragSourceDelegate(wl_resource* resource, uint32_t settings)
      : resource_(resource), settings_(settings) {}

  ZcrExtendedDragSourceDelegate(const ZcrExtendedDragSourceDelegate&) = delete;
  ZcrExtendedDragSourceDelegate& operator=(
      const ZcrExtendedDragSourceDelegate&) = delete;

  ~ZcrExtendedDragSourceDelegate() override = default;

  // ExtendedDragSource::Delegate:
  bool ShouldAllowDropAnywhere() const override {
    return settings_ & ZCR_EXTENDED_DRAG_V1_OPTIONS_ALLOW_DROP_NO_TARGET;
  }

  bool ShouldLockCursor() const override {
    return settings_ & ZCR_EXTENDED_DRAG_V1_OPTIONS_LOCK_CURSOR;
  }

  void OnDataSourceDestroying() override { delete this; }

 private:
  const raw_ptr<wl_resource> resource_;
  const uint32_t settings_;
};

void extended_drag_source_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void extended_drag_source_drag(wl_client* client,
                               wl_resource* resource,
                               wl_resource* surface_resource,
                               int32_t x_offset,
                               int32_t y_offset) {
  Surface* surface =
      surface_resource ? GetUserDataAs<Surface>(surface_resource) : nullptr;
  gfx::Vector2d offset{x_offset, y_offset};
  GetUserDataAs<ExtendedDragSource>(resource)->Drag(surface, offset);
}

const struct zcr_extended_drag_source_v1_interface
    extended_drag_source_implementation = {extended_drag_source_destroy,
                                           extended_drag_source_drag};

////////////////////////////////////////////////////////////////////////////////
// zcr_extended_drag_offer interface:

class ZcrExtendedOfferDelegate : public ExtendedDragOffer::Delegate {
 public:
  explicit ZcrExtendedOfferDelegate(wl_resource* resource)
      : resource_(resource) {
    DCHECK(resource_);
  }

  ZcrExtendedOfferDelegate(const ZcrExtendedOfferDelegate&) = delete;
  ZcrExtendedOfferDelegate& operator=(const ZcrExtendedOfferDelegate&) = delete;

  ~ZcrExtendedOfferDelegate() override = default;

  // ExtendedDragOffer::Delegate:
  void OnDataOfferDestroying() override { delete this; }

 private:
  const raw_ptr<wl_resource> resource_;
};

void extended_drag_offer_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void extended_drag_offer_swallow(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t serial,
                                 const char* mime_type) {
  // [deprecatd]: No need to implement this.
}

void extended_drag_offer_unswallow(wl_client* client,
                                   wl_resource* resource,
                                   uint32_t serial,
                                   const char* mime_type,
                                   int32_t x_offset,
                                   int32_t y_offset) {
  // [deprecatd]: No need to implement this.
}

const struct zcr_extended_drag_offer_v1_interface
    extended_drag_offer_implementation = {extended_drag_offer_destroy,
                                          extended_drag_offer_swallow,
                                          extended_drag_offer_unswallow};

////////////////////////////////////////////////////////////////////////////////
// zcr_extended_drag interface:

void extended_drag_get_extended_drag_source(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t id,
                                            wl_resource* data_source_resource,
                                            uint32_t settings) {
  DataSource* source = GetUserDataAs<DataSource>(data_source_resource);

  wl_resource* extended_drag_source_resource =
      wl_resource_create(client, &zcr_extended_drag_source_v1_interface,
                         wl_resource_get_version(resource), id);

  SetImplementation(extended_drag_source_resource,
                    &extended_drag_source_implementation,
                    std::make_unique<ExtendedDragSource>(
                        source, new ZcrExtendedDragSourceDelegate(
                                    extended_drag_source_resource, settings)));
}

void extended_drag_get_extended_drag_offer(wl_client* client,
                                           wl_resource* resource,
                                           uint32_t id,
                                           wl_resource* data_offer_resource) {
  DataOffer* offer = GetUserDataAs<DataOffer>(data_offer_resource);

  wl_resource* extended_drag_offer_resource =
      wl_resource_create(client, &zcr_extended_drag_offer_v1_interface,
                         wl_resource_get_version(resource), id);

  SetImplementation(
      extended_drag_offer_resource, &extended_drag_offer_implementation,
      std::make_unique<ExtendedDragOffer>(
          offer, new ZcrExtendedOfferDelegate(extended_drag_offer_resource)));
}

const struct zcr_extended_drag_v1_interface extended_drag_implementation = {
    extended_drag_get_extended_drag_source,
    extended_drag_get_extended_drag_offer};

}  // namespace

void bind_extended_drag(wl_client* client,
                        void* data,
                        uint32_t version,
                        uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_extended_drag_v1_interface, version, id);

  wl_resource_set_implementation(resource, &extended_drag_implementation, data,
                                 nullptr);
}

}  // namespace wayland
}  // namespace exo
