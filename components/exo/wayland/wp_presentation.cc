// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wp_presentation.h"

#include <presentation-time-server-protocol.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "components/exo/wayland/server_util.h"
#include "ui/gfx/presentation_feedback.h"

namespace exo {
namespace wayland {
namespace {

////////////////////////////////////////////////////////////////////////////////
// presentation_interface:

void HandleSurfacePresentationCallback(
    wl_resource* resource,
    const gfx::PresentationFeedback& feedback) {
  if (feedback.timestamp.is_null()) {
    wp_presentation_feedback_send_discarded(resource);
  } else {
    int64_t presentation_time_us = feedback.timestamp.ToInternalValue();
    int64_t seconds = presentation_time_us / base::Time::kMicrosecondsPerSecond;
    int64_t microseconds =
        presentation_time_us % base::Time::kMicrosecondsPerSecond;
    static_assert(
        static_cast<uint32_t>(gfx::PresentationFeedback::Flags::kVSync) ==
            static_cast<uint32_t>(WP_PRESENTATION_FEEDBACK_KIND_VSYNC),
        "gfx::PresentationFlags::VSync don't match!");
    static_assert(
        static_cast<uint32_t>(gfx::PresentationFeedback::Flags::kHWClock) ==
            static_cast<uint32_t>(WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK),
        "gfx::PresentationFlags::HWClock don't match!");
    static_assert(
        static_cast<uint32_t>(
            gfx::PresentationFeedback::Flags::kHWCompletion) ==
            static_cast<uint32_t>(WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION),
        "gfx::PresentationFlags::HWCompletion don't match!");
    static_assert(
        static_cast<uint32_t>(gfx::PresentationFeedback::Flags::kZeroCopy) ==
            static_cast<uint32_t>(WP_PRESENTATION_FEEDBACK_KIND_ZERO_COPY),
        "gfx::PresentationFlags::ZeroCopy don't match!");
    wp_presentation_feedback_send_presented(
        resource, seconds >> 32, seconds & 0xffffffff,
        microseconds * base::Time::kNanosecondsPerMicrosecond,
        feedback.interval.InMicroseconds() *
            base::Time::kNanosecondsPerMicrosecond,
        0, 0, feedback.flags);
  }
  wl_client_flush(wl_resource_get_client(resource));
}

void presentation_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void presentation_feedback(wl_client* client,
                           wl_resource* resource,
                           wl_resource* surface_resource,
                           uint32_t id) {
  wl_resource* presentation_feedback_resource =
      wl_resource_create(client, &wp_presentation_feedback_interface,
                         wl_resource_get_version(resource), id);

  // base::Unretained is safe as the resource owns the callback.
  auto cancelable_callback = std::make_unique<base::CancelableRepeatingCallback<
      void(const gfx::PresentationFeedback&)>>(
      base::BindRepeating(&HandleSurfacePresentationCallback,
                          base::Unretained(presentation_feedback_resource)));

  GetUserDataAs<Surface>(surface_resource)
      ->RequestPresentationCallback(cancelable_callback->callback());

  SetImplementation(presentation_feedback_resource, nullptr,
                    std::move(cancelable_callback));
}

const struct wp_presentation_interface presentation_implementation = {
    presentation_destroy, presentation_feedback};

}  // namespace

void bind_presentation(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wp_presentation_interface, 1, id);

  wl_resource_set_implementation(resource, &presentation_implementation, data,
                                 nullptr);

  wp_presentation_send_clock_id(resource, CLOCK_MONOTONIC);
}

}  // namespace wayland
}  // namespace exo
