// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_TRANSFERABLE_RESOURCE_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_TRANSFERABLE_RESOURCE_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "build/build_config.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/viz_common_export.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

namespace gpu {
class ClientSharedImage;
}

namespace viz {

struct ReturnedResource;

struct VIZ_COMMON_EXPORT TransferableResource {
  struct VIZ_COMMON_EXPORT MetadataOverride {
    std::optional<SharedImageFormat> format;
    std::optional<gfx::Size> size;
    std::optional<uint32_t> texture_target;
    std::optional<bool> is_overlay_candidate;
    std::optional<gfx::ColorSpace> color_space;
    std::optional<GrSurfaceOrigin> origin;
    std::optional<SkAlphaType> alpha_type;
  };

  enum class SynchronizationType : uint8_t {
    // Commands issued (SyncToken) - a resource can be reused as soon as display
    // compositor issues the latest command on it and SyncToken will be signaled
    // when this happens.
    kSyncToken = 0,
    // Commands completed (aka read lock fence) - If a gpu resource is backed by
    // a GpuMemoryBuffer, then it will be accessed out-of-band, and a gpu fence
    // needs to be waited on before the resource is returned and reused. In
    // other words, the resource will be returned only when gpu commands are
    // completed.
    kGpuCommandsCompleted,
    // Commands submitted (release fence) - a resource will be returned after
    // gpu service submitted commands to the gpu and provide the fence.
    kReleaseFence,
  };

  // Differentiates between the various sources that create a resource. They
  // have different lifetime expectations, and we want to be able to determine
  // which remain after we Evict a Surface.
  //
  // These values are persistent to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ResourceSource : uint8_t {
    kUnknown = 0,
    kAR = 1,
    kCanvas = 2,
    kDrawingBuffer = 3,
    kExoBuffer = 4,
    kHeadsUpDisplay = 5,
    kImageLayerBridge = 6,
    kPPBGraphics3D = 7,
    kPepperGraphics2D = 8,
    kViewTransition = 9,
    kStaleContent = 10,
    kTest = 11,
    kTileRasterTask = 12,
    kUI = 13,
    kVideo = 14,
    kWebGPUSwapBuffer = 15,
  };

  // Creates transferable resource from the ClientSharedImage. `override` allows
  // to temporary override SharedImage metadata to facilitate current
  // discrepancies until they are fixed. Do not pass it in the new code.
  static TransferableResource Make(
      const scoped_refptr<gpu::ClientSharedImage>& shared_image,
      ResourceSource source,
      const gpu::SyncToken& sync_token,
      const MetadataOverride& override = {});

  // Following Make* functions are deprecated. Please use the one above.
  static TransferableResource MakeSoftwareSharedImage(
      const scoped_refptr<gpu::ClientSharedImage>& client_shared_image,
      const gpu::SyncToken& sync_token,
      const gfx::Size& size,
      SharedImageFormat format,
      ResourceSource source = ResourceSource::kUnknown);
  static TransferableResource MakeGpu(
      const gpu::Mailbox& mailbox,
      uint32_t texture_target,
      const gpu::SyncToken& sync_token,
      const gfx::Size& size,
      SharedImageFormat format,
      bool is_overlay_candidate,
      ResourceSource source = ResourceSource::kUnknown);
  static TransferableResource MakeGpu(
      const scoped_refptr<gpu::ClientSharedImage>& client_shared_image,
      uint32_t texture_target,
      const gpu::SyncToken& sync_token,
      const gfx::Size& size,
      SharedImageFormat format,
      bool is_overlay_candidate,
      ResourceSource source = ResourceSource::kUnknown);

  static std::vector<ReturnedResource> ReturnResources(
      const std::vector<TransferableResource>& input);

  TransferableResource();
  ~TransferableResource();

  TransferableResource(const TransferableResource& other);
  TransferableResource& operator=(const TransferableResource& other);

  ReturnedResource ToReturnedResource() const;

  bool is_empty() const { return mailbox().IsZero(); }

  void set_mailbox(const gpu::Mailbox& mailbox) { memory_buffer_id_ = mailbox; }
  void set_sync_token(const gpu::SyncToken& sync_token) {
    sync_token_ = sync_token;
  }
  void set_texture_target(const uint32_t texture_target) {
    texture_target_ = texture_target;
  }
  // For usage only in Mojo serialization/deserialization.
  void set_memory_buffer_id(gpu::Mailbox memory_buffer_id) {
    memory_buffer_id_ = memory_buffer_id;
  }

  // Returns the Mailbox that this instance is storing. Valid to call only if
  // this instance has been created via MakeSoftwareSharedImage() or MakeGpu().
  const gpu::Mailbox& mailbox() const { return memory_buffer_id_; }
  const gpu::SyncToken& sync_token() const { return sync_token_; }
  gpu::SyncToken& mutable_sync_token() { return sync_token_; }
  uint32_t texture_target() const { return texture_target_; }
  // For usage only in Mojo serialization/deserialization.
  const gpu::Mailbox& memory_buffer_id() const { return memory_buffer_id_; }

