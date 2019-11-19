// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/layered_window_updater_impl.h"

#include "base/trace_event/trace_event.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace viz {

LayeredWindowUpdaterImpl::LayeredWindowUpdaterImpl(
    HWND hwnd,
    mojo::PendingReceiver<mojom::LayeredWindowUpdater> receiver)
    : hwnd_(hwnd), receiver_(this, std::move(receiver)) {}

LayeredWindowUpdaterImpl::~LayeredWindowUpdaterImpl() = default;

void LayeredWindowUpdaterImpl::OnAllocatedSharedMemory(
    const gfx::Size& pixel_size,
    base::UnsafeSharedMemoryRegion region) {
  canvas_.reset();

  if (!region.IsValid())
    return;

  // Make sure |pixel_size| is sane.
  size_t expected_bytes;
  bool size_result = ResourceSizes::MaybeSizeInBytes(
      pixel_size, ResourceFormat::RGBA_8888, &expected_bytes);
  if (!size_result)
    return;

  // The SkCanvas maps shared memory on creation and unmaps on destruction.
  canvas_ = skia::CreatePlatformCanvasWithSharedSection(
      pixel_size.width(), pixel_size.height(), true, region.GetPlatformHandle(),
      skia::RETURN_NULL_ON_FAILURE);

  // |region|'s handle will close when it goes out of scope.
}

void LayeredWindowUpdaterImpl::Draw(DrawCallback draw_callback) {
  TRACE_EVENT0("viz", "LayeredWindowUpdaterImpl::Draw");

  if (!canvas_) {
    std::move(draw_callback).Run();
    return;
  }

  // Set WS_EX_LAYERED extended window style if not already set.
  DWORD style = GetWindowLong(hwnd_, GWL_EXSTYLE);
  DCHECK(!(style & WS_EX_COMPOSITED));
  if (!(style & WS_EX_LAYERED))
    SetWindowLong(hwnd_, GWL_EXSTYLE, style | WS_EX_LAYERED);

  // Draw to the HWND. If |canvas_| size doesn't match HWND size then it will be
  // clipped or guttered accordingly.
  RECT wr;
  GetWindowRect(hwnd_, &wr);
  SIZE size = {wr.right - wr.left, wr.bottom - wr.top};
  POINT position = {wr.left, wr.top};
  POINT zero = {0, 0};
  BLENDFUNCTION blend = {AC_SRC_OVER, 0x00, 0xFF, AC_SRC_ALPHA};
  HDC dib_dc = skia::GetNativeDrawingContext(canvas_.get());
  UpdateLayeredWindow(hwnd_, nullptr, &position, &size, dib_dc, &zero,
                      RGB(0xFF, 0xFF, 0xFF), &blend, ULW_ALPHA);

  std::move(draw_callback).Run();
}

}  // namespace viz
