// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/wayland/surface_augmenter.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/exo/buffer.h"
#include "components/exo/sub_surface.h"
#include "components/exo/sub_surface_observer.h"
#include "components/exo/surface.h"
#include "components/exo/surface_observer.h"
#include "components/exo/wayland/server_util.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/size.h"

namespace exo::wayland {

namespace {

// A property key containing a boolean set to true if a surface augmenter is
// associated with with subsurface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSubSurfaceHasAugmentedSubSurfaceKey, false)

////////////////////////////////////////////////////////////////////////////////
// augmented_surface_interface:

// Implements the augmenter interface to a Surface. The "augmented"-state is set
// to null upon destruction. A window property will be set during the lifetime
// of this class to prevent multiple instances from being created for the same
// Surface.
class AugmentedSurface : public SurfaceObserver {
 public:
  explicit AugmentedSurface(Surface* surface) : surface_(surface) {
    surface_->AddSurfaceObserver(this);
    surface_->set_is_augmented(true);
    // No need to create AX Tree for augmented surfaces because they're
    // equivalent to quads.
    // TODO(b/296326746): Revert this CL and set the property to the root
    // surface once arc accessibility is refactored.
    surface_->window()->SetProperty(ui::kAXConsiderInvisibleAndIgnoreChildren,
                                    true);
    surface_->set_leave_enter_callback(Surface::LeaveEnterCallback());
    surface_->set_legacy_buffer_release_skippable(true);
  }
  AugmentedSurface(const AugmentedSurface&) = delete;
  AugmentedSurface& operator=(const AugmentedSurface&) = delete;
  ~AugmentedSurface() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
    }
  }

  void SetCorners(float x,
                  float y,
                  float width,
                  float height,
                  float top_left,
                  float top_right,
                  float bottom_right,
                  float bottom_left) {
    surface_->SetRoundedCorners(
        gfx::RRectF(gfx::RectF(x, y, width, height),
                    gfx::RoundedCornersF(top_left, top_right, bottom_right,
                                         bottom_left)),
        /*commit_override=*/false);
  }

  void SetDestination(float width, float height) {
    surface_->SetViewport(gfx::SizeF(width, height));
  }

  void SetBackgroundColor(std::optional<SkColor4f> background_color) {
    surface_->SetBackgroundColor(background_color);
  }

  void SetClipRect(float x, float y, float width, float height) {
    std::optional<gfx::RectF> clip_rect;
    if (width >= 0 && height >= 0) {
      clip_rect = gfx::RectF(x, y, width, height);
    }
    surface_->SetClipRect(clip_rect);
  }

  void SetFrameTraceId(int64_t frame_trace_id) {
    surface_->SetFrameTraceId(frame_trace_id);
  }

  // SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  raw_ptr<Surface> surface_;
};

void augmented_surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void augmented_surface_set_corners_DEPRECATED(wl_client* client,
                                              wl_resource* resource,
                                              wl_fixed_t top_left,
                                              wl_fixed_t top_right,
                                              wl_fixed_t bottom_right,
                                              wl_fixed_t bottom_left) {
  LOG(WARNING) << "Deprecated. The server doesn't support this request.";
}

void augmented_surface_set_destination_size(wl_client* client,
                                            wl_resource* resource,
                                            wl_fixed_t width,
                                            wl_fixed_t height) {
  if (width < 0 || height < 0) {
    wl_resource_post_error(resource, AUGMENTED_SURFACE_ERROR_BAD_VALUE,
                           "Dimension can't be negative (%d, %d)", width,
                           height);
    return;
  }

  GetUserDataAs<AugmentedSurface>(resource)->SetDestination(
      wl_fixed_to_double(width), wl_fixed_to_double(height));
}

void augmented_surface_set_rounded_corners_bounds_DEPRECATED(
    wl_client* client,
    wl_resource* resource,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    wl_fixed_t top_left,
    wl_fixed_t top_right,
    wl_fixed_t bottom_right,
    wl_fixed_t bottom_left) {
  LOG(WARNING) << "Deprecated. The server does not support this request.";
}

void augmented_surface_set_background_color(wl_client* client,
                                            wl_resource* resource,
                                            wl_array* color_data) {
  std::optional<SkColor4f> sk_color;
  // Empty data means no color.
  if (color_data->size) {
    float* data = reinterpret_cast<float*>(color_data->data);
    sk_color = {data[0], data[1], data[2], data[3]};
  }

  GetUserDataAs<AugmentedSurface>(resource)->SetBackgroundColor(sk_color);
}

void augmented_surface_set_trusted_damage_DEPRECATED(wl_client* client,
                                                     wl_resource* resource,
                                                     int enabled) {
  LOG(WARNING) << "Deprecated. The server doesn't support this request.";
}