  bool operator==(const TransferableResource& o) const {
    return id == o.id && is_software == o.is_software && size == o.size &&
           format == o.format && memory_buffer_id_ == o.memory_buffer_id_ &&
           sync_token_ == o.sync_token_ &&
           texture_target_ == o.texture_target_ &&
           color_space == o.color_space && hdr_metadata == o.hdr_metadata &&
           is_overlay_candidate == o.is_overlay_candidate &&
#if BUILDFLAG(IS_ANDROID)
           is_backed_by_surface_view == o.is_backed_by_surface_view &&
           wants_promotion_hint == o.wants_promotion_hint &&
#elif BUILDFLAG(IS_WIN)
           wants_promotion_hint == o.wants_promotion_hint &&
#endif
           synchronization_type == o.synchronization_type &&
           resource_source == o.resource_source;
  }

  // TODO(danakj): Some of these fields are only GL, some are only Software,
  // some are both but used for different purposes (like the mailbox name).
  // It would be nice to group things together and make it more clear when
  // they will be used or not, and provide easier access to fields such as the
  // mailbox that also show the intent for software for GL.
  // An |id| field that can be unique to this resource. For resources
  // generated by compositor clients, this |id| may be used for their
  // own book-keeping but need not be set at all.
  ResourceId id = kInvalidResourceId;

  // Indicates if the resource is gpu or software backed.
  bool is_software = false;

  // The number of pixels in the gpu mailbox/software bitmap.
  gfx::Size size;

  // The format of the pixels in the gpu mailbox/software bitmap. This should
  // almost always be RGBA_8888 for resources generated by compositor clients,
  // and must be RGBA_8888 always for software resources.
  SharedImageFormat format = SinglePlaneFormat::kRGBA_8888;

  // The color space that is used for pixel path operations (e.g, TexImage,
  // CopyTexImage, DrawPixels) and when displaying as an overlay.
  //
  // TODO(b/220336463): On ChromeOS, the color space for hardware decoded video
  // frames is currently specified at the time of creating the SharedImage.
  // Therefore, for the purposes of that use case and compositing, the
  // |color_space| field here is ignored. We should consider using it.
  //
  // TODO(b/233667677): For ChromeOS NV12 hardware overlays, |color_space| is
  // only used for deciding if an NV12 resource should be promoted to a hardware
  // overlay. Instead, we should plumb this information to DRM/KMS so that if
  // the resource does get promoted to overlay, the display controller knows how
  // to perform the YUV-to-RGB conversion.
  gfx::ColorSpace color_space;
  gfx::HDRMetadata hdr_metadata;

  // A gpu resource may be possible to use directly in an overlay if this is
  // true.
  bool is_overlay_candidate = false;

  // Indicates if the resource uses low latency rendering.
  bool is_low_latency_rendering = false;

  // This defines when the display compositor returns resources. Clients may use
  // different synchronization types based on their needs.
  SynchronizationType synchronization_type = SynchronizationType::kSyncToken;

  // YCbCr info for resources backed by YCbCr Vulkan images.
  std::optional<gpu::VulkanYCbCrInfo> ycbcr_info;

#if BUILDFLAG(IS_ANDROID)
  // Indicates whether this resource may be overlaid on Android via legacy
  // overlay flow, since it's backed by a SurfaceView. It's good to find this
  // out in advance, since one has no fallback path for displaying a
  // SurfaceView except via promoting it to an overlay.
  bool is_backed_by_surface_view = false;
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
  // Indicates that this resource would like a promotion hint.
  bool wants_promotion_hint = false;
#endif

  // If true, we need to run a detiling image processor on the quad before we
  // can scan it out.
  bool needs_detiling = false;

  // Origin of the underlying resource.
  GrSurfaceOrigin origin = kTopLeft_GrSurfaceOrigin;

  SkAlphaType alpha_type = kPremul_SkAlphaType;

  // The source that originally allocated this resource. For determining which
  // sources are maintaining lifetime after surface eviction.
  ResourceSource resource_source = ResourceSource::kUnknown;

 private:
  gpu::Mailbox memory_buffer_id_;

  // The SyncToken associated with the above buffer. Allows the receiver to wait
  // until the producer has finished using the texture before it begins using
  // the texture.
  gpu::SyncToken sync_token_;

  // When the shared memory buffer is backed by a GPU texture, the
  // `texture_target` is that texture's type.
  // See here for OpenGL texture types:
  // https://www.opengl.org/wiki/Texture#Texture_Objects
  uint32_t texture_target_ = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_TRANSFERABLE_RESOURCE_H_
