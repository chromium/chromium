// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/wayland/wayland_dmabuf_feedback_manager.h"

#include <bits/types.h>
#include <drm_fourcc.h>
#include <linux-dmabuf-unstable-v1-server-protocol.h>
#include <sys/stat.h>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "components/exo/buffer.h"
#include "components/exo/display.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wayland/server_util.h"
#include "components/viz/common/gpu/context_provider.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/linux/drm_util_linux.h"

namespace exo {
namespace wayland {
namespace {

enum TrancheFlags : uint32_t { kNone = 0, kScanout = 1 };

struct WaylandDmabufFeedbackFormat {
  uint32_t format;
  uint32_t padding;
  uint64_t modifier;
};

void feedbackDestroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zwp_linux_dmabuf_feedback_v1_interface feedback_implementation = {
    feedbackDestroy,
};

// A tranche links to a subset of formats and modifiers which are supported on a
// specific device for a specific task. The most common use cases are the
// "default" tranche (targeting the main device, advertising all formats and
// modifiers supported by the compositor, for non-scanout cases) and "scanout"
// tranches (advertising the formats and modifiers on a specific device that, if
// used by the client, allows the compositor to offload some work to the
// hardware). "scanout" tranches should usually only get advertised by the
// compositor if there's a reasonable chance that composition can be skipped -
// e.g. when a surface is in fullscreen mode.
class WaylandDmabufFeedbackTranche {
 public:
  explicit WaylandDmabufFeedbackTranche(
      dev_t target_device_id,
      TrancheFlags flags,
      IndexedDrmFormatsAndModifiers drm_formats_and_modifiers)
      : target_device_id_(target_device_id),
        flags_(flags),
        drm_formats_and_modifiers_(drm_formats_and_modifiers) {}
  explicit WaylandDmabufFeedbackTranche(
      const std::unique_ptr<WaylandDmabufFeedbackTranche>& other)
      : target_device_id_(other->target_device_id_),
        flags_(other->flags_),
        drm_formats_and_modifiers_(other->drm_formats_and_modifiers_) {}

  WaylandDmabufFeedbackTranche(const WaylandDmabufFeedbackTranche&) = delete;
  WaylandDmabufFeedbackTranche& operator=(const WaylandDmabufFeedbackTranche&) =
      delete;

  ~WaylandDmabufFeedbackTranche() = default;

  dev_t GetTargetDeviceId() const { return target_device_id_; }
  TrancheFlags GetFlags() const { return flags_; }

  const IndexedDrmFormatsAndModifiers GetFormatsAndModifiers() const {
    return drm_formats_and_modifiers_;
  }

 private:
  const dev_t target_device_id_;
  const TrancheFlags flags_;
  const IndexedDrmFormatsAndModifiers drm_formats_and_modifiers_;
};

// A feedback is a set of information that is send to the client. It consists of
// a main device (the device the compositor uses for rendering), a format table
// (containing all formats and modifiers that may be advertised in one of the
// tranches, including those not supported by the main device) and one or more
// tranches (there must always be at least one for the main device). The
// compositor may choose to update a feedback at any time if necessary. The
// provided information should allow the client to pick the optimal combination
// of device, format and modifier for any given situation.
// A feedback object may be reused for several clients/requests in order to
// minimize resource usage.
// TODO: Updating a feedback is not yet implemented.
class WaylandDmabufFeedback {
 public:
  explicit WaylandDmabufFeedback(
      dev_t device_id,
      std::unique_ptr<WaylandDmabufFeedbackTranche> default_tranche)
      : main_device_id_(device_id),
        default_tranche_(std::move(default_tranche)) {}
  explicit WaylandDmabufFeedback(
      const std::unique_ptr<WaylandDmabufFeedback>& other)
      : main_device_id_(other->main_device_id_),
        default_tranche_(std::make_unique<WaylandDmabufFeedbackTranche>(
            other->default_tranche_)) {
    if (other->scanout_tranche_)
      scanout_tranche_ = std::make_unique<WaylandDmabufFeedbackTranche>(
          other->scanout_tranche_);
  }

  WaylandDmabufFeedback(const WaylandDmabufFeedback&) = delete;
  WaylandDmabufFeedback& operator=(const WaylandDmabufFeedback&) = delete;

  ~WaylandDmabufFeedback() = default;

