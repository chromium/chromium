// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shared_memory.h"

#include <GLES2/gl2extchromium.h>
#include <stddef.h>

#include <utility>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "components/exo/buffer.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace exo {
namespace {

bool IsSupportedFormat(gfx::BufferFormat format) {
  return format == gfx::BufferFormat::RGBX_8888 ||
         format == gfx::BufferFormat::RGBA_8888 ||
         format == gfx::BufferFormat::BGRX_8888 ||
         format == gfx::BufferFormat::BGRA_8888;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// SharedMemory, public:

SharedMemory::SharedMemory(base::UnsafeSharedMemoryRegion shared_memory_region)
    : shared_memory_region_(std::move(shared_memory_region)) {}

SharedMemory::~SharedMemory() = default;

std::unique_ptr<Buffer> SharedMemory::CreateBuffer(const gfx::Size& size,
                                                   gfx::BufferFormat format,
                                                   unsigned offset,
                                                   uint32_t stride) {
  TRACE_EVENT2("exo", "SharedMemory::CreateBuffer", "size", size.ToString(),
               "format", static_cast<int>(format));

  if (!IsSupportedFormat(format)) {
    DLOG(WARNING) << "Failed to create shm buffer. Unsupported format 0x"
                  << static_cast<int>(format);
    return nullptr;
  }

  if (gfx::RowSizeForBufferFormat(size.width(), format, 0) > stride ||
      stride & 3) {
    DLOG(WARNING) << "Failed to create shm buffer. Unsupported stride "
                  << stride;
    return nullptr;
  }

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.region = shared_memory_region_.Duplicate();
  handle.offset = offset;
  handle.stride = stride;

  const gfx::BufferUsage buffer_usage = gfx::BufferUsage::GPU_READ;

  // COMMANDS_ISSUED queries are sufficient for shared memory
  // buffers as binding to texture is implemented using a call to
  // glTexImage2D and the buffer can be reused as soon as that
  // command has been issued.
  const unsigned query_type = GL_COMMANDS_ISSUED_CHROMIUM;

  // Zero-copy doesn't provide a benefit in the case of shared memory as an
  // implicit copy is required when trying to use these buffers as zero-copy
  // buffers. Making the copy explicit allows the buffer to be reused earlier.
  const bool use_zero_copy = false;
  const bool is_overlay_candidate = false;
  const bool y_invert = false;

    return Buffer::CreateBufferFromGMBHandle(
        std::move(handle), size, format, buffer_usage, query_type,
        use_zero_copy, is_overlay_candidate, y_invert);
}

size_t SharedMemory::GetSize() const {
  return shared_memory_region_.GetSize();
}

bool SharedMemory::Resize(const size_t new_size) {
  // The following code is to replace |shared_memory_region_| with an identical
  // UnsafeSharedMemoryRegion with a new size.
  base::subtle::PlatformSharedMemoryRegion platform_region =
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(shared_memory_region_));
  base::UnguessableToken guid = platform_region.GetGUID();
  base::subtle::PlatformSharedMemoryRegion updated_platform_region =
      base::subtle::PlatformSharedMemoryRegion::Take(
          platform_region.PassPlatformHandle(),
          base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe, new_size,
          guid);
  shared_memory_region_ = base::UnsafeSharedMemoryRegion::Deserialize(
      std::move(updated_platform_region));

  return shared_memory_region_.IsValid();
}

}  // namespace exo
