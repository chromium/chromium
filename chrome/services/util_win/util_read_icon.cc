// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/util_read_icon.h"

#include <windows.h>

#include <string.h>

#include <utility>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/win/scoped_gdi_object.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image_skia.h"

using chrome::mojom::IconSize;

namespace {

gfx::ImageSkia LoadIcon(base::File file, int size, float scale) {
  base::MemoryMappedFile map;
  if (!map.Initialize(std::move(file),
                      base::MemoryMappedFile::READ_CODE_IMAGE)) {
    return gfx::ImageSkia();
  }

  HMODULE library = reinterpret_cast<HMODULE>(map.data());
  // Find the first icon referenced in the file.  This matches Explorer.
  LPWSTR id = nullptr;
  // Because the lambda below returns FALSE, EnumResourceNames() itself will
  // return FALSE even when it "succeeds", so ignore its return value.
  ::EnumResourceNames(
      library, RT_GROUP_ICON,
      [](HMODULE, LPCWSTR, LPWSTR name, LONG_PTR param) {
        *reinterpret_cast<LPWSTR*>(param) =
            IS_INTRESOURCE(name) ? name : _wcsdup(name);
        return FALSE;
      },
      reinterpret_cast<LONG_PTR>(&id));

  base::win::ScopedHICON icon(static_cast<HICON>(
      ::LoadImage(library, id, IMAGE_ICON, size, size, LR_DEFAULTCOLOR)));
  if (!IS_INTRESOURCE(id))
    free(id);
  if (!icon.is_valid())
    return gfx::ImageSkia();

  const SkBitmap bitmap = IconUtil::CreateSkBitmapFromHICON(icon.get());
  if (bitmap.isNull())
    return gfx::ImageSkia();

  gfx::ImageSkia image_skia(gfx::ImageSkiaRep(bitmap, scale));
  image_skia.MakeThreadSafe();
  return image_skia;
}

}  // namespace

UtilReadIcon::UtilReadIcon(
    mojo::PendingReceiver<chrome::mojom::UtilReadIcon> receiver)
    : receiver_(this, std::move(receiver)) {}

UtilReadIcon::~UtilReadIcon() = default;

// This is exposed to the browser process only. |file| is a handle to
// a downloaded file, such as an |.exe|.
void UtilReadIcon::ReadIcon(base::File file,
                            IconSize icon_size,
                            float scale,
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
      NOTREACHED_IN_MIGRATION();
  }
  std::move(callback).Run(LoadIcon(std::move(file), size * scale, scale));
}