  dev_t GetMainDeviceId() const { return main_device_id_; }
  const WaylandDmabufFeedbackTranche* GetDefaultTranche() const {
    return default_tranche_.get();
  }
  const WaylandDmabufFeedbackTranche* GetScanoutTranche() const {
    return scanout_tranche_.get();
  }

  void MaybeAddScanoutTranche(Surface* surface) {
    DCHECK(!scanout_tranche_);

    const display::Display surface_display = surface->GetDisplay();
    display::DrmFormatsAndModifiers display_formats_and_modifiers =
        ash::Shell::Get()
            ->display_manager()
            ->GetDisplayInfo(surface_display.id())
            .GetDRMFormatsAndModifiers();
    IndexedDrmFormatsAndModifiers scanout_formats_and_modifiers;

    for (const auto& [format, modifier_entries] :
         default_tranche_->GetFormatsAndModifiers()) {
      if (base::Contains(display_formats_and_modifiers, format)) {
        base::flat_map<size_t, uint64_t> scanout_modifier_entries;

        if (modifier_entries.size() == 1) {
          auto it = modifier_entries.begin();
          DCHECK(it->second == DRM_FORMAT_MOD_INVALID);
          scanout_modifier_entries.emplace(it->first, it->second);
        } else {
          for (const auto& [table_index, modifier] : modifier_entries) {
            if (base::Contains(display_formats_and_modifiers.at(format),
                               modifier)) {
              scanout_modifier_entries.emplace(table_index, modifier);
            }
          }
        }

        if (!scanout_modifier_entries.empty())
          scanout_formats_and_modifiers.emplace(format,
                                                scanout_modifier_entries);
      }
    }
    if (scanout_formats_and_modifiers.empty())
      return;

    scanout_tranche_ = std::make_unique<WaylandDmabufFeedbackTranche>(
        main_device_id_, TrancheFlags::kScanout, scanout_formats_and_modifiers);
  }

  void ClearScanoutTranche() {
    DCHECK(scanout_tranche_);
    scanout_tranche_ = nullptr;
  }

 private:
  const dev_t main_device_id_;
  const std::unique_ptr<WaylandDmabufFeedbackTranche> default_tranche_;
  std::unique_ptr<WaylandDmabufFeedbackTranche> scanout_tranche_;
};

class WaylandDmabufSurfaceFeedbackResourceWrapper;

// A surface feedback can be requested by a client to get an optimized feedback
// for a specific surface - most importantly if the client wants to receive
// "scanout" tranches when possible. A client may request the surface feedback
// multiple times for the same surface (e.g. implicitly via Mesa and explicitly
// in the application). Thus a surface feedback can have multiple resource
// objects.
class WaylandDmabufSurfaceFeedback : public SurfaceObserver {
 public:
  explicit WaylandDmabufSurfaceFeedback(
      WaylandDmabufFeedbackManager* feedback_manager,
      Surface* surface,
      std::unique_ptr<WaylandDmabufFeedback> feedback)
      : feedback_manager_(feedback_manager),
        surface_(surface),
        feedback_(std::move(feedback)) {
    surface_->AddSurfaceObserver(this);
  }

  WaylandDmabufSurfaceFeedback(const WaylandDmabufSurfaceFeedback&) = delete;
  WaylandDmabufSurfaceFeedback& operator=(const WaylandDmabufSurfaceFeedback&) =
      delete;

  ~WaylandDmabufSurfaceFeedback() override;

  void OnSurfaceDestroying(Surface* surface) override {
    feedback_manager_->RemoveSurfaceFromScanoutCandidates(
        surface_, ScanoutReasonFlags::kNone);
    feedback_manager_->RemoveSurfaceFeedback(surface_);
  }

  void OnDisplayChanged(Surface* surface,
                        int64_t old_display,
                        int64_t new_display) override {
    feedback_manager_->MaybeResendFeedback(surface_);
  }

  void OnFullscreenStateChanged(bool fullscreen) override {
    if (fullscreen) {
      feedback_manager_->AddSurfaceToScanoutCandidates(
          surface_, ScanoutReasonFlags::kFullscreen);
    } else {
      feedback_manager_->RemoveSurfaceFromScanoutCandidates(
          surface_, ScanoutReasonFlags::kFullscreen);
    }
  }

  void OnOverlayPriorityHintChanged(
      OverlayPriority overlay_priority_hint) override {
    if (overlay_priority_hint == OverlayPriority::REQUIRED) {
      feedback_manager_->AddSurfaceToScanoutCandidates(
          surface_, ScanoutReasonFlags::kOverlayPriorityHint);
    } else {
      feedback_manager_->RemoveSurfaceFromScanoutCandidates(
          surface_, ScanoutReasonFlags::kOverlayPriorityHint);
    }
  }

