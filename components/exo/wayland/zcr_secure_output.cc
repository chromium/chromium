// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_secure_output.h"

#include <secure-output-unstable-v1-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "components/exo/surface.h"
#include "components/exo/surface_observer.h"
#include "components/exo/wayland/server_util.h"

namespace exo {
namespace wayland {
namespace {

// A property key containing a boolean set to true if a security object is
// associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasSecurityKey, false)

////////////////////////////////////////////////////////////////////////////////
// security_interface:

// Implements the security interface to a Surface. The "only visible on secure
// output"-state is set to false upon destruction. A window property will be set
// during the lifetime of this class to prevent multiple instances from being
// created for the same Surface.
class Security : public SurfaceObserver {
 public:
  explicit Security(Surface* surface) : surface_(surface) {
    surface_->AddSurfaceObserver(this);
    surface_->SetProperty(kSurfaceHasSecurityKey, true);
  }

  Security(const Security&) = delete;
  Security& operator=(const Security&) = delete;

  ~Security() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
      surface_->SetOnlyVisibleOnSecureOutput(false);
      surface_->SetProperty(kSurfaceHasSecurityKey, false);
    }
  }

  void OnlyVisibleOnSecureOutput() {
    if (surface_)
      surface_->SetOnlyVisibleOnSecureOutput(true);
  }

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  raw_ptr<Surface> surface_;
};

void security_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void security_only_visible_on_secure_output(wl_client* client,
                                            wl_resource* resource) {
  GetUserDataAs<Security>(resource)->OnlyVisibleOnSecureOutput();
}

const struct zcr_security_v1_interface security_implementation = {
    security_destroy, security_only_visible_on_secure_output};

////////////////////////////////////////////////////////////////////////////////
// secure_output_interface:

void secure_output_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void secure_output_get_security(wl_client* client,
                                wl_resource* resource,
                                uint32_t id,
                                wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceHasSecurityKey)) {
    wl_resource_post_error(resource, ZCR_SECURE_OUTPUT_V1_ERROR_SECURITY_EXISTS,
                           "a security object for that surface already exists");
    return;
  }

  wl_resource* security_resource =
      wl_resource_create(client, &zcr_security_v1_interface, 1, id);

  SetImplementation(security_resource, &security_implementation,
                    std::make_unique<Security>(surface));
}

const struct zcr_secure_output_v1_interface secure_output_implementation = {
    secure_output_destroy, secure_output_get_security};

}  // namespace

void bind_secure_output(wl_client* client,
                        void* data,
                        uint32_t version,
                        uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_secure_output_v1_interface, 1, id);

  wl_resource_set_implementation(resource, &secure_output_implementation, data,
                                 nullptr);
}

}  // namespace wayland
}  // namespace exo