void augmented_surface_set_rounded_corners_clip_bounds(wl_client* client,
                                                       wl_resource* resource,
                                                       wl_fixed_t x,
                                                       wl_fixed_t y,
                                                       wl_fixed_t width,
                                                       wl_fixed_t height,
                                                       wl_fixed_t top_left,
                                                       wl_fixed_t top_right,
                                                       wl_fixed_t bottom_right,
                                                       wl_fixed_t bottom_left) {
  if (width < 0 || height < 0 || top_left < 0 || bottom_left < 0 ||
      bottom_right < 0 || top_right < 0) {
    wl_resource_post_error(resource, AUGMENTED_SURFACE_ERROR_BAD_VALUE,
                           "The size and corners must have positive values "
                           "(%d, %d, %d, %d, %d, %d)",
                           width, height, top_left, top_right, bottom_right,
                           bottom_left);
    return;
  }

  // Rounded corners on local surface coordinates is supported since version 9.
  if (wl_resource_get_version(resource) < 9) {
    LOG(ERROR) << "Rounded corners clip bounds are set on the root surface "
               << "coordinates which is deperecated. Use 9 or newer version "
               << "for surface augmenter.";
  }

  GetUserDataAs<AugmentedSurface>(resource)->SetCorners(
      wl_fixed_to_double(x), wl_fixed_to_double(y), wl_fixed_to_double(width),
      wl_fixed_to_double(height), wl_fixed_to_double(top_left),
      wl_fixed_to_double(top_right), wl_fixed_to_double(bottom_right),
      wl_fixed_to_double(bottom_left));
}

void augmented_surface_set_clip_rect(wl_client* client,
                                     wl_resource* resource,
                                     wl_fixed_t x,
                                     wl_fixed_t y,
                                     wl_fixed_t width,
                                     wl_fixed_t height) {
  GetUserDataAs<AugmentedSurface>(resource)->SetClipRect(
      wl_fixed_to_double(x), wl_fixed_to_double(y), wl_fixed_to_double(width),
      wl_fixed_to_double(height));
}

void augmented_surface_set_frame_trace_id(wl_client* client,
                                          wl_resource* resource,
                                          uint32_t id_hi,
                                          uint32_t id_lo) {
  base::CheckedNumeric<int64_t> id(id_hi);
  id <<= 32;
  id += id_lo;

  if (!id.IsValid()) {
    wl_resource_post_error(
        resource, AUGMENTED_SURFACE_ERROR_BAD_VALUE,
        "The frame trace ID cannot be converted to a valid int64_t (%u, %u)",
        id_hi, id_lo);
    return;
  }

  GetUserDataAs<AugmentedSurface>(resource)->SetFrameTraceId(id.ValueOrDie());
}

const struct augmented_surface_interface augmented_implementation = {
    augmented_surface_destroy,
    augmented_surface_set_corners_DEPRECATED,
    augmented_surface_set_destination_size,
    augmented_surface_set_rounded_corners_bounds_DEPRECATED,
    augmented_surface_set_background_color,
    augmented_surface_set_trusted_damage_DEPRECATED,
    augmented_surface_set_rounded_corners_clip_bounds,
    augmented_surface_set_clip_rect,
    augmented_surface_set_frame_trace_id,
};

////////////////////////////////////////////////////////////////////////////////
// augmented_sub_surface_interface:

// Implements the augmenter interface to a Surface. The "augmented"-state is set
// to null upon destruction. A window property will be set during the lifetime
// of this class to prevent multiple instances from being created for the same
// Surface.
class AugmentedSubSurface : public SubSurfaceObserver {
 public:
  explicit AugmentedSubSurface(SubSurface* sub_surface)
      : sub_surface_(sub_surface) {
    sub_surface_->AddSubSurfaceObserver(this);
    sub_surface_->SetProperty(kSubSurfaceHasAugmentedSubSurfaceKey, true);
  }
  AugmentedSubSurface(const AugmentedSubSurface&) = delete;
  AugmentedSubSurface& operator=(const AugmentedSubSurface&) = delete;
  ~AugmentedSubSurface() override {
    if (sub_surface_) {
      sub_surface_->SetProperty(kSubSurfaceHasAugmentedSubSurfaceKey, false);
      sub_surface_->RemoveSubSurfaceObserver(this);
    }
  }

  void SetPosition(float x, float y) {
    sub_surface_->SetPosition(gfx::PointF(x, y));
  }

  void SetTransform(const gfx::Transform& transform) {
    sub_surface_->SetTransform(transform);
  }

  // SurfaceObserver:
  void OnSubSurfaceDestroying(SubSurface* sub_surface) override {
    sub_surface->RemoveSubSurfaceObserver(this);
    sub_surface_ = nullptr;
  }

 private:
  raw_ptr<SubSurface> sub_surface_;
};

void augmented_sub_surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void augmented_sub_surface_set_position(wl_client* client,
                                        wl_resource* resource,
                                        wl_fixed_t x,
                                        wl_fixed_t y) {
  GetUserDataAs<AugmentedSubSurface>(resource)->SetPosition(
      wl_fixed_to_double(x), wl_fixed_to_double(y));
}