  void AddSurfaceFeedbackRef(
      WaylandDmabufSurfaceFeedbackResourceWrapper* surface_feedback_ref) {
    surface_feedback_refs_.insert(surface_feedback_ref);
  }
  void OnSurfaceFeedbackRefDestroyed(
      WaylandDmabufSurfaceFeedbackResourceWrapper* surface_feedback_ref) {
    DCHECK(base::Contains(surface_feedback_refs_, surface_feedback_ref));
    surface_feedback_refs_.erase(surface_feedback_ref);
    if (surface_feedback_refs_.empty())
      feedback_manager_->RemoveSurfaceFeedback(surface_);
  }

  Surface* GetSurface() { return surface_; }
  WaylandDmabufFeedback* GetFeedback() { return feedback_.get(); }
  std::set<
      raw_ptr<WaylandDmabufSurfaceFeedbackResourceWrapper, SetExperimental>>
  GetFeedbackRefs() {
    return surface_feedback_refs_;
  }

 private:
  const raw_ptr<WaylandDmabufFeedbackManager> feedback_manager_;
  const raw_ptr<Surface> surface_;
  std::unique_ptr<WaylandDmabufFeedback> const feedback_;
  std::set<
      raw_ptr<WaylandDmabufSurfaceFeedbackResourceWrapper, SetExperimental>>
      surface_feedback_refs_;
};

// Simple helper class to use a surface feedback with multiple resource objects
// when using SetImplementation()
class WaylandDmabufSurfaceFeedbackResourceWrapper {
 public:
  explicit WaylandDmabufSurfaceFeedbackResourceWrapper(
      WaylandDmabufSurfaceFeedback* surface_feedback,
      wl_resource* resource)
      : surface_feedback_(surface_feedback), resource_(resource) {
    surface_feedback->AddSurfaceFeedbackRef(this);
  }

  WaylandDmabufSurfaceFeedbackResourceWrapper(
      const WaylandDmabufSurfaceFeedbackResourceWrapper&) = delete;
  WaylandDmabufSurfaceFeedbackResourceWrapper& operator=(
      const WaylandDmabufSurfaceFeedbackResourceWrapper&) = delete;

  ~WaylandDmabufSurfaceFeedbackResourceWrapper() {
    if (surface_feedback_)
      surface_feedback_->OnSurfaceFeedbackRefDestroyed(this);
  }

  wl_resource* GetFeedbackResource() { return resource_; }
  void SetInert() { surface_feedback_ = nullptr; }

