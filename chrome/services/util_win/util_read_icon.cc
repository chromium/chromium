// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/util_read_icon.h"

#include <windows.h>

#include <string>

#include "base/files/file_path.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/display/win/dpi.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image_skia.h"

namespace {
// Provides storage for icon handles.
struct ScopedIconHandles {
 public:
  explicit ScopedIconHandles(size_t n)
      : handles(std::vector<HICON>(n, nullptr)),
        resources(std::vector<UINT>(n, 0)) {}
  ~ScopedIconHandles() {
    for (HICON icon : handles) {
      if (icon) {
        DeleteObject(icon);
      }
    }
  }

  std::vector<HICON> handles;
  std::vector<UINT> resources;
};
}  // namespace

using chrome::mojom::IconSize;

UtilReadIcon::UtilReadIcon(
    mojo::PendingReceiver<chrome::mojom::UtilReadIcon> receiver)
    : receiver_(this, std::move(receiver)) {}

UtilReadIcon::~UtilReadIcon() = default;

// This is exposed to the browser process only. |filename| is a path to
// a downloaded file, such as an |.exe|.
void UtilReadIcon::ReadIcon(const base::FilePath& filename,
                            IconSize icon_size,
                            ReadIconCallback callback) {
  int size = 0;
  // See IconLoader::IconSize.
  switch (icon_size) {
    case IconSize::kSmall:
      size = 16;
      break;
    case IconSize::kNormal:
      size = 32;
      break;
    case IconSize::kLarge:
      size = 48;
      break;
    default:
      NOTREACHED();
  }

  gfx::ImageSkia image_ret;

  // Returns number of icons, or 0 on failure.
  UINT nIcons = PrivateExtractIconsW(filename.value().c_str(), 0, 0, 0, nullptr,
                                     nullptr, 0, 0);

  if (nIcons == 0) {
    std::move(callback).Run(std::move(image_ret), filename.AsUTF16Unsafe());
    return;
  }

  ScopedIconHandles icons(nIcons);
  UINT ret = PrivateExtractIconsW(filename.value().c_str(), 0, size, size,
                                  icons.handles.data(), icons.resources.data(),
                                  nIcons, 0);

  if (ret != nIcons) {
    std::move(callback).Run(std::move(image_ret), filename.AsUTF16Unsafe());
    return;
  }

  // Use icon with lowest resource value, or first if no hints available.
  UINT best = 0xFFFFFFFF;
  HICON selected = icons.handles.at(0);
  for (size_t i = 0; i < icons.handles.size(); i++) {
    if (icons.resources.at(i) < best) {
      best = icons.resources.at(i);
      selected = icons.handles.at(i);
    }
  }

  const SkBitmap bitmap = IconUtil::CreateSkBitmapFromHICON(selected);
  if (!bitmap.isNull()) {
    gfx::ImageSkia image_skia(
        gfx::ImageSkiaRep(bitmap, display::win::GetDPIScale()));
    image_skia.MakeThreadSafe();
    image_ret = std::move(image_skia);
  }

  std::move(callback).Run(std::move(image_ret), filename.AsUTF16Unsafe());
}