void augmented_sub_surface_set_clip_rect_DEPRECATED(wl_client* client,
                                                    wl_resource* resource,
                                                    wl_fixed_t x,
                                                    wl_fixed_t y,
                                                    wl_fixed_t width,
                                                    wl_fixed_t height) {
  LOG(WARNING) << "Deprecated. The server doesn't support this request.";
}

void augmented_sub_surface_set_transform(wl_client* client,
                                         wl_resource* resource,
                                         wl_array* matrix_data) {
  gfx::Transform transform;
  // Empty data represents the identity matrix.
  if (matrix_data->size == 6 * sizeof(float)) {
    // | a c x |
    // | b d y | -> float[6] { a b c d x y }
    float* data = reinterpret_cast<float*>(matrix_data->data);
    // If b and c are 0, make a simplified transform using AxisTransform2d.
    if (data[1] == 0 && data[2] == 0) {
      transform = gfx::Transform(gfx::AxisTransform2d::FromScaleAndTranslation(
          gfx::Vector2dF(data[0], data[3]), gfx::Vector2dF(data[4], data[5])));
    } else {
      transform = gfx::Transform::Affine(data[0], data[1], data[2], data[3],
                                         data[4], data[5]);
    }
  } else if (matrix_data->size != 0) {
    wl_resource_post_error(resource, AUGMENTED_SUB_SURFACE_ERROR_INVALID_SIZE,
                           "The matrix must contain 0 or 6 %zu-byte floats "
                           "(%zu bytes given)",
                           sizeof(float), matrix_data->size);
    return;
  }

  GetUserDataAs<AugmentedSubSurface>(resource)->SetTransform(transform);
}

const struct augmented_sub_surface_interface
    augmented_sub_surface_implementation = {
        augmented_sub_surface_destroy, augmented_sub_surface_set_position,
        augmented_sub_surface_set_clip_rect_DEPRECATED,
        augmented_sub_surface_set_transform};

////////////////////////////////////////////////////////////////////////////////
// wl_buffer_interface:

void buffer_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_buffer_interface buffer_implementation = {buffer_destroy};

////////////////////////////////////////////////////////////////////////////////
// surface_augmenter_interface:

void augmenter_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void HandleBufferReleaseCallback(wl_resource* resource) {
  wl_buffer_send_release(resource);
  wl_client_flush(wl_resource_get_client(resource));
}

void augmenter_create_solid_color_buffer(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t id,
                                         wl_array* color_data,
                                         int width,
                                         int height) {
  float* data = reinterpret_cast<float*>(color_data->data);
  SkColor4f color = {data[0], data[1], data[2], data[3]};
  std::unique_ptr<SolidColorBuffer> buffer =
      std::make_unique<SolidColorBuffer>(color, gfx::Size(width, height));
  wl_resource* buffer_resource = wl_resource_create(
      client, &wl_buffer_interface, wl_resource_get_version(resource), id);

  buffer->set_release_callback(base::BindRepeating(
      &HandleBufferReleaseCallback, base::Unretained(buffer_resource)));

  SetImplementation(buffer_resource, &buffer_implementation, std::move(buffer));
}

void augmenter_get_augmented_surface(wl_client* client,
                                     wl_resource* resource,
                                     uint32_t id,
                                     wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->is_augmented()) {
    wl_resource_post_error(resource,
                           SURFACE_AUGMENTER_ERROR_AUGMENTED_SURFACE_EXISTS,
                           "an augmenter for that surface already exists");
    return;
  }

  wl_resource* augmented_resource =
      wl_resource_create(client, &augmented_surface_interface,
                         wl_resource_get_version(resource), id);

  SetImplementation(augmented_resource, &augmented_implementation,
                    std::make_unique<AugmentedSurface>(surface));
}

void augmenter_get_augmented_sub_surface(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t id,
                                         wl_resource* sub_surface_resource) {
  SubSurface* sub_surface = GetUserDataAs<SubSurface>(sub_surface_resource);
  if (sub_surface->GetProperty(kSubSurfaceHasAugmentedSubSurfaceKey)) {
    wl_resource_post_error(resource,
                           SURFACE_AUGMENTER_ERROR_AUGMENTED_SURFACE_EXISTS,
                           "an augmenter for that sub-surface already exists");
    return;
  }

  wl_resource* augmented_resource =
      wl_resource_create(client, &augmented_sub_surface_interface,
                         wl_resource_get_version(resource), id);

  SetImplementation(augmented_resource, &augmented_sub_surface_implementation,
                    std::make_unique<AugmentedSubSurface>(sub_surface));
}

const struct surface_augmenter_interface augmenter_implementation = {
    augmenter_destroy, augmenter_create_solid_color_buffer,
    augmenter_get_augmented_surface, augmenter_get_augmented_sub_surface};

}  // namespace

void bind_surface_augmenter(wl_client* client,
                            void* data,
                            uint32_t version,
                            uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &surface_augmenter_interface,
                         std::min(version, kSurfaceAugmenterVersion), id);

  wl_resource_set_implementation(resource, &augmenter_implementation, data,
                                 nullptr);
}

}  // namespace exo::wayland