 private:
  // Dangling when starting Borealis and Steam starts updating.
  raw_ptr<WaylandDmabufSurfaceFeedback, DanglingUntriaged> surface_feedback_;
  raw_ptr<wl_resource> resource_;
};

WaylandDmabufSurfaceFeedback::~WaylandDmabufSurfaceFeedback() {
  for (WaylandDmabufSurfaceFeedbackResourceWrapper* surface_feedback_ref :
       surface_feedback_refs_) {
    surface_feedback_ref->SetInert();
  }
  surface_->RemoveSurfaceObserver(this);
}

}  // namespace

WaylandDmabufFeedbackManager::WaylandDmabufFeedbackManager(Display* display)
    : display_(display) {
  scoped_refptr<viz::RasterContextProvider> context_provider =
      aura::Env::GetInstance()
          ->context_factory()
          ->SharedMainThreadRasterContextProvider();
  gpu::Capabilities caps = context_provider->ContextCapabilities();

  // Intel CCS modifiers leak memory on gbm to gl buffer import. Block these
  // modifiers for now. See crbug.com/1445252, crbug.com/1458575
  // Also blocking |DRM_FORMAT_YVU420| for a specific modifier based a test
  // failing in minigbm. See b/289714323
  const base::flat_set<std::pair<uint64_t, uint64_t>> modifier_block_list = {
      {DRM_FORMAT_INVALID, I915_FORMAT_MOD_Y_TILED_CCS},
      {DRM_FORMAT_INVALID, I915_FORMAT_MOD_Yf_TILED_CCS},
      {DRM_FORMAT_INVALID, I915_FORMAT_MOD_Y_TILED_GEN12_RC_CCS},
      {DRM_FORMAT_INVALID, I915_FORMAT_MOD_Y_TILED_GEN12_MC_CCS},
      {DRM_FORMAT_INVALID, I915_FORMAT_MOD_Y_TILED_GEN12_RC_CCS_CC},
      {DRM_FORMAT_INVALID, I915_FORMAT_MOD_4_TILED_DG2_RC_CCS},
      {DRM_FORMAT_INVALID, I915_FORMAT_MOD_4_TILED_DG2_MC_CCS},
      {DRM_FORMAT_INVALID, I915_FORMAT_MOD_4_TILED_DG2_RC_CCS_CC},
      {DRM_FORMAT_YVU420, DRM_FORMAT_MOD_QCOM_COMPRESSED}};

  size_t format_table_index = 0;
  for (const auto& [drm_format, modifiers] : caps.drm_formats_and_modifiers) {
    if (!ui::IsValidBufferFormat(drm_format))
      continue;

    if (!caps.gpu_memory_buffer_formats.Has(
            ui::GetBufferFormatFromFourCCFormat(drm_format))) {
      continue;
    }

    base::flat_map<size_t, uint64_t> modifier_entries;
    modifier_entries.emplace(format_table_index++, DRM_FORMAT_MOD_INVALID);

    if (base::FeatureList::IsEnabled(ash::features::kExoLinuxDmabufModifiers)) {
      for (uint64_t modifier : modifiers) {
        // Check for generic blocking first then format specific blocking.
        if (!modifier_block_list.contains({DRM_FORMAT_INVALID, modifier}) &&
            !modifier_block_list.contains({drm_format, modifier})) {
          modifier_entries.emplace(format_table_index++, modifier);
        }
      }
    }

    drm_formats_and_modifiers_.emplace(drm_format, modifier_entries);
  }
  if (drm_formats_and_modifiers_.empty()) {
    // Fallback path, to be removed ASAP. We should not advertise the protocol
    // at all.
    gpu::GpuMemoryBufferFormatSet format_set = caps.gpu_memory_buffer_formats;
    for (int i = 0; i <= static_cast<int>(gfx::BufferFormat::LAST); i++) {
      gfx::BufferFormat buffer_format = static_cast<gfx::BufferFormat>(i);
      if (format_set.Has(buffer_format)) {
        int drm_format = ui::GetFourCCFormatFromBufferFormat(buffer_format);
        if (ui::IsValidBufferFormat(drm_format)) {
          base::flat_map<size_t, uint64_t> modifier_entries;
          modifier_entries.emplace(format_table_index++,
                                   DRM_FORMAT_MOD_INVALID);
          drm_formats_and_modifiers_.emplace(drm_format, modifier_entries);
        }
      }
    }
    version_ = ZWP_LINUX_BUFFER_PARAMS_V1_CREATE_IMMED_SINCE_VERSION;
    return;
  }

  if (!base::FeatureList::IsEnabled(ash::features::kExoLinuxDmabufV3) &&
      !base::FeatureList::IsEnabled(ash::features::kExoLinuxDmabufV4)) {
    version_ = ZWP_LINUX_BUFFER_PARAMS_V1_CREATE_IMMED_SINCE_VERSION;
    return;
  }

  if (!base::FeatureList::IsEnabled(ash::features::kExoLinuxDmabufV4) ||
      !caps.drm_device_id) {
    version_ = ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION;
    return;
  }

  auto tranche = std::make_unique<WaylandDmabufFeedbackTranche>(
      caps.drm_device_id, TrancheFlags::kNone, drm_formats_and_modifiers_);
  default_feedback_ = std::make_unique<WaylandDmabufFeedback>(
      caps.drm_device_id, std::move(tranche));

  size_t size = sizeof(WaylandDmabufFeedbackFormat) * format_table_index;
  base::MappedReadOnlyRegion mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(size);
  CHECK(mapped_region.IsValid());

  shared_memory_region_ = std::make_unique<base::ReadOnlySharedMemoryRegion>(
      std::move(mapped_region.region));

  WaylandDmabufFeedbackFormat* format_table =
      static_cast<WaylandDmabufFeedbackFormat*>(mapped_region.mapping.memory());

  for (const auto& [format, modifier_entries] : drm_formats_and_modifiers_) {
    for (const auto& [table_index, modifier] : modifier_entries) {
      format_table[table_index].format = format;
      format_table[table_index].modifier = modifier;
    }
  }

  version_ = ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION;
}

WaylandDmabufFeedbackManager::~WaylandDmabufFeedbackManager() = default;

bool WaylandDmabufFeedbackManager::IsFormatSupported(uint32_t format) const {
  return base::Contains(drm_formats_and_modifiers_, format);
}

void WaylandDmabufFeedbackManager::SendFormatsAndModifiers(
    wl_resource* resource) const {
  for (const auto& [format, modifier_entries] : drm_formats_and_modifiers_) {
    zwp_linux_dmabuf_v1_send_format(resource, format);
    if (wl_resource_get_version(resource) >=
        ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION) {
      for (const auto& [table_index, modifier] : modifier_entries) {
        zwp_linux_dmabuf_v1_send_modifier(resource, format, modifier >> 32,
                                          modifier & 0xffffffff);
      }
    }
  }
}

void WaylandDmabufFeedbackManager::GetDefaultFeedback(
    wl_client* client,
    wl_resource* dma_buf_resource,
    uint32_t feedback_id) {
  wl_resource* feedback_resource = wl_resource_create(
      client, &zwp_linux_dmabuf_feedback_v1_interface,
      wl_resource_get_version(dma_buf_resource), feedback_id);

  SetImplementation(feedback_resource, &feedback_implementation);

  SendFeedback(default_feedback_.get(), feedback_resource);
}

void WaylandDmabufFeedbackManager::GetSurfaceFeedback(
    wl_client* client,
    wl_resource* dma_buf_resource,
    uint32_t feedback_id,
    wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);

