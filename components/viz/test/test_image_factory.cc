// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_image_factory.h"

#include <stddef.h>
#include <utility>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/system/sys_info.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_image_memory.h"

namespace viz {

namespace {

class GLImageSharedMemory : public gl::GLImageMemory {
 public:
  explicit GLImageSharedMemory(const gfx::Size& size)
      : gl::GLImageMemory(size) {}

  GLImageSharedMemory(const GLImageSharedMemory&) = delete;
  GLImageSharedMemory& operator=(const GLImageSharedMemory&) = delete;

  bool Initialize(const base::UnsafeSharedMemoryRegion& region,
                  gfx::GenericSharedMemoryId shared_memory_id,
                  gfx::BufferFormat format,
                  size_t offset,
                  size_t stride) {
    if (!region.IsValid())
      return false;

    if (gfx::NumberOfPlanesForLinearBufferFormat(format) != 1)
      return false;

    base::CheckedNumeric<size_t> checked_size = stride;
    checked_size *= GetSize().height();
    if (!checked_size.IsValid())
      return false;

    // Minimize the amount of address space we use but make sure offset is a
    // multiple of page size as required by MapAt().
    size_t memory_offset = offset % base::SysInfo::VMAllocationGranularity();
    size_t map_offset = base::SysInfo::VMAllocationGranularity() *
                        (offset / base::SysInfo::VMAllocationGranularity());

    checked_size += memory_offset;
    if (!checked_size.IsValid())
      return false;

    auto shared_memory_mapping =
        region.MapAt(static_cast<off_t>(map_offset), checked_size.ValueOrDie());
    if (!shared_memory_mapping.IsValid()) {
      DVLOG(0) << "Failed to map shared memory.";
      return false;
    }

    if (!gl::GLImageMemory::Initialize(
            static_cast<uint8_t*>(shared_memory_mapping.memory()) +
                memory_offset,
            format, stride)) {
      return false;
    }

    DCHECK(!shared_memory_mapping_.IsValid());
    shared_memory_mapping_ = std::move(shared_memory_mapping);
    shared_memory_id_ = shared_memory_id;
    return true;
  }

 private:
  ~GLImageSharedMemory() override = default;

  base::WritableSharedMemoryMapping shared_memory_mapping_;
  gfx::GenericSharedMemoryId shared_memory_id_;
};

}  // namespace

TestImageFactory::TestImageFactory() = default;

TestImageFactory::~TestImageFactory() = default;

scoped_refptr<gl::GLImage> TestImageFactory::CreateImageForGpuMemoryBuffer(
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    const gfx::ColorSpace& color_space,
    gfx::BufferPlane plane,
    int client_id,
    gpu::SurfaceHandle surface_handle) {
  DCHECK_EQ(handle.type, gfx::SHARED_MEMORY_BUFFER);
  auto image = base::MakeRefCounted<GLImageSharedMemory>(size);
  if (!image->Initialize(handle.region, handle.id, format, handle.offset,
                         handle.stride))
    return nullptr;

  return image;
}

}  // namespace viz
