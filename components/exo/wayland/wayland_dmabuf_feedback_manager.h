// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_DMABUF_FEEDBACK_MANAGER_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_DMABUF_FEEDBACK_MANAGER_H_

#include <stdint.h>
#include <sys/types.h>
#include <map>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "components/exo/surface_observer.h"

struct wl_client;
struct wl_resource;

namespace exo {

class Display;

namespace wayland {

namespace {
class WaylandDmabufFeedback;
class WaylandDmabufFeedbackTranche;
class WaylandDmabufSurfaceFeedback;
}  // namespace

using IndexedDrmFormatsAndModifiers =
    base::flat_map<uint32_t, base::flat_map<size_t, uint64_t>>;

enum class ScanoutReasonFlags : uint32_t {
  kNone = 0,
  kFullscreen = 1,
  kOverlayPriorityHint = 2
};

class WaylandDmabufFeedbackManager {
 public:
  explicit WaylandDmabufFeedbackManager(Display* display);

  WaylandDmabufFeedbackManager(const WaylandDmabufFeedbackManager&) = delete;
  WaylandDmabufFeedbackManager& operator=(const WaylandDmabufFeedbackManager&) =
      delete;

  ~WaylandDmabufFeedbackManager();

  Display* GetDisplay() { return display_; }
  uint32_t GetVersionSupportedByPlatform() const { return version_; }

  bool IsFormatSupported(uint32_t format) const;
  void SendFormatsAndModifiers(wl_resource* resource) const;
  void GetDefaultFeedback(wl_client* client,
                          wl_resource* dma_buf_resource,
                          uint32_t feedback_id);
  void GetSurfaceFeedback(wl_client* client,
                          wl_resource* dma_buf_resource,
                          uint32_t feedback_id,
                          wl_resource* surface_resource);
  void RemoveSurfaceFeedback(Surface* surface);

  void AddSurfaceToScanoutCandidates(Surface* surface,
                                     ScanoutReasonFlags reason);
  void RemoveSurfaceFromScanoutCandidates(Surface* surface,
                                          ScanoutReasonFlags reason);
  void MaybeResendFeedback(Surface* surface);

 private:
  void SendFeedback(WaylandDmabufFeedback* feedback, wl_resource* resource);
  void SendTranche(const WaylandDmabufFeedbackTranche* tranche,
                   wl_resource* resource);

  const raw_ptr<Display> display_;
  uint32_t version_;
  IndexedDrmFormatsAndModifiers drm_formats_and_modifiers_;
  std::unique_ptr<base::ReadOnlySharedMemoryRegion> shared_memory_region_;
  std::unique_ptr<WaylandDmabufFeedback> default_feedback_;
  base::flat_map<Surface*, std::unique_ptr<WaylandDmabufSurfaceFeedback>>
      surface_feedbacks_;
  std::map<Surface*, ScanoutReasonFlags> scanout_candidates_;
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_DMABUF_FEEDBACK_MANAGER_H_