  auto it = surface_feedbacks_.find(surface);
  if (it == surface_feedbacks_.end()) {
    auto defaut_feedback_copy =
        std::make_unique<WaylandDmabufFeedback>(default_feedback_);
    auto new_surface_feedback = std::make_unique<WaylandDmabufSurfaceFeedback>(
        this, surface, std::move(defaut_feedback_copy));

    ShellSurfaceBase* shell_surface_base = nullptr;
    for (auto* window = surface->window(); window && !shell_surface_base;
         window = window->parent()) {
      shell_surface_base = GetShellSurfaceBaseForWindow(window);
    }
    if (shell_surface_base) {
      bool fullscreen = shell_surface_base->GetWidget()->IsFullscreen();
      new_surface_feedback->OnFullscreenStateChanged(fullscreen);
    }

    new_surface_feedback->OnOverlayPriorityHintChanged(
        surface->GetOverlayPriorityHint());

    it = surface_feedbacks_.emplace_hint(it, surface,
                                         std::move(new_surface_feedback));
  }
  WaylandDmabufSurfaceFeedback* surface_feedback = it->second.get();
  DCHECK(surface_feedback);

  wl_resource* feedback_resource = wl_resource_create(
      client, &zwp_linux_dmabuf_feedback_v1_interface,
      wl_resource_get_version(dma_buf_resource), feedback_id);

  auto surface_feedback_ref =
      std::make_unique<WaylandDmabufSurfaceFeedbackResourceWrapper>(
          surface_feedback, feedback_resource);
  SetImplementation(feedback_resource, &feedback_implementation,
                    std::move(surface_feedback_ref));

  auto* feedback = surface_feedback->GetFeedback();
  if (base::Contains(scanout_candidates_, surface))
    feedback->MaybeAddScanoutTranche(surface);

