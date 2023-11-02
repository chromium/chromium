// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_cursor_shapes.h"

#include <cursor-shapes-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "components/exo/pointer.h"
#include "components/exo/wayland/server_util.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"

namespace exo {
namespace wayland {

namespace {

////////////////////////////////////////////////////////////////////////////////
// cursor_shapes interface:

static ui::mojom::CursorType GetCursorType(int32_t cursor_shape) {
  switch (cursor_shape) {
#define ADD_CASE(wayland, chrome)                        \
  case ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_##wayland: \
    return ui::mojom::CursorType::chrome

    ADD_CASE(POINTER, kPointer);
    ADD_CASE(CROSS, kCross);
    ADD_CASE(HAND, kHand);
    ADD_CASE(IBEAM, kIBeam);
    ADD_CASE(WAIT, kWait);
    ADD_CASE(HELP, kHelp);
    ADD_CASE(EAST_RESIZE, kEastResize);
    ADD_CASE(NORTH_RESIZE, kNorthResize);
    ADD_CASE(NORTH_EAST_RESIZE, kNorthEastResize);
    ADD_CASE(NORTH_WEST_RESIZE, kNorthWestResize);
    ADD_CASE(SOUTH_RESIZE, kSouthResize);
    ADD_CASE(SOUTH_EAST_RESIZE, kSouthEastResize);
    ADD_CASE(SOUTH_WEST_RESIZE, kSouthWestResize);
    ADD_CASE(WEST_RESIZE, kWestResize);
    ADD_CASE(NORTH_SOUTH_RESIZE, kNorthSouthResize);
    ADD_CASE(EAST_WEST_RESIZE, kEastWestResize);
    ADD_CASE(NORTH_EAST_SOUTH_WEST_RESIZE, kNorthEastSouthWestResize);
    ADD_CASE(NORTH_WEST_SOUTH_EAST_RESIZE, kNorthWestSouthEastResize);
    ADD_CASE(COLUMN_RESIZE, kColumnResize);
    ADD_CASE(ROW_RESIZE, kRowResize);
    ADD_CASE(MIDDLE_PANNING, kMiddlePanning);
    ADD_CASE(EAST_PANNING, kEastPanning);
    ADD_CASE(NORTH_PANNING, kNorthPanning);
    ADD_CASE(NORTH_EAST_PANNING, kNorthEastPanning);
    ADD_CASE(NORTH_WEST_PANNING, kNorthWestPanning);
    ADD_CASE(SOUTH_PANNING, kSouthPanning);
    ADD_CASE(SOUTH_EAST_PANNING, kSouthEastPanning);
    ADD_CASE(SOUTH_WEST_PANNING, kSouthWestPanning);
    ADD_CASE(WEST_PANNING, kWestPanning);
    ADD_CASE(MOVE, kMove);
    ADD_CASE(VERTICAL_TEXT, kVerticalText);
    ADD_CASE(CELL, kCell);
    ADD_CASE(CONTEXT_MENU, kContextMenu);
    ADD_CASE(ALIAS, kAlias);
    ADD_CASE(PROGRESS, kProgress);
    ADD_CASE(NO_DROP, kNoDrop);
    ADD_CASE(COPY, kCopy);
    ADD_CASE(NONE, kNone);
    ADD_CASE(NOT_ALLOWED, kNotAllowed);
    ADD_CASE(ZOOM_IN, kZoomIn);
    ADD_CASE(ZOOM_OUT, kZoomOut);
    ADD_CASE(GRAB, kGrab);
    ADD_CASE(GRABBING, kGrabbing);
    ADD_CASE(DND_NONE, kDndNone);
    ADD_CASE(DND_MOVE, kDndMove);
    ADD_CASE(DND_COPY, kDndCopy);
    ADD_CASE(DND_LINK, kDndLink);
#undef ADD_CASE
    default:
      return ui::mojom::CursorType::kNull;
  }
}

void cursor_shapes_set_cursor_shape(wl_client* client,
                                    wl_resource* resource,
                                    wl_resource* pointer_resource,
                                    int32_t shape) {
  ui::mojom::CursorType cursor_type = GetCursorType(shape);
  if (cursor_type == ui::mojom::CursorType::kNull) {
    wl_resource_post_error(resource, ZCR_CURSOR_SHAPES_V1_ERROR_INVALID_SHAPE,
                           "Unrecognized shape %d", shape);
    return;
  }

  Pointer* pointer = GetUserDataAs<Pointer>(pointer_resource);
  pointer->SetCursorType(cursor_type);
}

const struct zcr_cursor_shapes_v1_interface cursor_shapes_implementation = {
    cursor_shapes_set_cursor_shape};

}  // namespace

void bind_cursor_shapes(wl_client* client,
                        void* data,
                        uint32_t version,
                        uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_cursor_shapes_v1_interface, version, id);

  wl_resource_set_implementation(resource, &cursor_shapes_implementation, data,
                                 nullptr);
}

}  // namespace wayland
}  // namespace exo
