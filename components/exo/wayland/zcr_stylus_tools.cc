// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_stylus_tools.h"

#include <stylus-tools-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "base/memory/raw_ptr.h"
#include "components/exo/surface.h"
#include "components/exo/surface_observer.h"
#include "components/exo/wayland/server_util.h"

namespace exo {
namespace wayland {

namespace {

// A property key containing a boolean set to true if the stylus_tool
// object is associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasStylusToolKey, false)

////////////////////////////////////////////////////////////////////////////////
// stylus_tool interface:

class StylusTool : public SurfaceObserver {
 public:
  explicit StylusTool(Surface* surface) : surface_(surface) {
    surface_->AddSurfaceObserver(this);
    surface_->SetProperty(kSurfaceHasStylusToolKey, true);
  }

  StylusTool(const StylusTool&) = delete;
  StylusTool& operator=(const StylusTool&) = delete;

  ~StylusTool() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
      surface_->SetProperty(kSurfaceHasStylusToolKey, false);
    }
  }

  void SetStylusOnly() { surface_->SetStylusOnly(); }

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  raw_ptr<Surface> surface_;
};

void stylus_tool_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void stylus_tool_set_stylus_only(wl_client* client, wl_resource* resource) {
  GetUserDataAs<StylusTool>(resource)->SetStylusOnly();
}

const struct zcr_stylus_tool_v1_interface stylus_tool_implementation = {
    stylus_tool_destroy, stylus_tool_set_stylus_only};

////////////////////////////////////////////////////////////////////////////////
// stylus_tools interface:

void stylus_tools_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void stylus_tools_get_stylus_tool(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t id,
                                  wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceHasStylusToolKey)) {
    wl_resource_post_error(
        resource, ZCR_STYLUS_TOOLS_V1_ERROR_STYLUS_TOOL_EXISTS,
        "a stylus_tool object for that surface already exists");
    return;
  }

  wl_resource* stylus_tool_resource =
      wl_resource_create(client, &zcr_stylus_tool_v1_interface, 1, id);

  SetImplementation(stylus_tool_resource, &stylus_tool_implementation,
                    std::make_unique<StylusTool>(surface));
}

const struct zcr_stylus_tools_v1_interface stylus_tools_implementation = {
    stylus_tools_destroy, stylus_tools_get_stylus_tool};

}  // namespace

void bind_stylus_tools(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_stylus_tools_v1_interface, 1, id);

  wl_resource_set_implementation(resource, &stylus_tools_implementation, data,
                                 nullptr);
}

}  // namespace wayland
}  // namespace exo