  SendFeedback(feedback, feedback_resource);
}

void WaylandDmabufFeedbackManager::RemoveSurfaceFeedback(Surface* surface) {
  DCHECK(base::Contains(surface_feedbacks_, surface));
  surface_feedbacks_.erase(surface);
}

void WaylandDmabufFeedbackManager::AddSurfaceToScanoutCandidates(
    Surface* surface,
    ScanoutReasonFlags reason) {
  auto search = scanout_candidates_.find(surface);
  if (search != scanout_candidates_.end()) {
    search->second = static_cast<ScanoutReasonFlags>(
        static_cast<uint32_t>(search->second) | static_cast<uint32_t>(reason));
    return;
  }

  scanout_candidates_.emplace(surface, reason);

  if (!base::Contains(surface_feedbacks_, surface)) {
    return;
  }

  const auto& surface_feedback = surface_feedbacks_[surface];
  auto* feedback = surface_feedback->GetFeedback();
  if (feedback->GetScanoutTranche()) {
    return;
  }

  feedback->MaybeAddScanoutTranche(surface);
  if (!feedback->GetScanoutTranche()) {
    return;
  }

  for (WaylandDmabufSurfaceFeedbackResourceWrapper* feedback_ref :
       surface_feedback->GetFeedbackRefs()) {
    SendFeedback(feedback, feedback_ref->GetFeedbackResource());
  }
}

void WaylandDmabufFeedbackManager::RemoveSurfaceFromScanoutCandidates(
    Surface* surface,
    ScanoutReasonFlags reason) {
  auto search = scanout_candidates_.find(surface);
  if (search == scanout_candidates_.end()) {
    return;
  }

  search->second = static_cast<ScanoutReasonFlags>(
      static_cast<uint32_t>(search->second) & ~static_cast<uint32_t>(reason));
  if (search->second == ScanoutReasonFlags::kNone ||
      reason == ScanoutReasonFlags::kNone) {
    scanout_candidates_.erase(surface);
  } else {
    return;
  }

  if (!base::Contains(surface_feedbacks_, surface)) {
    return;
  }

  const auto& surface_feedback = surface_feedbacks_[surface];
  auto* feedback = surface_feedback->GetFeedback();
  if (!feedback->GetScanoutTranche()) {
    return;
  }

  feedback->ClearScanoutTranche();
  for (WaylandDmabufSurfaceFeedbackResourceWrapper* feedback_ref :
       surface_feedback->GetFeedbackRefs()) {
    SendFeedback(feedback, feedback_ref->GetFeedbackResource());
  }
}

void WaylandDmabufFeedbackManager::MaybeResendFeedback(Surface* surface) {
  if (!base::Contains(scanout_candidates_, surface) ||
      !base::Contains(surface_feedbacks_, surface)) {
    return;
  }

  const auto& surface_feedback = surface_feedbacks_[surface];
  auto* feedback = surface_feedback->GetFeedback();
  feedback->MaybeAddScanoutTranche(surface);

  for (WaylandDmabufSurfaceFeedbackResourceWrapper* feedback_ref :
       surface_feedback->GetFeedbackRefs()) {
    SendFeedback(feedback, feedback_ref->GetFeedbackResource());
  }
}

void WaylandDmabufFeedbackManager::SendFeedback(WaylandDmabufFeedback* feedback,
                                                wl_resource* resource) {
  wl_array main_device_buf;
  wl_array_init(&main_device_buf);
  dev_t* device_id_ptr =
      static_cast<dev_t*>(wl_array_add(&main_device_buf, sizeof(dev_t)));
  *device_id_ptr = feedback->GetMainDeviceId();
  zwp_linux_dmabuf_feedback_v1_send_main_device(resource, &main_device_buf);
  wl_array_release(&main_device_buf);

  base::subtle::FDPair fd_pair = shared_memory_region_->GetPlatformHandle();
  zwp_linux_dmabuf_feedback_v1_send_format_table(
      resource, fd_pair.fd, shared_memory_region_->GetSize());

  if (feedback->GetScanoutTranche())
    SendTranche(feedback->GetScanoutTranche(), resource);

  SendTranche(feedback->GetDefaultTranche(), resource);

  zwp_linux_dmabuf_feedback_v1_send_done(resource);
}

void WaylandDmabufFeedbackManager::SendTranche(
    const WaylandDmabufFeedbackTranche* tranche,
    wl_resource* resource) {
  wl_array target_device_buf;
  wl_array_init(&target_device_buf);
  dev_t* device_id_ptr =
      static_cast<dev_t*>(wl_array_add(&target_device_buf, sizeof(dev_t)));
  *device_id_ptr = tranche->GetTargetDeviceId();
  zwp_linux_dmabuf_feedback_v1_send_tranche_target_device(resource,
                                                          &target_device_buf);
  wl_array_release(&target_device_buf);

  zwp_linux_dmabuf_feedback_v1_send_tranche_flags(resource,
                                                  tranche->GetFlags());

  wl_array formats_array;
  wl_array_init(&formats_array);
  for (const auto& [format, modifier_entries] :
       tranche->GetFormatsAndModifiers()) {
    for (const auto& [table_index, modifier] : modifier_entries) {
      uint16_t* format_index_ptr = static_cast<uint16_t*>(
          wl_array_add(&formats_array, sizeof(uint16_t)));
      *format_index_ptr = table_index;
    }
  }
  zwp_linux_dmabuf_feedback_v1_send_tranche_formats(resource, &formats_array);
  wl_array_release(&formats_array);

  zwp_linux_dmabuf_feedback_v1_send_tranche_done(resource);
}

}  // namespace wayland
}  // namespace exo
